/*
 * 注册系统核心头文件
 * 提供类型特征注册、工厂模式、元素管理的基础设施
 * 编译要求：C++11及以上标准
 */

#ifndef RED_SEARCH_CPPCOMMON_REGISTRY_REGISTRY_H_
#define RED_SEARCH_CPPCOMMON_REGISTRY_REGISTRY_H_

#include <string>             // 字符串处理
#include <unordered_map>      // 哈希表容器
#include <unordered_set>      // 哈希集合
#include <vector>             // 动态数组
#include <functional>         // 函数对象
#include <typeindex>          // 类型索引支持
#include <memory>             // 智能指针
#include <stdexcept>          // 异常处理

namespace red_search_cppcommon {

/* 类型特征注册表
 * 功能：
 * 1. 维护类型名称与类型信息的映射
 * 2. 记录类型的继承关系（需TR2支持）
 */
class TypeTraitsRegistry {
public:
    // 注册类型特征模板方法
    template <typename T>
    void register_type_traits(const char *);

    // 获取类型的基类列表（需要启用TR2）
    const std::vector<std::type_index> & base_type_index(const char *name) const {
#ifndef __RED_ENABLE_TR2__
        throw std::runtime_error("TR2 support required"); // TR2未启用时抛出异常
#endif
        static std::vector<std::type_index> kEmptyList; // 空列表占位符
        auto iter = _base_type_mapping.find(name);
        return iter == _base_type_mapping.end() ? kEmptyList : iter->second;
    }

    // 通过类型名称查找类型索引
    const std::type_index* type_index(const char * name) const {
        auto iter = _mapping.find(name);
        return iter == _mapping.end() ? nullptr : &iter->second;
    }

private:
    std::unordered_map<std::string, std::type_index> _mapping; // 类型名称->类型索引
    std::unordered_map<std::string, std::vector<std::type_index>> _base_type_mapping; // 继承关系存储
};

/* 工厂注册系统抽象接口
 * @tparam Element 工厂管理的基类类型
 * 功能：
 * 1. 管理具体类的创建/销毁方法
 * 2. 维护类型特征信息
 */
template <typename Element>
class FactoryRegistry {
public:
    // 类型别名
    typedef Element element_type;          // 基础元素类型
    typedef std::function<Element*()> creator;    // 对象创建函数类型
    typedef std::function<void(Element *)> destroyer; // 对象销毁函数类型

    virtual ~FactoryRegistry() {}

    // 核心接口方法
    virtual void register_factory(const char* name, creator ctor, destroyer dtor) = 0; // 注册工厂
    virtual Element* create_element(const char* name) = 0;         // 创建实例
    virtual void destroy_element(const char* name, Element* p) = 0; // 销毁实例
    virtual bool contain(const char *name) = 0;                     // 存在性检查
    virtual std::unordered_set<std::string> get_factory_names() const = 0; // 获取注册列表
    virtual TypeTraitsRegistry& type_traits_registry() = 0;         // 获取类型注册表
};

// 通用工厂类型别名（void特化版本）
typedef FactoryRegistry<void> AnyFactoryRegistry;

/* 元素注册系统抽象接口
 * @tparam Element 管理的元素类型
 * 功能：
 * 1. 管理已实例化对象的生命周期
 * 2. 提供全局访问点
 */
template <typename Element>
class ElementRegistry {
public:
    typedef Element element_type;  // 元素类型

    virtual ~ElementRegistry() {}

    // 核心接口方法
    virtual void register_element(const char* name, Element* p) = 0; // 注册元素实例
    virtual Element* find_element(const char* name) = 0;            // 查找元素
    virtual void visit(std::function<bool(const std::string& name, Element*)> visitor) = 0; // 遍历元素
    virtual TypeTraitsRegistry& type_traits_registry() = 0;         // 获取类型注册表
};

// 通用元素管理别名（void特化版本）
typedef ElementRegistry<void> AnyElementRegistry;

}  // namespace red_search_cppcommon

/* 宏定义：标识符连接器
 * 功能：安全生成唯一标识符，防止宏展开问题
 */
#ifndef RED_CONCAT
# define RED_CONCAT(a, b) RED_CONCAT_HELPER(a, b)  // 二级展开防止粘连
# define RED_CONCAT_HELPER(a, b) a##b
#endif

// 包含模板实现（分离式编译）
#include "registry_inl.h"

#endif  // 头文件保护结束
