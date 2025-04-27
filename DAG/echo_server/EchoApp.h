#ifndef RED_SEARCH_WORKER_SW_APP_DEMO_ECHOAPP_H_
#define RED_SEARCH_WORKER_SW_APP_DEMO_ECHOAPP_H_

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

//#include "ConfigComponent.h"
//#include "MonitorComponent.h"
#include "SwApp.h"
#include "ModuleRegistry.h"
//#include "app_sdk/logger.h"
#include "EchoService.pb.h"

namespace sw {
namespace example {

class EchoApp : public SwApp {
public:
    EchoApp() {
        _topology.push_back({"echo_module", "echo_module", {}});//这个图在开始的时候就自己在 所依赖的 app 就拼dag 图了.......
    }

    bool init(std::unordered_map<std::string, Component*>* components) override {
        if (nullptr == components) {
            APP_FORMAT_LOG(ERROR, "EchoApp::init components is null");
            return false;
        }

        auto iter = components->find("log_component");
        if (components->end() != iter) {
            LoggerHolder::instance()->set_logger(dynamic_cast<LogComponent*>(iter->second));
            APP_LOG(TRACE) << "log_component is " << iter->second;
        }

        iter = components->find("rpc_component");// 关注这个 主件就行了 
        if (components->end() == iter) {
            APP_FORMAT_LOG(ERROR, "rpc_component not found");
            return false;
        }

        RpcComponent* rpc_component = dynamic_cast<RpcComponent*>(iter->second);
        if (!rpc_component->add_topology("default", _topology)) {
            APP_FORMAT_LOG(ERROR, "EchoApp::init add_topology failed");
            return false;
        }
        if (!rpc_component->add_serving("sw.example.proto.EchoService", "echo", "default", nullptr)) {//这个屌用有点意思.......
            APP_FORMAT_LOG(ERROR, "EchoApp::init add_serving failed");
            return false;
        }
        return true;
    }

    DataFactory* session_local_data_factory() override {
        return nullptr;
    }

    // red_search_cppcommon::AnyFactoryRegistry* get_runnable_module_factory_registry() override {
    red_search_cppcommon::FactoryRegistry<sw::RunnableModule>* get_runnable_module_factory_registry()
        override {
        return &sw::ModuleRegistry::instance()->get_runnable_module_factory_registry();
    }

    // red_search_cppcommon::AnyElementRegistry* get_reloadable_module_registry() override {
    red_search_cppcommon::ElementRegistry<sw::ReloadableModule>* get_reloadable_module_registry() override {
        return &sw::ModuleRegistry::instance()->get_reloadable_module_registry();
    }

private:
    std::vector<RunnableElement> _topology;
};

}  // namespace example
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_DEMO_ECHOAPP_H_
