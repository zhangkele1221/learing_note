#include "AppProxy.h"
#include "brpc/adaptive_protocol_type.h"
#include "butil/file_util.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/service.h"
#include "google/protobuf/util/json_util.h"
//#include "sw/proto/rpc_call_config.pb.h"
//#include "sw/proto/topology_config.pb.h"
#include "ModuleTopologyManager.h"
//#include "sw/server/SessionLocalData.h"
#include "RpcComponentImpl.h"
#include "Log.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <gflags/gflags.h>

DEFINE_int64(module_reload_interval, 5000, "reload interval(ms) for reloadable_module");

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, AppProxy);

AppProxy::AppProxy(SwApp* app) : _app(app) {
    _rpc_component = nullptr;
}

AppProxy::~AppProxy() {}

bool AppProxy::init(std::unordered_map<std::string, Component*>* components) {
    std::cout<<" AppProxy::init "<<std::endl;
    if (!_app || !components) {
        return false;
    }
    auto iter = components->find("rpc_component");
    if (iter == components->end()) {
        TERR("rpc_component is nullptr");
        return false;
    }
    _rpc_component = dynamic_cast<RpcComponentImpl*>(iter->second);
    if (!_rpc_component) {
        TERR("rpc_component is nullptr");
        return false;
    }
    _rpc_component->set_app(_app);
    if (!_app->init(components)) {// 这里调用的  SwApp 继承类 EchoApp 的 init ......................
        TERR("init SwApp failed");
        return false;
    }

    for (const auto& s : _rpc_component->rpc_serving_configs()) {//rpc_component->add_serving 做了一些 configs 的初始化.....
        // check (default) topology existence
        if (!_rpc_component->module_topology_manager()->contain(s.topology_name)) {
            TERR("default topology[%s] of method[%s.%s] not found", s.topology_name.c_str(),
                    s.service_descriptor->full_name().c_str(), s.method_name.c_str());
            return false;
        }
    }

    for (const auto& s : _rpc_component->raw_services()) {
        auto * integrated = dynamic_cast<IntegratingWithSw *>(s.first);
        if (integrated && !integrated->init(_app)) {
            TERR("init IntegratingWithSw[%s] with SwApp failed",
                 s.first->GetDescriptor()->full_name().c_str());
            return false;
        }
    }

    _rpc_component->set_app_initialized();// 告诉拓扑管理类  _rpc_component 初始化完成
    return true;
}

bool AppProxy::setup_reload_scheme() {
    // do this after app.init but before RunnableModule construct,
    // since ReloadableModule might depend on app.init and RunnableModule might depend on ReloadableModule.
    //auto* registry = _app->get_reloadable_module_registry();

    bool        succ = true;
    std::string failed_module_name;

    /*
    作用：遍历所有注册的 ReloadableModule 调用其 init 方法。
    关键点：
    任一模块初始化失败则立即停止遍历。
    succ 变量控制流程，避免后续无效操作。
    */
   // TimerModule 目前 searcherwork 框架中只有 这个一个 module 是这个  ReloadableModule
    /*
    registry->visit([this, &succ, &failed_module_name](const std::string& name, ReloadableModule* module) {
        if (succ && !module->init(_app)) {
            failed_module_name = name;
            succ               = false;
            return false;
        }
        return true;
    });
    */

    if (!succ) {
        TERR("init ReloadableModule[%s] failed", failed_module_name.c_str());
        return false;
    }

    // 首次加载模块 (load)
    /*
    registry->visit([&succ, &failed_module_name](const std::string& name, ReloadableModule* module) {
        if (succ && !module->load()) {
            failed_module_name = name;
            succ               = false;
            return false;
        }
        return true;
    });
    */

    if (!succ) {
        TERR("first load ReloadableModule[%s] failed", failed_module_name.c_str());
        return false;
    }

    /*
    _reload_thread = putil::LoopThread::createLoopThread(
        [registry]() {
            registry->visit([](const std::string& name, ReloadableModule* module) {
                if (!module->reload()) {
                    TERR("reload ReloadableModule[%s] failed", name.c_str());
                }
                TDEBUG("ReloadableModule[%s] reloaded", name.c_str());
                return true;
            });
        }, FLAGS_module_reload_interval * 1000);
    */

    return true;
}

/*
bool AppProxy::loadTopologyFromBiz(const std::string& name, const std::string& path) {
    if (_rpc_component == nullptr) {
        TERR("_rpc_component is null, loadTopologyFromBiz failed, name:[%s].", name.c_str());
        return false;
    }
    // file for topology
    std::string config_file = path + "/topology_config.json";
    std::string content;
    if (!butil::ReadFileToString(butil::FilePath(config_file), &content)) {
        TERR("biz [%s] read file[%s] failed, can't load topology.", name.c_str(), config_file.c_str());
        return false;
    }

    // check file content
    sw::proto::TopologyConfigWithModuleDefinition topology_config;
    google::protobuf::util::JsonParseOptions       parse_options;
    parse_options.ignore_unknown_fields = true;
    auto status = google::protobuf::util::JsonStringToMessage(content, &topology_config, parse_options);
    if (!status.ok()) {
        TERR("biz [%s] parse as TopologyConfigWithModuleDefinition from string[%s] failed. details: %s, can't load "
             "topology.",
             name.c_str(), content.c_str(), status.ToString().c_str());
        return false;
    }

    // check topology name
    if (topology_config.runnable_topology().name() != name) {
        TERR("biz [%s] name must be the same with topology name [%s]", name.c_str(),
             topology_config.runnable_topology().name().c_str());
        return false;
    }
    return _rpc_component->module_topology_manager()->add_topology(topology_config);
}*/

bool AppProxy::unloadTopologyByName(const std::string& name) {
    if (_rpc_component == nullptr) {
        TERR("_rpc_component is null, unloadTopologyByName failed, name:[%s].", name.c_str());
        return false;
    }
    // do not remove default topology
    if (name == "default") {
        return true;
    }

    _rpc_component->module_topology_manager()->remove_topology(name);
    return true;
}

bool AppProxy::loadBiz(const std::string& name, const std::string& path) {

/*#ifdef _SW_BIND_BIZ_AND_TOPOLOGY
    if (!loadTopologyFromBiz(name, path)) {
        TERR("loadTopologyFromBiz  failed, name:[%s] path:[%s]", name.c_str(), path.c_str());
        return false;
    }
#endif*/

    return _app->loadBiz(name, path);
}

bool AppProxy::unloadBiz(const std::string& name) {
#ifdef _SW_BIND_BIZ_AND_TOPOLOGY
    if (!unloadTopologyByName(name)) {
        TERR("unloadTopologyByName  failed, name:[%s]", name.c_str());
        return false;
    }
#endif
    return _app->unloadBiz(name);
}

}  // namespace sw
