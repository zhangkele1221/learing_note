#include "RpcComponentImpl.h"
#include "ChannelManager.h"
#include "ComponentRegistry.h"
#include "ServerConfiguration.h"
#include "Log.h"
//#include "sw/proto/rpc_call_config.pb.h"
//#include "sw/proto/topology_config.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/json_util.h"
#include "butil/file_util.h"
//#include "brpc/adaptive_protocol_type.h"

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, RpcComponentImpl);

static const ServiceCallingOptions kDefaultServiceCallingOptions = {200, 500, 0, "baidu_std", "rr", "", "", "", ""};

/*
static bool pack_minimal_topology(
        const ::google::protobuf::RepeatedPtrField<proto::Module>& module_list,
        const proto::MinimalTopology& topology, proto::TopologyConfigWithModuleDefinition* output) {
    proto::TopologyConfigWithModuleDefinition packed;
    *packed.mutable_module() = module_list;
    packed.mutable_runnable_topology()->set_name(topology.name());
    std::string name_of_last_element;
    for (const auto & e : topology.element()) {
        auto *added = packed.mutable_runnable_topology()->add_element();
        added->set_name(e);
        if (!name_of_last_element.empty()) {
            added->add_dependency(name_of_last_element);
        }
        name_of_last_element = e;
    }

    output->Swap(&packed);
    return true;
}
*/

static bool pack_simple_topology(
        const ::google::protobuf::RepeatedPtrField<proto::Module>& module_list,
        const proto::SimpleTopology& topology, proto::TopologyConfigWithModuleDefinition* output) {
    proto::TopologyConfigWithModuleDefinition packed;
    *packed.mutable_module() = module_list;
    packed.mutable_runnable_topology()->set_name(topology.name());
    std::string name_of_last_element;
    for (const auto & e : topology.element()) {
        auto *added = packed.mutable_runnable_topology()->add_element();
        added->set_name(e.name());
        if (!name_of_last_element.empty()) {
            added->add_dependency(name_of_last_element);
        }
        *added->mutable_custom_params() = e.custom_params();
        name_of_last_element = e.name();
    }
    output->Swap(&packed);
    return true;
}

static bool pack_semi_sequential_topology(
        const ::google::protobuf::RepeatedPtrField<proto::Module>& module_list,
        const proto::SemiSequentialTopology& topology,
        const ModuleTopologyManager& topology_manager,
        proto::TopologyConfigWithModuleDefinition* output) {
    proto::TopologyConfigWithModuleDefinition packed;
    *packed.mutable_module() = module_list;

    packed.mutable_runnable_topology()->set_name(topology.name());
    *packed.mutable_runnable_topology()->mutable_custom_params() = topology.custom_params();
    std::string name_of_last_sync_element;
    for (const auto & e : topology.element()) {
        auto *added = packed.mutable_runnable_topology()->add_element();
        added->set_name(e.name());
        *added->mutable_custom_params() = e.custom_params();
        *added->mutable_dependency() = e.dependency();
        if (!name_of_last_sync_element.empty() && std::find(added->dependency().begin(),
                added->dependency().end(), name_of_last_sync_element) == added->dependency().end()) {
            added->add_dependency(name_of_last_sync_element);
        }

        std::string factory_name;
        for (const auto & m : module_list) {
            if (m.name() == e.name()) {
                factory_name = m.factory_name();
            }
        }
        if (factory_name.empty()) {
            TERR("element[%s] of topology[%s] not found in module definitions", e.name().c_str(),
                 topology.name().c_str());
            return false;
        }

        if (topology_manager.is_sync_module(factory_name)) {
            name_of_last_sync_element = e.name();
        }
    }

    output->Swap(&packed);
    return true;
}

/*
static bool pack_runnable_topology(
        const ::google::protobuf::RepeatedPtrField<proto::Module>& module_list,
        const proto::RunnableTopology& topology, proto::TopologyConfigWithModuleDefinition* output) {
    proto::TopologyConfigWithModuleDefinition packed;
    *packed.mutable_module() = module_list;
    *packed.mutable_runnable_topology() = topology;

    output->Swap(&packed);
    return true;
}*/

