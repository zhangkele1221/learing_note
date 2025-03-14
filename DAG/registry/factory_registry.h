/*
 * 工厂注册系统实现
 * 提供类型安全的工厂模式实现，支持动态对象创建和销毁
 * 包含类型特征注册机制，用于记录类型关系信息
 */

#ifndef RED_SEARCH_CPPCOMMON_SW_APP_SDK_FACTORYREGISTRY_H_
#define RED_SEARCH_CPPCOMMON_SW_APP_SDK_FACTORYREGISTRY_H_

#include "registry.h"          // 基础注册表定义
#include <string>
#include <unordered_map>
#include <type_traits>

namespace red_search_cppcommon {

/* 工厂注册实现类
 * @tparam Element 工厂管理的基类类型
 * 功能：
 * 1. 注册/注销具体类的构造方法
 * 2. 创建/销毁具体类实例
 * 3. 维护类型特征信息
 */
template <typename Element>
class FactoryRegistryImpl : public FactoryRegistry<Element> {
 public:
    // 类型别名定义
    typedef typename FactoryRegistry<Element>::creator   creator;    // 对象创建函数类型
    typedef typename FactoryRegistry<Element>::destroyer destroyer;  // 对象销毁函数类型
    typedef Element element_type;  // 管理的基类类型

    // 注册具体类的工厂方法
    virtual void register_factory(const char* name, creator ctor, destroyer dtor) {
        std::string factory_name(name);
        _creator_list[factory_name]   = ctor;    // 存储构造器
        _destroyer_list[factory_name] = dtor;    // 存储析构器
    }

    // 创建具体类实例
    virtual Element* create_element(const char* name) {
        auto iter = _creator_list.find(std::string(name));
        if (_creator_list.end() == iter) {
            fprintf(stderr, "Error: Creator not found for [%s]\n", name ? name : "null");
            return nullptr;
        }
        return (iter->second)();  // 执行注册的构造函数
    }

    // 销毁对象实例
    virtual void destroy_element(const char* name, Element* p) {
        auto iter = _destroyer_list.find(std::string(name));
        if (_destroyer_list.end() == iter) {
            fprintf(stderr, "Error: Destroyer not found for [%s, %p]\n", 
                    name ? name : "null", p);
        } else {
            (iter->second)(p);  // 执行注册的析构函数
        }
    }

    // 检查是否存在指定工厂
    virtual bool contain(const char *name) {
        return _creator_list.find(std::string(name)) != _creator_list.end();
    }

    // 获取所有注册的工厂名称
    virtual std::unordered_set<std::string> get_factory_names() const {
        std::unordered_set<std::string> keys;
        for (auto& pair : _creator_list) {
            keys.insert(pair.first);
        }
        return keys;
    }

    // 获取类型特征注册表引用
    TypeTraitsRegistry& type_traits_registry() override {
        return _type_traits_registry;
    }

 private:
    std::unordered_map<std::string, creator>   _creator_list;    // 构造器映射表
    std::unordered_map<std::string, destroyer> _destroyer_list;  // 析构器映射表
    TypeTraitsRegistry _type_traits_registry;  // 类型特征注册表
};

// 通用工厂类型别名（可创建任意类型）
typedef FactoryRegistryImpl<void> AnyFactoryRegistryImpl;

/* 类型特征模板
 * 提供标准化的对象创建/销毁方法
 * @tparam T    具体类类型
 * @tparam Base 基类类型
 */
template <class T, class Base>
struct TypeTraits {
    // 创建对象实例（带异常安全）
    static Base* create_object() {
        return new (std::nothrow) T();  // 使用nothrow避免异常
    }

