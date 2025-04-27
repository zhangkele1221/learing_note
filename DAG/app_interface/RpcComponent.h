#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_RPCCOMPONENT_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_RPCCOMPONENT_H_

#include <functional>
#include <unordered_map>
#include <vector>

#include "Component.h"

namespace google {
namespace protobuf {
class RpcController;
class Closure;
class Message;
class Service;
class ServiceDescriptor;
class MethodDescriptor;
}  // namespace protobuf
}  // namespace google

namespace sw {
class SwApp;
struct RunnableElement {
    std::string name;
    std::string factory_name;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::string> module_params;
};

template <typename T>
struct ServiceMetaExtractor {};

template <typename Svc, typename Req, typename Res>
struct ServiceMetaExtractor<void (Svc::*)(::google::protobuf::RpcController*, const Req*, Res*,
                                          ::google::protobuf::Closure*)> {
    typedef Svc Service;
    typedef Req Request;
    typedef Res Response;
};

struct ServiceCallingOptions {
    int connect_timeout_ms;
    int timeout_ms;
    int max_retry;
    std::string protocol;
    std::string load_balancer_name;
    std::string etcd_address;
    std::string etcd_path;
    std::string meta_message_name;
    std::string naming_service_tag;
    int backup_request_ms;
};

/**
 * if some class which inherits ::google::protobuf::Service and `RpcComponent::add_serving`ed
 * also inherits this interface its methods will be called at an appropriate time
 */
class IntegratingWithSw {
public:
    // called after SwApp is initialized. `return false` leads to failed startup.
    virtual bool init(SwApp*) = 0;
};

struct TopologyPickerOptions {
    TopologyPickerOptions() : topology_params(nullptr) {}

    std::string service_name;
    std::string method_name;
    std::unordered_map<std::string, const std::unordered_map<std::string, std::string>*>* topology_params;
};

struct ServiceServingOptions {
    /**
     * If valid, it will be called to get the topology_name instead of using param `topology_name`
     * specified in method params.
     */
    std::function<std::string(const ::google::protobuf::Message* /* request */,
                              const TopologyPickerOptions& /* options */)>
        topology_picker;

    // name of brpc.CompressType in brpc/options.proto#brpc.CompressType
    std::string response_compress_type;
};

class RpcComponent : public Component {
public:
    ComponentType get_component_type() const final {
        return ComponentType::RPC_COMPONENT;
    }

    /**
     * Add raw google protobuf service. Usually, it's not for the core services of your app.
     * It is designed for extending builtin services in brpc. Details can be found at
     * https://github.com/apache/incubator-brpc/blob/master/docs/en/builtin_service.md.
     *
     * You may actually expose your core services using this method, but you will not use
     * these abilities provided by SW:
     * (1) modularized code management
     * (2) session local modules avoiding thread-safe issues
     * (3) session local arena for memory management
     * (4) builtin monitor for modules and rpc calling
     * (5) rpc calling with PNS support
     * (6) request sampling and collecting
     * (7) semi-sequential (maybe concurrent in the future) scheduling sw modules
     * (8) release resources after making response which is not counted in server latency
     *
     * However, in some cases where the service is extremely simple and needs extremely high
     * performance, you have an option to remove the overhead of SW runtime framework while keeping
     * SW background data management. Refer to `sw::IntegratingWithSw` for more information.
     *
     * `add_serving` can only be called in SwApp::init
     */
    virtual bool add_serving(::google::protobuf::Service* service, bool transfer_ownership) = 0;

    /**
     * Please use or copy ADD_SERVING macro below.
     */
    virtual bool add_serving(const ::google::protobuf::ServiceDescriptor* service_descriptor,
                             const ::google::protobuf::Message* request_prototype,
                             const ::google::protobuf::Message* response_prototype,
                             const std::string& method_name, const std::string& topology_name,
                             const ServiceServingOptions* options) = 0;

    /**
     * Make sure the protobuf of this service is the same with that of SW, especially when using dynamic
     * linkage.
     *
     * @param service_full_name
     * @param method_name
     * @param topology_name
     * @param options
     * @return
     */
    virtual bool add_serving(const std::string& service_full_name, const std::string& method_name,
                             const std::string& topology_name, const ServiceServingOptions* options) = 0;

    /**
     * `add_topology` is not limited to be called at startup now[2020-07-30]
     *
     * Called at runtime to achieve adding topologies without restart
     *
     * @param name Reference this topology in add_serving
     * @param topology
     * @return
     */
    virtual bool add_topology(const std::string& name, const std::vector<RunnableElement>& topology) = 0;

    /**
     * Parse contents or file according to structure described by `../sw/proto/topology_config.proto`
     *
     * Called at runtime to achieve adding topologies without restart
     */
    virtual bool add_topology(const std::string& config_or_file_path, bool is_config_key) = 0;

    /**
     * `add_calling` can be used anytime and anywhere. It is thread-safe and optimized
     * for single-thread-write-and-multiple-thread-read.
     */
    virtual bool add_calling(const std::string& name,
                             const ::google::protobuf::MethodDescriptor* method_descriptor,
                             const std::string& naming_service_uri,
                             const ServiceCallingOptions* options) = 0;

    /**
     * Make sure the protobuf of this service is the same with that of SW, especially when using dynamic
     * linkage.
     *
     * @param name Reference this calling in AsyncRpcModule
     * @param method_full_name
     * @param naming_service_uri
     * @param options
     * @return
     */
    virtual bool add_calling(const std::string& name, const std::string& method_full_name,
                             const std::string& naming_service_uri,
                             const ServiceCallingOptions* options) = 0;

    /**
     * Parse contents or file according to structure described by `../sw/proto/rpc_call_config.proto`
     */
    virtual bool add_calling(const std::string& config_or_file_path, bool is_config_key) = 0;

    /**
     * Query whether the calling named `name` has been added.
     *
     * @param name reference this calling in AsyncRpcModule
     * @return true if the calling named `name` already added
     */
    virtual bool has_calling(const std::string& name) const = 0;
};

}  // namespace sw

#define ADD_SERVING(_rpc_component_, _service_, _method_, _topology_)                                       \
    (_rpc_component_)                                                                                       \
        .add_serving(                                                                                       \
            _service_::descriptor(),                                                                        \
            &sw::ServiceMetaExtractor<decltype(&_service_::_method_)>::Request::default_instance(),         \
            &sw::ServiceMetaExtractor<decltype(&_service_::_method_)>::Response::default_instance(),        \
            #_method_, _topology_, nullptr)
#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_RPCCOMPONENT_H_
