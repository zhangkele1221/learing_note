#ifndef RED_SEARCH_WORKER_SW_SERVER_RPCSERVICEIMPL_H_
#define RED_SEARCH_WORKER_SW_SERVER_RPCSERVICEIMPL_H_

#include <vector>
#include <string>
#include <unordered_map>
#include <brpc/controller.h>
#include "google/protobuf/service.h"

namespace sw {
class RpcServingConfig;
class RpcComponentImpl;
class ModuleTopology;
class RpcServiceImpl : public ::google::protobuf::Service {
 public:
    explicit RpcServiceImpl(const ::google::protobuf::ServiceDescriptor *,
            const std::vector<const RpcServingConfig *>&);

    static void PickTopology(const std::unordered_map<std::string, ModuleTopology *> &topology_list,
                             const ::google::protobuf::Message* request, const RpcServingConfig* config,
                             std::string* topology_picked_by_app, brpc::Controller *cntl,
                             ModuleTopology** final_topology);

    const ::google::protobuf::ServiceDescriptor* GetDescriptor() override;

    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                            ::google::protobuf::RpcController*          controller,
                            const ::google::protobuf::Message*          request,
                            ::google::protobuf::Message*                response,
                            ::google::protobuf::Closure*                done) override;

    const ::google::protobuf::Message&
    GetRequestPrototype(const ::google::protobuf::MethodDescriptor* method) const override;

    const ::google::protobuf::Message&
    GetResponsePrototype(const ::google::protobuf::MethodDescriptor* method) const override;

 private:
    static std::string GetHostName();

    std::string BuildAccessLog(const ::google::protobuf::MethodDescriptor* method,
            const ::google::protobuf::RpcController * controller,
            const ::google::protobuf::Message* request);

    const ::google::protobuf::ServiceDescriptor *_service_descriptor;
    std::vector<const RpcServingConfig*> _method_necessities_list;
    const RpcComponentImpl * _rpc_component_impl;
    std::string _hostname;
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_RPCSERVICEIMPL_H_
