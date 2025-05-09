#ifndef RED_SEARCH_WORKER_SW_SERVER_COMPONENT_RPCCOMPONENTIMPL_H_
#define RED_SEARCH_WORKER_SW_SERVER_COMPONENT_RPCCOMPONENTIMPL_H_

#include <utility>
#include <unordered_map>
#include <mutex>  // NOLINT
#include "brpc/adaptive_protocol_type.h"
#include "RpcComponent.h"
#include "butil/containers/flat_map.h"
#include "ModuleTopologyManager.h"
#include "brpc/options.pb.h"

namespace sw {
class SwApp;
struct RpcServingConfig {
    RpcServingConfig() : service_descriptor(nullptr), request_prototype(nullptr)
        , response_prototype(nullptr), response_compress_type(brpc::COMPRESS_TYPE_NONE) {}
    const ::google::protobuf::ServiceDescriptor *service_descriptor;
    const ::google::protobuf::Message *request_prototype;
    const ::google::protobuf::Message *response_prototype;
    std::string method_name;
    std::string topology_name;
    ServiceServingOptions options;
    brpc::CompressType response_compress_type;
};

struct RpcCallingConfig {
    RpcCallingConfig() : method_descriptor(nullptr), custom_meta_message_prototype(nullptr) {}
    std::string name;
    const ::google::protobuf::MethodDescriptor * method_descriptor;
    std::string method_full_name;
    std::string naming_service_uri;
    ServiceCallingOptions options;
    const ::google::protobuf::Message* custom_meta_message_prototype;
};

class RpcComponentImpl : public sw::RpcComponent {
public:
    RpcComponentImpl();

    bool add_serving(::google::protobuf::Service *service, bool transfer_ownership) override;

    bool add_serving(const ::google::protobuf::ServiceDescriptor *service_descriptor,
                     const ::google::protobuf::Message *request_prototype,
                     const ::google::protobuf::Message *response_prototype,
                     const std::string &method_name,
                     const std::string &topology_name,
                     const ServiceServingOptions *options) override;

    bool add_serving(const std::string &service_full_name,
                     const std::string &method_name,
                     const std::string &topology_name,
                     const ServiceServingOptions *options) override;

    bool add_topology(const std::string &name, const std::vector<RunnableElement> &topology) override;

    bool add_topology(const std::string& config_or_file_path, bool is_config_path) override;

    bool add_calling(const std::string &name,
                     const ::google::protobuf::MethodDescriptor *method_descriptor,
                     const std::string &naming_service_uri,
                     const ServiceCallingOptions *options) override;

    bool add_calling(const std::string &name,
                     const std::string &method_full_name,
                     const std::string &naming_service_uri,
                     const ServiceCallingOptions *options) override;

    bool add_calling(const std::string& config_or_file_path, bool is_config_path) override;

    bool has_calling(const std::string& name) const override;

    const std::vector<std::pair<::google::protobuf::Service *, bool>> &raw_services() const {
        return _raw_services;
    }

    const std::vector<RpcServingConfig>& rpc_serving_configs() const {
        return _rpc_serving_configs;
    }

    const ModuleTopologyManager* module_topology_manager() const {
        return &_module_topology;
    }

    ModuleTopologyManager* module_topology_manager() {
        return &_module_topology;
    }

    // TODO(ziqian): make these settings more explicit
    void set_app(SwApp * app) { _module_topology.set_app(app); }

    void set_app_initialized() { _module_topology.set_app_initialized(); }

    void clear();

private:
    bool add_calling(const std::string &name,
                     const ::google::protobuf::MethodDescriptor *method_descriptor,
                     const std::string &method_full_name,
                     const std::string &naming_service_uri,
                     const ServiceCallingOptions &options);

    ModuleTopologyManager _module_topology;
    std::vector<RpcServingConfig> _rpc_serving_configs;
    std::vector<std::pair<::google::protobuf::Service *, bool>> _raw_services;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_COMPONENT_RPCCOMPONENTIMPL_H_
