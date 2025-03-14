/*


// registry_demo.cpp
#include <iostream>
#include <cassert>
#include <string>

// 启用C++17特性（解决if初始化语句警告）
#define __RED_ENABLE_TR2__  // 如果需要tr2支持可取消注释

#include "registry.h"
#include "factory_registry.h"
#include "element_registry.h"

using namespace red_search_cppcommon;

// ==================== 工厂注册测试 ====================
class Shape {
public:
    virtual ~Shape() = default;
    virtual double area() const = 0;
    virtual void scale(double factor) = 0;
};

class Circle : public Shape {
public:
    Circle() = default;
    explicit Circle(double r) : radius(r) {}
    
    double area() const override { 
        return 3.14159 * radius * radius; 
    }
    
    void scale(double factor) override { 
        radius *= factor; 
    }
private:
    double radius = 1.0;
};

class Square : public Shape {
public:
    Square() = default;
    explicit Square(double s) : side(s) {}
    
    double area() const override { 
        return side * side; 
    }
    
    void scale(double factor) override { 
        side *= factor; 
    }
private:
    double side = 1.0;
};

// ==================== 元素注册测试 ====================
class AppConfig {
public:
    std::string theme = "light";
    int fontSize = 12;
};

// 1. 创建形状工厂 全局注册
FactoryRegistryImpl<Shape> g_shapeFactory;
// 2. 注册具体类型
REGISTER_FACTORY(&g_shapeFactory, Circle, Circle);
REGISTER_FACTORY(&g_shapeFactory, Square, Square);

// 1. 创建配置注册器 全局注册
ElementRegistryImpl<AppConfig> g_configRegistry;
// 2. 注册配置实例
REGISTER_ELEMENT(&g_configRegistry, GlobalConfig, AppConfig);


void test_factory_registry() {


    // 3. 创建对象
    Shape* circle = g_shapeFactory.create_element("Circle");
    Shape* square = g_shapeFactory.create_element("Square");

    // 验证基础功能
    assert(circle != nullptr);
    assert(square != nullptr);
    std::cout << "Circle initial area: " << circle->area() << std::endl;
    std::cout << "Square initial area: " << square->area() << std::endl;

    // 4. 修改对象状态
    circle->scale(2.0);
    square->scale(3.0);
    assert(std::abs(circle->area() - 12.56636) < 0.001);
    assert(square->area() == 9.0);

    // 5. 正确销毁对象
    g_shapeFactory.destroy_element("Circle", circle);
    g_shapeFactory.destroy_element("Square", square);
}



void test_element_registry() {

    // 3. 获取配置实例
    AppConfig* cfg = g_configRegistry.find_element("GlobalConfig");
    assert(cfg != nullptr);
    
    // 4. 修改配置
    cfg->theme = "dark";
    cfg->fontSize = 14;

    // 5. 验证单例特性
    AppConfig* sameCfg = g_configRegistry.find_element("GlobalConfig");
    std::cout << "Theme: " << sameCfg->theme 
              << ", FontSize: " << sameCfg->fontSize << std::endl;
    assert(sameCfg->theme == "dark");
    assert(sameCfg->fontSize == 14);
}

int main() {
    std::cout << "=== Factory Registry Test ===" << std::endl;
    test_factory_registry();
    
    std::cout << "\n=== Element Registry Test ===" << std::endl;
    test_element_registry();

    std::cout << "\nAll tests passed!" << std::endl;
    return 0;
}


*/


#include <iostream>
#include <cassert>
#include <typeinfo>
#include <cmath>

// 注册系统头文件
#include "factory_registry.h"
#include "element_registry.h"

using namespace red_search_cppcommon;

/********************** 测试类定义 ​**********************/
// 基类必须首先定义
class Animal {
public:
    virtual ~Animal() = default;
    virtual std::string sound() = 0;
};

class Dog : public Animal {
public:
    Dog(){
        std::cout<<" Dog "<<std::endl;
    }
    std::string sound() override { return "Woof!"; }
};

class Cat : public Animal {
public:
    std::string sound() override { return "Meow~"; }
};

// 不相关的类用于测试类型系统
class Car {
public:
    std::string noise() { return "Vroom!"; }
};

