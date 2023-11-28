#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <typeindex>
#include <memory>
#include <stdexcept>

#include <type_traits>
#include <gtest/gtest.h>



namespace red_search_cppcommon {
class TypeTraitsRegistry {
public:
    template <typename T>
    void register_type_traits(const char *);

    const std::vector<std::type_index> & base_type_index(const char *name) const {
        static std::vector<std::type_index> kEmptyList;
        auto iter = _base_type_mapping.find(name);
        return iter == _base_type_mapping.end() ? kEmptyList : iter->second;
    }

    // return value is mutable
    const std::type_index* type_index(const char * name) const {
        auto iter = _mapping.find(name);
        return iter == _mapping.end() ? nullptr : &iter->second;
    }

private:
    std::unordered_map<std::string, std::type_index> _mapping;
    std::unordered_map<std::string, std::vector<std::type_index>> _base_type_mapping;
};

template <typename Element>
class FactoryRegistry {
public:
    typedef Element element_type;
    
    typedef std::function<Element*()> creator;

    typedef std::function<void(Element *)> destroyer;

    virtual ~FactoryRegistry() {}

    virtual void register_factory(const char* name, creator ctor, destroyer dtor) = 0;

    virtual Element* create_element(const char* name) = 0;

    virtual void destroy_element(const char* name, Element* p) = 0;

    virtual bool contain(const char *name) = 0;

    virtual std::unordered_set<std::string> get_factory_names() const = 0;

    virtual TypeTraitsRegistry& type_traits_registry() = 0;
};

typedef FactoryRegistry<void> AnyFactoryRegistry;

template <typename Element>
class ElementRegistry {
public:
    typedef Element element_type;

    virtual ~ElementRegistry() {}

    virtual void register_element(const char* name, Element* p) = 0;

    virtual Element* find_element(const char* name) = 0;

    virtual void visit(std::function<bool(const std::string& name, Element*)> visitor) = 0;

    virtual TypeTraitsRegistry& type_traits_registry() = 0;
};

typedef ElementRegistry<void> AnyElementRegistry;
}  // namespace red_search_cppcommon


namespace red_search_cppcommon {
template <typename T, typename std::enable_if<T::empty::value, int>::type = 0>
void iterate_bases(std::vector<std::type_index> &, T *) {}

template <typename T, typename std::enable_if<!T::empty::value, int>::type = 0>
void iterate_bases(std::vector<std::type_index> &v, T *) {
    typedef typename T::first::type Base;
    v.push_back(std::type_index(typeid(Base)));
    typename T::rest::type * dummy = nullptr;
    iterate_bases(v, dummy);
}

template <typename T>
void TypeTraitsRegistry::register_type_traits(const char * name) {
    std::vector<std::type_index> base_types;
    _mapping.emplace(std::string(name), std::type_index(typeid(T)));
    _base_type_mapping.emplace(std::string(name), base_types);
}
}  // namespace red_search_cppcommon



namespace red_search_cppcommon {

    template <typename Element>
    class FactoryRegistryImpl : public FactoryRegistry<Element> {
    public:
        typedef typename FactoryRegistry<Element>::creator   creator;
        typedef typename FactoryRegistry<Element>::destroyer destroyer;
        typedef Element element_type;

        virtual void register_factory(const char* name, creator ctor, destroyer dtor) {
            std::string factory_name(name);
            _creator_list[factory_name]   = ctor;
            _destroyer_list[factory_name] = dtor;
        }

        virtual Element* create_element(const char* name) {
            auto iter = _creator_list.find(std::string(name));
            if (_creator_list.end() == iter) {
                fprintf(stderr, "creator not found for [%s]\n", nullptr == name ? "null" : name);
                return nullptr;
            } else {
                return (iter->second)();
            }
        }

        virtual void destroy_element(const char* name, Element* p) {
            auto iter = _destroyer_list.find(std::string(name));
            if (_destroyer_list.end() == iter) {
                fprintf(stderr, "destroyer not found for [%s, %p]\n", nullptr == name ? "null" : name,
                        p);
            } else {
                (iter->second)(p);
            }
        }

        virtual bool contain(const char *name) {
            return _creator_list.find(std::string(name)) != _creator_list.end();
        }

        virtual std::unordered_set<std::string> get_factory_names() const {
            std::unordered_set<std::string> keys;
            for (auto iter : _creator_list) {
                keys.insert(iter.first);
            }

            return keys;
        }

