#ifndef RED_SEARCH_CPPCOMMON_SW_APP_SDK_ELEMENTREGISTRY_H_
#define RED_SEARCH_CPPCOMMON_SW_APP_SDK_ELEMENTREGISTRY_H_

#include "registry.h"
#include <string>
#include <unordered_map>
#include <type_traits>

namespace red_search_cppcommon {

template <typename Element>
class ElementRegistryImpl : public ElementRegistry<Element> {
public:
    typedef typename std::unordered_map<std::string, Element*>::const_iterator const_iterator;
    typedef typename std::unordered_map<std::string, Element*>::iterator       iterator;
    typedef Element element_type;

    void register_element(const char* name, Element* p) override {
        std::string element_name(name);
        _list[element_name] = p;
    }

    Element* find_element(const char* name) override {
        auto iter = _list.find(std::string(name));
        if (_list.end() == iter) {
            return nullptr;
        } else {
            return iter->second;
        }
    }

    const_iterator begin() const {
        return _list.begin();
    }

    const_iterator end() const {
        return _list.end();
    }

    iterator begin() {
        return _list.begin();
    }

    iterator end() {
        return _list.end();
    }

    void visit(std::function<bool(const std::string& name, Element*)> visitor) {
        for (auto item : _list) {
            if (!visitor(item.first, item.second)) {
                break;
            }
        }
    }

    TypeTraitsRegistry& type_traits_registry() override {
        return _type_traits_registry;
    }
 protected:
    std::unordered_map<std::string, Element*> _list;
    TypeTraitsRegistry _type_traits_registry;
};

typedef ElementRegistryImpl<void> AnyElementRegistryImpl;

template<typename C>
class ElementRegisterer {
 public:
    template <typename R>
    ElementRegisterer(R* registry, const char* name) {
        typedef typename std::remove_cv<R>::type::element_type Element;
        static_assert(std::is_same<Element, void>::value
                      || std::is_same<Element, C>::value
                      || std::is_base_of<Element, C>::value, "registering class not match element_type in registry");
        registry->register_element(name, &instance_);
        registry->type_traits_registry().template register_type_traits<C>(name);
    }

private:
    C instance_;
};

};  // namespace red_search_cppcommon

#define REGISTER_ELEMENT(_registry_, _name_, _class_name_)               \
namespace {                                                              \
static ::red_search_cppcommon::ElementRegisterer<_class_name_>           \
        RED_CONCAT(g_registerer_, __COUNTER__)(_registry_, #_name_);        \
}

#endif  // RED_SEARCH_CPPCOMMON_SW_APP_SDK_ELEMENTREGISTRY_H_
