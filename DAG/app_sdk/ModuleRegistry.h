#ifndef RED_SEARCH_WORKER_SW_APP_SDK_MODULEREGISTRY_H_
#define RED_SEARCH_WORKER_SW_APP_SDK_MODULEREGISTRY_H_

#include "app_interface/ReloadableModule.h"
#include "app_interface/RunnableModule.h"
#include "registry/element_registry.h"
#include "registry/factory_registry.h"

namespace sw {
class ModuleRegistry {
public:
    // non-thread-safe
    static ModuleRegistry* instance() {
        if (nullptr == _instance) {
            _instance = new ModuleRegistry();
        }

        return _instance;
    }

    red_search_cppcommon::FactoryRegistry<RunnableModule>& get_runnable_module_factory_registry() {
        return _runnable_module_factory_registry;
    }

    red_search_cppcommon::ElementRegistry<ReloadableModule>& get_reloadable_module_registry() {
        return _reloadable_module_registry;
    }

private:
    red_search_cppcommon::FactoryRegistryImpl<RunnableModule> _runnable_module_factory_registry;
    red_search_cppcommon::ElementRegistryImpl<ReloadableModule> _reloadable_module_registry;
    static ModuleRegistry* _instance;
};
}  // namespace sw

// 这就是对测试例子 REGISTER_FACTORY 定义的额  FactoryRegistryImpl<Animal> g_animal_factory; 包装了一个宏函数 

#define REGISTER_RUNNABLE_MODULE(_name_, _class_)                                                           \
    REGISTER_FACTORY(&sw::ModuleRegistry::instance()->get_runnable_module_factory_registry(), _name_,       \
                     _class_)

#define REGISTER_RELOADABLE_MODULE(_name_, _class_)                                                         \
    REGISTER_ELEMENT(&sw::ModuleRegistry::instance()->get_reloadable_module_registry(), _name_, _class_)

#endif  // RED_SEARCH_WORKER_SW_APP_SDK_MODULEREGISTRY_H_