bool is_pb_service(const std::string& protocol) {
    brpc::AdaptiveProtocolType protocol_type;
    protocol_type = protocol;
    // only http service is treated as non_pb_service till now(2020/04/22).
    return (brpc::PROTOCOL_HTTP != (brpc::ProtocolType)(protocol_type));
}

const ::google::protobuf::MethodDescriptor * FindProtobufMethod(
        const std::string& full_method_name) {
    const auto * descriptor_pool = ::google::protobuf::DescriptorPool::generated_pool();
    if (!descriptor_pool) {
        TERR("::google::protobuf::DescriptorPool::generated_pool() is nullptr");
        return nullptr;
    }
    auto * message_pool = ::google::protobuf::MessageFactory::generated_factory();
    if (!message_pool) {
        TERR("::google::protobuf::MessageFactory::generated_factory() is nullptr");
        return nullptr;
    }
    auto *method_descriptor = descriptor_pool->FindMethodByName(full_method_name);
    if (!method_descriptor) {
        TERR("method[%s] not found in generated_pool. Maybe you have different protobuf linked? "
             "Or you just provided the wrong full qualified name", full_method_name.c_str());
        return nullptr;
    }
    return method_descriptor;
}

RpcComponentImpl::RpcComponentImpl() {}

bool RpcComponentImpl::add_serving(::google::protobuf::Service *service, bool transfer_ownership) {
    _raw_services.push_back(std::make_pair(service, transfer_ownership));
    return true;
}

// 2
bool RpcComponentImpl::add_serving(const ::google::protobuf::ServiceDescriptor *service_descriptor,
                                   const ::google::protobuf::Message *request_prototype,
                                   const ::google::protobuf::Message *response_prototype,
                                   const std::string &method_name, const std::string &topology_name,
                                   const ServiceServingOptions *options) {
    RpcServingConfig config;
    config.service_descriptor = service_descriptor;
    config.method_name = method_name;
    config.request_prototype = request_prototype;
    config.response_prototype = response_prototype;
    config.topology_name = topology_name;
    if (options) {
        config.options = *options;

        if (!options->response_compress_type.empty()) {
            // response_compress_type
            const auto *descriptor_pool = ::google::protobuf::DescriptorPool::generated_pool();
            if (!descriptor_pool) {
                TERR("::google::protobuf::DescriptorPool::generated_pool() is nullptr");
                return false;
            }

            const auto *enum_descriptor = descriptor_pool->FindEnumTypeByName("brpc.CompressType");
            if (!enum_descriptor) {
                TERR("EnumDescriptor not found for brpc.CompressType. This ERROR indicates some bugs in linkage");
                return false;
            }

            const auto *enum_value_descriptor = enum_descriptor->FindValueByName(
                    options->response_compress_type);
            if (!enum_value_descriptor) {
                TERR("invalid response_compress_type[%s], which must be name of brpc.CompressType",
                     options->response_compress_type.c_str());
                return false;
            }
            config.response_compress_type = static_cast<brpc::CompressType>(enum_value_descriptor->number());
        }
    }

    _rpc_serving_configs.push_back(std::move(config));
    TDEBUG("add rpc_serving[%s@%s.%s] successfully", topology_name.c_str(),
           service_descriptor->full_name().c_str(), method_name.c_str());
    return true;
}

