#include "sw/server/RpcServer.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "sw/server/RpcServiceImpl.h"
#include "sw/server/SwBuiltinServiceImpl.h"
#include "sw/util/Log.h"
#include "sw/common/ComponentRegistry.h"
#include "sw/server/component/RpcComponentImpl.h"
#include "sw/server/AppProxy.h"

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, RpcServer);

bool RpcServer::start(int32_t port, AppProxy * app, const brpc::ServerOptions& options) {
    if (!addService(app)) {
        return false;
    }
    if (_server.AddService(new (std::nothrow)SwBuiltinServiceImpl(), brpc::SERVER_OWNS_SERVICE) != 0) {
        TERR("start server failed due to add SwBuiltinService failed!");
        return false;
    }
    _server.set_version(gflags::VersionString());
    if (_server.Start(port, &options) != 0) {
        TERR("failed to start RpcServer!");
        return false;
    }
    TLOG("start RpcServer on port [%d] success!", port);
    return true;
}

bool RpcServer::addService(AppProxy *app) {
    auto * rpc_component = dynamic_cast<RpcComponentImpl *>(
            ComponentRegistry::instance()->find_element("rpc_component"));
    for (auto s : rpc_component->raw_services()) {
        auto ownership = s.second ? brpc::SERVER_OWNS_SERVICE : brpc::SERVER_DOESNT_OWN_SERVICE;
        if (0 != _server.AddService(s.first, ownership)) {
            TERR("start server failed due to add AppBuiltinService failed!");
            return false;
        }
    }

    std::map<std::string, std::unordered_set<std::string> > method_sets;
    std::map<std::string, std::vector<const RpcServingConfig *>> method_mapping;
    for (size_t i = 0; i < rpc_component->rpc_serving_configs().size(); ++i) {
        const auto &meta = rpc_component->rpc_serving_configs().at(i);
        if (meta.service_descriptor->FindMethodByName(meta.method_name) == nullptr) {
            TERR("method[%s] not exists in service[%s]", meta.method_name.c_str(),
                 meta.service_descriptor->full_name().c_str());
            return false;
        }

        // if not found, operator[] would insert an default one
        auto &method_set = method_sets[meta.service_descriptor->full_name()];
        if (method_set.find(meta.method_name) != method_set.end()) {
            TERR("repeated method[%s] found in service[%s]", meta.method_name.c_str(),
                 meta.service_descriptor->full_name().c_str());
            return false;
        }
        method_set.insert(meta.method_name);

        // if not found, operator[] would insert an default one
        method_mapping[meta.service_descriptor->full_name()].push_back(&meta);
    }

    for (const auto &iter : method_mapping) {
        if (static_cast<int>(iter.second.size()) != iter.second.at(0)->service_descriptor->method_count()) {
            TERR("You should expose all method of service[%s] to sw. "
                 "Or process would crash when requesting sw unknown methods",
                 iter.second.at(0)->service_descriptor->name().c_str());
            return false;
        }
        auto * serviceImpl = new (std::nothrow)RpcServiceImpl(iter.second.at(0)->service_descriptor, iter.second);
        if (!serviceImpl) {
            TERR("new serviceImpl failed");
            return false;
        }

        if (_server.AddService(serviceImpl, brpc::SERVER_OWNS_SERVICE) != 0) {
            TERR("start server failed due to add SwApp Service failed!");
            return false;
        }
    }

    return true;
}

bool RpcServer::isRunning() {
    return _server.IsRunning();
}

void RpcServer::stop() {
    _server.Stop(0);
    _server.Join();
}

}  // namespace sw