    // 安全销毁对象
    static void destroy_object(Base* p) {
        if (p) {
            delete static_cast<T*>(p);  // 正确派生类指针转换
        }
    }
};

/* 工厂注册辅助类
 * 用于自动注册类型到工厂系统
 * @tparam C 需要注册的具体类
 * 使用示例：
 * FactoryRegisterer<MyClass> reg(&factory, "my_class");
 */
template <typename C>
class FactoryRegisterer {
 public:
    // 注册构造函数
    template <typename R>
    FactoryRegisterer(R* registry, const char* name) {//g_animal_factory  dog 
        // 提取工厂管理的元素类型
        typedef typename std::remove_cv<R>::type::element_type Element;
        
        // 类型安全验证（满足以下条件之一）：
        // 1. 工厂接受任意类型(void)
        // 2. 注册类与工厂元素类型相同
        // 3. 注册类继承自工厂元素类型
        //在编译时验证 Dog 是否满足工厂类型要求 假设 g_animal_factory 的 element_type 是 Animal，则验证 Dog 必须继承自 Animal
        static_assert(std::is_same<Element, void>::value || // 允许通用工厂
                      std::is_same<Element, C>::value ||  // 精确匹配
                      std::is_base_of<Element, C>::value, // 继承关系
                      "Type mismatch: Registered class must inherit from factory element type");

        // 注册创建/销毁方法到工厂
        registry->register_factory(name, 
            &TypeTraits<C, Element>::create_object,
            &TypeTraits<C, Element>::destroy_object);
        
        
        
        /* 展开 
        g_animal_factory->register_factory(
            "dog",
            &TypeTraits<Dog, Animal>::create_object,  // 创建函数 就是上面的抽象的构造函数 return new (std::nothrow) T();  // 使用nothrow避免异常
            &TypeTraits<Dog, Animal>::destroy_object // 销毁函数 就是上面的抽象的析构函数 delete static_cast<T*>(p); 
        );
        */


        // 记录类型特征信息
        registry->type_traits_registry().template register_type_traits<C>(name);
    }
};

}  // namespace red_search_cppcommon

/* 工厂注册宏
 * 简化注册操作的语法糖
 * 参数：
 * _registry_ 工厂实例指针
 * _name_     注册名称（字符串）
 * _class_name_ 具体类类型
 * 示例：
 * REGISTER_FACTORY(&factory, "dog", Dog);
 */
#define REGISTER_FACTORY(_registry_, _name_, _class_name_)          \
namespace { /* 匿名命名空间避免符号冲突 */                          \
static ::red_search_cppcommon::FactoryRegisterer<_class_name_>      \
        RED_CONCAT(g_registerer_, __COUNTER__)(_registry_, #_name_); \
}


//REGISTER_FACTORY(&g_animal_factory, dog, Dog);

// 展开过程 
// namespace {
//  static ::red_search_cppcommon::FactoryRegisterer<Dog>
//        RED_CONCAT(g_registerer_, __COUNTER__)(&g_animal_factory, "dog");
// }

/*

#ifndef RED_CONCAT
# define RED_CONCAT(a, b) RED_CONCAT_HELPER(a, b)  // 二级展开防止粘连
# define RED_CONCAT_HELPER(a, b) a##b
#endif

假设 假设当前__COUNTER__值为123 

RED_CONCAT(g_registerer_, 123)
→ RED_CONCAT_HELPER(g_registerer_, 123)
→ g_registerer_##123
→ g_registerer_123

最终变成 这样一种 static 类型对象的定义了 

 static ::red_search_cppcommon::FactoryRegisterer<Dog> g_registerer_123(&g_animal_factory,// 目标工厂地址 "dog"// 注册名称字符串);

    程序启动时 : 静态对象 g_registerer_123 自动构造，完成工厂注册
    对象创建时 : g_animal_factory.create_element("dog") 返回 new Dog()
    程序退出时 : 静态对象自动销毁（无显式资源需要释放）

     
    其实就是对应的下面类构造函数
    // FactoryRegisterer构造函数伪代码
    template <typename R>
    FactoryRegisterer(R* registry, const char* name) {
        registry->register_factory(name, create_fn, destroy_fn);
        registry->type_traits_registry().register_type<C>(name);
    }

    在静态变量初始化时自动执行构造函数
    向工厂注册创建/销毁方法
    记录类型特征信息


    总结
    该宏通过巧妙的预处理器操作，在编译期完成了:
    唯一标识符生成 - 避免符号冲突
    类型安全验证 - 编译时静态断言
    自动化注册 - 零运行时开销的初始化
    最终实现了类型安全的工厂注册机制，同时保持代码简洁性。

*/



#endif  // 头文件保护结束