// 1 
bool RpcComponentImpl::add_serving(const std::string &service_full_name, const std::string &method_name,
                                   const std::string &topology_name, const ServiceServingOptions *options) {
    const auto * descriptor_pool = ::google::protobuf::DescriptorPool::generated_pool();
    if (!descriptor_pool) {
        TERR("::google::protobuf::DescriptorPool::generated_pool() is nullptr");
        return false;
    }
    auto * message_pool = ::google::protobuf::MessageFactory::generated_factory();
    if (!message_pool) {
        TERR("::google::protobuf::MessageFactory::generated_factory() is nullptr");
        return false;
    }
    const auto * service_desc = descriptor_pool->FindServiceByName(service_full_name);
    if (!service_desc) {
        std::cout<<" RpcComponentImpl::add_serving service_full_name "<<service_full_name<<std::endl;
        TERR("service[%s] not found in generated_pool. Maybe you have different protobuf linked? "
             "Or you just provided the wrong full qualified name", service_full_name.c_str());
        return false;
    }
    const auto * method_desc = service_desc->FindMethodByName(method_name);
    if (!method_desc) {
        TERR("method[%s] not found in service[%s]", method_name.c_str(), service_full_name.c_str());
        return false;
    }
    const auto * request_prototype = message_pool->GetPrototype(method_desc->input_type());
    if (!request_prototype) {
        TERR("input_type[%s] of method[%s] not found", method_desc->input_type()->full_name().c_str(),
             method_desc->full_name().c_str());
        return false;
    }
    const auto * response_prototype = message_pool->GetPrototype(method_desc->output_type());
    if (!response_prototype) {
        TERR("output_type[%s] of method[%s] not found", method_desc->output_type()->full_name().c_str(),
             method_desc->full_name().c_str());
        return false;
    }
    //2 
    return add_serving(service_desc, request_prototype, response_prototype, method_name, topology_name, options);
}

bool RpcComponentImpl::add_topology(const std::string &name, const std::vector<RunnableElement> &topology) {
    std::unordered_map<std::string, std::string> topology_params;
    return _module_topology.add_topology(name, topology, std::move(topology_params));
}

bool RpcComponentImpl::add_topology(const std::string &config_or_file_path, bool is_config_path) {
    std::string content;
    if (is_config_path) {
        if (!ServerConfiguration::instance()->Get(config_or_file_path, &content)) {
            TERR("fetch config[%s] failed", config_or_file_path.c_str());
            return false;
        }
    } else {
        if (!butil::ReadFileToString(butil::FilePath(config_or_file_path), &content)) {
            TERR("read file[%s] failed", config_or_file_path.c_str());
            return false;
        }
    }

    sw::proto::TopologyConfig topology_config;
    google::protobuf::util::JsonParseOptions parse_options;
    parse_options.ignore_unknown_fields = true;
    auto status = google::protobuf::util::JsonStringToMessage(content, &topology_config, parse_options);
    if (!status.ok()) {
        TERR("parse as TopologyConfig from string[%s] failed. details: %s", content.c_str(), status.ToString().c_str());
        return false;
    }

#ifdef _SW_BIND_BIZ_AND_TOPOLOGY
    for (const auto & i : topology_config.module()) {
        if (!_module_topology.add_module(i)) {
            return false;
        }
    }
#endif

    /*
    // for minimal topology
    for (const auto & t : topology_config.minimal_topology()) {
        proto::TopologyConfigWithModuleDefinition packed;
        if (!pack_minimal_topology(topology_config.module(), t, &packed)) {
            return false;
        }
        if (!_module_topology.add_topology(packed)) {
            return false;
        }
    }
    */

    // for simple topology
    /*
    for (const auto & t : topology_config.simple_topology()) {
        proto::TopologyConfigWithModuleDefinition packed;
        if (!pack_simple_topology(topology_config.module(), t, &packed)) {
            return false;
        }
        if (!_module_topology.add_topology(packed)) {
            return false;
        }
    }

    // for SemiSequentialTopology
    for (const auto & t : topology_config.semi_sequential_topology()) {
        proto::TopologyConfigWithModuleDefinition packed;
        if (!pack_semi_sequential_topology(topology_config.module(), t, _module_topology,
                &packed)) {
            return false;
        }
        if (!_module_topology.add_topology(packed)) {
            return false;
        }
    }

    // for general topology
    for (const auto & t : topology_config.runnable_topology()) {
        proto::TopologyConfigWithModuleDefinition packed;
        if (!pack_runnable_topology(topology_config.module(), t, &packed)) {
            return false;
        }
        if (!_module_topology.add_topology(packed)) {
            return false;
        }
    }
    */

    return true;
}


