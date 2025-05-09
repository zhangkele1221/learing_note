#include "RpcServiceImpl.h"
#include "SwApp.h"
#include "brpc/controller.h"
#include "brpc/reloadable_flags.h"
#include "brpc/closure_guard.h"
#include "json2pb/pb_to_json.h"
#include "google/protobuf/descriptor.h"
#include "ModuleScheduler.h"
//#include "sw/server/RequestContextImpl.h"
//#include "sw/server/SessionLocalData.h"
#include "RpcComponentImpl.h"
#include "ComponentRegistry.h"
#include "Log.h"
//#include "butil/string_printf.h"
//#include "butil/fast_rand.h"
//#include "sw/config/ServerConfiguration.h"
#include "google/protobuf/message.h"
#include "gflags/gflags.h"

DECLARE_string(log);
namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, RpcServiceImpl);

DEFINE_int64(access_log_sample_rate, 0, "sample rate of access log. denominator is 1,000,000.");
DEFINE_bool(enable_more_meta_in_access_log, false, "enable more meta in access log, such as "
                                                   "remote side and so on");

BRPC_VALIDATE_GFLAG(access_log_sample_rate, brpc::PassValidate);
RpcServiceImpl::RpcServiceImpl(const ::google::protobuf::ServiceDescriptor *service_descriptor,
                               const std::vector<const RpcServingConfig *>& method_list)
        : _service_descriptor(service_descriptor) {
    _method_necessities_list.resize(method_list.size());
    for (size_t i = 0; i < method_list.size(); ++i) {
        auto* method_descriptor = service_descriptor->FindMethodByName(method_list[i]->method_name);
        if (method_descriptor->index() >= static_cast<int>(_method_necessities_list.size())) {
            _method_necessities_list.resize(method_descriptor->index() + 1);
        }

        _method_necessities_list[method_descriptor->index()] = method_list[i];
    }
    _rpc_component_impl = dynamic_cast<RpcComponentImpl *>(
            ComponentRegistry::instance()->find_element("rpc_component"));
    _hostname = GetHostName();
}

void RpcServiceImpl::PickTopology(const std::unordered_map<std::string, ModuleTopology *> &topology_list,
             const ::google::protobuf::Message* request, const RpcServingConfig* config,
             std::string* topology_picked_by_app, brpc::Controller *cntl, ModuleTopology** final_topology) {
    ModuleTopology *ret = nullptr;
    // try picker
    if (config->options.topology_picker) {
        std::unordered_map<std::string, const std::unordered_map<std::string, std::string> *> all_topology_params;
        for (const auto &iter : topology_list) {
            all_topology_params.emplace(iter.first, &iter.second->custom_params);
        }

        TopologyPickerOptions options;
        options.topology_params = &all_topology_params;
        if (cntl != nullptr) {
            options.service_name = cntl->method()->service()->full_name();
            options.method_name = cntl->method()->name();
        }
        *topology_picked_by_app = config->options.topology_picker(request, options);

        // try picked topology
        auto iter = topology_list.find(*topology_picked_by_app);
        if (iter != topology_list.end()) {
            ret = iter->second;
        }
    }

    if (!ret) {
        // try method-default topology
        auto iter = topology_list.find(config->topology_name);
        if (iter != topology_list.end()) {
            ret = iter->second;
        }
    }

    if (ret) {
        // increase reference in dbd.Read which is sequence before or after dbd.Modify
        ret->increase_reference();
        *final_topology = ret;
    }
}

const ::google::protobuf::ServiceDescriptor* RpcServiceImpl::GetDescriptor() {
    return _service_descriptor;
}

