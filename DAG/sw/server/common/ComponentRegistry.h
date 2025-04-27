#ifndef RED_SEARCH_WORKER_SW_COMMON_COMPONENTREGISTRY_H_
#define RED_SEARCH_WORKER_SW_COMMON_COMPONENTREGISTRY_H_

#include "cppcommon/registry/element_registry.h"
#include <string>
#include <unordered_map>

namespace sw {

class Component;

class ComponentRegistry : public red_search_cppcommon::ElementRegistryImpl<Component> {
public:
    typedef red_search_cppcommon::ElementRegistryImpl<Component>::element_type element_type;

    static ComponentRegistry* instance() {
        if (nullptr == _instance) {
            _instance = new (std::nothrow) ComponentRegistry();
        }
        return _instance;
    }

    std::unordered_map<std::string, Component*>& get_inner_map() {
        return _list;// 这个list 都是 继承的   ElementRegistryImpl 这个类中的 std::unordered_map<std::string, Element*> _list; 
    }

 private:
    static ComponentRegistry* _instance;
};

}  // namespace sw

#define REGISTER_COMPONENT(_name_, _class_name_) \
    REGISTER_ELEMENT(sw::ComponentRegistry::instance(), _name_, _class_name_)

#endif  // RED_SEARCH_WORKER_SW_COMMON_COMPONENTREGISTRY_H_