bool RpcComponentImpl::add_calling(const std::string &name,
                                   const ::google::protobuf::MethodDescriptor *method_descriptor,
                                   const std::string &naming_service_uri, const ServiceCallingOptions *options) {
    ServiceCallingOptions calling_options = kDefaultServiceCallingOptions;
    if (options) {
        calling_options = *options;
    }
    return add_calling(name, method_descriptor, "", naming_service_uri, calling_options);
}

bool RpcComponentImpl::add_calling(const std::string &name,
                                   const ::google::protobuf::MethodDescriptor *method_descriptor,
                                   const std::string &method_full_name,
                                   const std::string &naming_service_uri, const ServiceCallingOptions &options) {
    if (name.empty()) {
        TERR("rpc_calling name is empty");
        return false;
    }
    if (naming_service_uri.empty()) {
        TERR("naming_service_uri of rpc_calling[%s] is empty", name.c_str());
        return false;
    }
    if (ChannelManager::instance()->seek(name)) {
        TERR("rpc_calling[%s] already added", name.c_str());
        return false;
    }

    // check consistency between method_full_name and method_descriptor
    if (method_descriptor && !method_full_name.empty() &&
        method_descriptor->full_name() != method_full_name) {
        TERR("full_method_name[%s] and method_descriptor[%s] are both specified but have "
             "inconsistent values", method_full_name.c_str(),
             method_descriptor->full_name().c_str());
        return false;
    }

    // calculate method_descriptor if necessary
    if (!method_descriptor && !method_full_name.empty() && is_pb_service(options.protocol)) {
        method_descriptor = FindProtobufMethod(method_full_name);
        if (!method_descriptor) {
            TERR("MethodDescriptor not found for Protobuf Service[%s]", method_full_name.c_str());
            return false;
        }
    }

    // calculate full_method_name if necessary
    std::string full_method_name = method_full_name;
    if (method_descriptor && method_full_name.empty()) {
        full_method_name = method_descriptor->full_name();
    }

    // find prototype for custom meta
    const ::google::protobuf::Message * custom_meta_prototype = nullptr;
    if (options.meta_message_name.length() > 0) {
        const auto * descriptor_pool = ::google::protobuf::DescriptorPool::generated_pool();
        if (!descriptor_pool) {
            TERR("::google::protobuf::DescriptorPool::generated_pool() is nullptr");
            return false;
        }
        auto * message_pool = ::google::protobuf::MessageFactory::generated_factory();
        if (!message_pool) {
            TERR("::google::protobuf::MessageFactory::generated_factory() is nullptr");
            return false;
        }
        auto* message_descriptor  = descriptor_pool->FindMessageTypeByName(options.meta_message_name);
        if (!message_descriptor) {
            TERR("custom meta schema[%s] is not found in descriptor_pool for [%s]",
                options.meta_message_name.c_str(), name.c_str());
            return false;
        }
        const ::google::protobuf::Message* meta_prototype = message_pool->GetPrototype(message_descriptor);
        if (!meta_prototype) {
            TERR("custom meta schema[%s] is not found in message_pool for [%s]",
                options.meta_message_name.c_str(), name.c_str());
            return false;
        }
        custom_meta_prototype = meta_prototype;
    }

    RpcCallingConfig config;
    config.name = name;
    config.method_descriptor = method_descriptor;
    config.method_full_name = full_method_name;
    config.naming_service_uri = naming_service_uri;
    config.options = options;
    config.custom_meta_message_prototype = custom_meta_prototype;

    bool res = ChannelManager::instance()->Add(config, nullptr);
    if (!res) {
        TERR("create channel for %s failed", name.c_str());
        return false;
    }
    TDEBUG("add rpc_calling[%s:%s@%s] successfully", name.c_str(), method_full_name.c_str(),
           naming_service_uri.c_str());
    return true;
}