        TypeTraitsRegistry& type_traits_registry() override {
            return _type_traits_registry;
        }

    private:
        std::unordered_map<std::string, creator>   _creator_list;
        std::unordered_map<std::string, destroyer> _destroyer_list;
        TypeTraitsRegistry _type_traits_registry;
    };

    typedef FactoryRegistryImpl<void> AnyFactoryRegistryImpl;

    template <class T, class Base>
    struct TypeTraits {
        static Base* create_object() {
            return new (std::nothrow) T();
        }

        static void destroy_object(Base* p) {
            if (nullptr != p) {
                delete static_cast<T*>(p);
            }
        }
    };

    template <typename C>
    class FactoryRegisterer {
    public:
        /**
         *
         * @tparam R concrete type of registry
         * @tparam C concrete type of class which is about to register
         * @param registry
         * @param name
         * @param dummy
         */
        template <typename R>
        FactoryRegisterer(R* registry, const char* name) {
            typedef typename std::remove_cv<R>::type::element_type Element;
            static_assert(std::is_same<Element, void>::value
                        || std::is_same<Element, C>::value
                        || std::is_base_of<Element, C>::value, "registering class not match element_type in registry");

            registry->register_factory(name, &TypeTraits<C, Element>::create_object, &TypeTraits<C, Element>::destroy_object);
            registry->type_traits_registry().template register_type_traits<C>(name);
        }
    };

}  // namespace red_search_cppcommon

#define REGISTER_FACTORY(_registry_, _name_, _class_name_)          \
namespace {                                                         \
static ::red_search_cppcommon::FactoryRegisterer<_class_name_>      \
        RED_CONCAT(g_registerer_, __COUNTER__)(_registry_, #_name_);   \
}

#ifndef RED_CONCAT
# define RED_CONCAT(a, b) RED_CONCAT_HELPER(a, b)
# define RED_CONCAT_HELPER(a, b) a##b
#endif  // RED_CONCAT

namespace red_search_cppcommon {

class ClassA {};

class ClassB : public ClassA {};

class ClassC : public ClassA {};

class ClassD {};

typedef FactoryRegistryImpl<ClassA> AFactoryRegistryImpl;

AnyFactoryRegistryImpl g_any_factory_registry;

AFactoryRegistryImpl g_a_factory_registry;

/*
TEST(FactoryRegistryTest, test) {
    ASSERT_TRUE(g_a_factory_registry.create_element("class_b") != nullptr);
    ASSERT_TRUE(g_a_factory_registry.create_element("class_c") != nullptr);
    ASSERT_TRUE(g_any_factory_registry.create_element("class_d") != nullptr);
    ASSERT_EQ(typeid(ClassD).hash_code(), g_any_factory_registry.type_traits_registry().type_index("class_d")->hash_code());
}*/

}  // namespace red_search_cppcommon

REGISTER_FACTORY(&red_search_cppcommon::g_a_factory_registry, class_b, red_search_cppcommon::ClassB);
REGISTER_FACTORY(&red_search_cppcommon::g_a_factory_registry, class_c, red_search_cppcommon::ClassC);
REGISTER_FACTORY(&red_search_cppcommon::g_any_factory_registry, class_d, red_search_cppcommon::ClassD);



int main(){
    std::cout<<" xxxx "<< red_search_cppcommon::g_a_factory_registry.create_element("class_b") <<std::endl;

    /*
    REGISTER_FACTORY(&red_search_cppcommon::g_a_factory_registry, class_b, red_search_cppcommon::ClassB);
    展开
    namespace {
        static ::red_search_cppcommon::FactoryRegisterer<red_search_cppcommon::ClassB>
            RED_CONCAT(g_registerer_, __COUNTER__)(&red_search_cppcommon::g_a_factory_registry, "class_b");
    }

        RED_CONCAT(g_registerer_, __COUNTER__)(&red_search_cppcommon::g_a_factory_registry, "class_b")
        继续展开
        static ::red_search_cppcommon::FactoryRegisterer<red_search_cppcommon::ClassB>
        g_registerer_0(&red_search_cppcommon::g_a_factory_registry, "class_b");// 这里调用 FactoryRegisterer 构造函数.......

        看上面展开的就是一个对象  这里 g_registerer_0 就是 FactoryRegisterer<red_search_cppcommon::ClassB> 类型的静态对象的名称 
    */

    return 1;
}