// 配置类用于元素注册测试
class AppConfig {
public:
    AppConfig(){
        std::cout<<" AppConfig "<<std::endl;
    }
    std::string theme = "light";
    int fontSize = 12;
};

/********************** 全局注册点 ​**********************/
// 动物工厂（限定Animal基类）
FactoryRegistryImpl<Animal> g_animal_factory;

// 通用工厂（允许任意类型）
FactoryRegistryImpl<void> g_any_factory;

// 配置注册器
ElementRegistryImpl<AppConfig> g_config_registry;

/********************** 注册操作 ​**********************/
REGISTER_FACTORY(&g_animal_factory, dog, Dog);    // 合法注册
REGISTER_FACTORY(&g_animal_factory, cat, Cat);
REGISTER_FACTORY(&g_any_factory, car, Car);      // 通用工厂注册任意类型
REGISTER_ELEMENT(&g_config_registry, global, AppConfig); // 元素注册

// 以下注册如果取消注释会编译失败（类型不匹配）
// REGISTER_FACTORY(&g_animal_factory, invalid, Car);

/********************** 测试函数 ​**********************/
void test_factory_system() {
    std::cout << "=== 开始工厂系统测试 ===" << std::endl;
    
    // 测试1：基础对象创建
    Animal* dog = g_animal_factory.create_element("dog");//这里会创建对象
    Animal* cat = g_animal_factory.create_element("cat");
    assert(dog != nullptr && cat != nullptr);
    std::cout << "✓ 对象创建测试通过" << std::endl;

    // 测试2：多态行为验证
    assert(dog->sound() == "Woof!");
    assert(cat->sound() == "Meow~");
    std::cout << "✓ 多态行为测试通过" << std::endl;

    // 测试3：通用工厂功能
    void* car = g_any_factory.create_element("car");
    assert(car != nullptr);
    assert(static_cast<Car*>(car)->noise() == "Vroom!");
    std::cout << "✓ 通用工厂测试通过" << std::endl;

    // 清理资源
    g_animal_factory.destroy_element("dog", dog);
    g_animal_factory.destroy_element("cat", cat);
    g_any_factory.destroy_element("car", car);
}

void test_element_registry() {
    std::cout << "\n=== 开始元素注册测试 ===" << std::endl;
    
    // 测试1：单例访问
    AppConfig* cfg1 = g_config_registry.find_element("global");
    assert(cfg1 != nullptr);
    std::cout << "✓ 单例访问测试通过" << std::endl;

    // 测试2：数据持久性
    cfg1->theme = "dark";
    cfg1->fontSize = 14;
    
    AppConfig* cfg2 = g_config_registry.find_element("global");
    assert(cfg2->theme == "dark");
    assert(cfg2->fontSize == 14);
    std::cout << "✓ 数据持久性测试通过" << std::endl;
}

void test_type_system() {
    std::cout << "\n=== 开始类型系统测试 ===" << std::endl;
    
    // 验证类型注册信息
    const auto& animal_types = g_animal_factory.type_traits_registry();
    const auto* dog_type = animal_types.type_index("dog");
    assert(dog_type != nullptr);
    assert(*dog_type == typeid(Dog));
    std::cout << "✓ 类型注册验证通过" << std::endl;

    // 通用工厂类型检查
    const auto& any_types = g_any_factory.type_traits_registry();
    assert(any_types.type_index("car")->hash_code() == typeid(Car).hash_code());
    std::cout << "✓ 类型哈希一致性验证通过" << std::endl;
}

void test_error_handling() {
    std::cout << "\n=== 开始错误处理测试 ===" << std::endl;
    
    // 测试1：无效工厂名称
    Animal* invalid = g_animal_factory.create_element("dinosaur");
    assert(invalid == nullptr);
    std::cout << "✓ 无效名称处理测试通过" << std::endl;

    // 测试2：空指针安全
    g_animal_factory.destroy_element("ghost", nullptr); // 不应崩溃
    std::cout << "✓ 空指针安全测试通过" << std::endl;
}

int main() {
    std::cout<<" staring "<<std::endl;
    test_factory_system();
    test_element_registry();
    test_type_system();
    test_error_handling();

    std::cout << "\n=== 所有测试通过 ===" << std::endl;
    return 0;
}