bool RpcComponentImpl::add_calling(const std::string &name, const std::string &method_full_name,
                                   const std::string &naming_service_uri, const ServiceCallingOptions *options) {
    ServiceCallingOptions calling_options = kDefaultServiceCallingOptions;
    if (options) {
        calling_options = *options;
    }

    return add_calling(name, nullptr, method_full_name, naming_service_uri, calling_options);
}

bool RpcComponentImpl::add_calling(const std::string &config_or_file_path, bool is_config_path) {
    std::string content;
    if (is_config_path) {
        if (!ServerConfiguration::instance()->Get(config_or_file_path, &content)) {
            TERR("fetch config[%s] failed", config_or_file_path.c_str());
            return false;
        }
    } else {
        if (!butil::ReadFileToString(butil::FilePath(config_or_file_path), &content)) {
            TERR("read file[%s] failed", config_or_file_path.c_str());
            return false;
        }
    }

    /*
    sw::proto::RpcCallConfig rpc_call_config;
    google::protobuf::util::JsonParseOptions parse_options;
    parse_options.ignore_unknown_fields = true;
    auto status = google::protobuf::util::JsonStringToMessage(content, &rpc_call_config, parse_options);
    if (!status.ok()) {
        TERR("parse as RpcCall from string[%s] failed. details: %s", content.c_str(), status.ToString().c_str());
        return false;
    }

    if (rpc_call_config.rpc_call_size() <= 0) {
        TWARN("no rpc_call config found in [%s], your config should be like `{\"rpc_call\":[{...}, {...}]}`, "
             "but is `%s`",
             config_or_file_path.c_str(), content.c_str());
        return true;
    }
    */

    /*
    for (const sw::proto::RpcCall& config : rpc_call_config.rpc_call()) {
        if (!config.has_name() || config.name().empty()) {
            TERR("name of rpc_call is required");
            return false;
        }
        if (!config.has_naming_service_uri() || config.naming_service_uri().empty()) {
            TERR("naming_service_uri of rpc_calling[%s] is required", config.name().c_str());
            return false;
        }
        ServiceCallingOptions options = kDefaultServiceCallingOptions;
        if (config.has_connect_timeout_ms()) {
            options.connect_timeout_ms = config.connect_timeout_ms();
        }
        if (config.has_timeout_ms()) {
            options.timeout_ms = config.timeout_ms();
        }
        if (config.has_max_retry()) {
            options.max_retry = config.max_retry();
        }
        if (config.has_protocol()) {
            options.protocol = config.protocol();
        }
        if (config.has_load_balancer_name()) {
            options.load_balancer_name = config.load_balancer_name();
        }
        if (config.has_meta_message_name()) {
            options.meta_message_name = config.meta_message_name();
        }
        if (config.has_etcd_address()) {
            options.etcd_address = config.etcd_address();
        }
        if (config.has_etcd_path()) {
            options.etcd_path = config.etcd_path();
        }
        if (config.has_naming_service_tag()) {
            options.naming_service_tag = config.naming_service_tag();
        }

        if (!add_calling(config.name(), config.remote_method_full_name(), config.naming_service_uri(), &options)) {
            TERR("add rpc_calling[%s] failed", config.name().c_str());
            return false;
        }
    }*/

    return true;
}

bool RpcComponentImpl::has_calling(const std::string& name) const {
    return ChannelManager::instance()->seek(name) != nullptr;
}

void RpcComponentImpl::clear() {
    _rpc_serving_configs.clear();
    _raw_services.clear();
}
}  // namespace sw

REGISTER_COMPONENT(rpc_component, sw::RpcComponentImpl)//"那里有find “rpc_component” 这个串的地方应该就是这里注册进去的.. "

/*
#define REGISTER_COMPONENT(_name_, _class_name_) \
    REGISTER_ELEMENT(sw::ComponentRegistry::instance(), _name_, _class_name_)
*/