void RpcServiceImpl::CallMethod(const ::google::protobuf::MethodDescriptor* method,
                                ::google::protobuf::RpcController*          controller,
                                const ::google::protobuf::Message*          request,
                                ::google::protobuf::Message*                response,
                                ::google::protobuf::Closure*                done) {
    TDEBUG("Request[%s] of method[%s] is %s", request->GetTypeName().c_str(),
            method->full_name().c_str(), request->ShortDebugString().c_str());
    auto *cntl = static_cast<brpc::Controller *>(controller);
    brpc::ClosureGuard done_guard(done);
    if (FLAGS_access_log_sample_rate > 0
        && (int64_t) butil::fast_rand_less_than(1000000) < FLAGS_access_log_sample_rate) {
        //alog::Logger::getLogger("_sw_request_sample")->logPureMessage(alog::LOG_LEVEL_INFO,
        //        BuildAccessLog(method, controller, request));
    }
    const RpcServingConfig *config = nullptr;
    if (method->index() >= 0 && method->index() < static_cast<int>(_method_necessities_list.size())) {
        config = _method_necessities_list[method->index()];
    }
    if (config) {
        cntl->set_response_compress_type(config->response_compress_type);
        ModuleTopology * final_topology = nullptr;
        std::string topology_picked_by_app;
        const auto * manager = _rpc_component_impl->module_topology_manager();
        bool success = manager->ReadTopologyList(RpcServiceImpl::PickTopology, request, config,
                &topology_picked_by_app, cntl, &final_topology);
        if (success && final_topology) {
            ModuleScheduler::schedule(*final_topology, request, response, controller, done_guard.release(),
                                      topology_picked_by_app);
        } else {
            cntl->SetFailed(-1, "pick topology for method[%s, %d] of service[%s] failed", method->name().c_str(),
                    method->index(), _service_descriptor->full_name().c_str());
        }
    } else {
        cntl->SetFailed(-1, "rpc method[%s, %d] not found in service[%s]", method->name().c_str(),
                              method->index(), _service_descriptor->full_name().c_str());
    }
}

const ::google::protobuf::Message&
RpcServiceImpl::GetRequestPrototype(const ::google::protobuf::MethodDescriptor* method) const {
    if (method->index() >= 0 && method->index() < static_cast<int>(_method_necessities_list.size())) {
        return *_method_necessities_list[method->index()]->request_prototype;
    }
    TERR("rpc method[%s, %d] not found in service[%s], process abort", method->name().c_str(), method->index(),
         _service_descriptor->full_name().c_str());
    std::abort();
}

const ::google::protobuf::Message&
RpcServiceImpl::GetResponsePrototype(const ::google::protobuf::MethodDescriptor* method) const {
    if (method->index() >= 0 && method->index() < static_cast<int>(_method_necessities_list.size())) {
        return *_method_necessities_list[method->index()]->response_prototype;
    }
    TERR("rpc method[%s, %d] not found in service[%s], process abort", method->name().c_str(), method->index(),
         _service_descriptor->full_name().c_str());
    std::abort();
}

std::string RpcServiceImpl::GetHostName() {
    std::string hostname;
    char tmpHostname[256];
    int rc = gethostname(tmpHostname, sizeof(tmpHostname));
    if (rc == 0) {
        hostname.assign(tmpHostname);
    } else {
        hostname.assign("unknown-host");
    }
    return hostname;
}

std::string RpcServiceImpl::BuildAccessLog(const ::google::protobuf::MethodDescriptor *method,
                                           const ::google::protobuf::RpcController *controller,
                                           const ::google::protobuf::Message *request) {
    auto *cntl = static_cast<const brpc::Controller *>(controller);
    std::string json;
    json.reserve(256);
    if (FLAGS_enable_more_meta_in_access_log) {
        json.append("hostname=").append(_hostname);
        json.append(" remote_side=").append(butil::endpoint2str(cntl->remote_side()).c_str());
        json.append(" method=");
    }
    json.append(method->full_name()).append(" - ");
    json2pb::Pb2JsonOptions options;
    json2pb::ProtoMessageToJson(*request, &json, options);
    return json;
}

}  // namespace sw
