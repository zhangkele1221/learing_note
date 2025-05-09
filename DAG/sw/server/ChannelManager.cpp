#include "ChannelManager.h"
#include "RpcComponentImpl.h"
//#include "sw/server/Monitor.h"
//#include "sw/proto/rpc_call_config.pb.h"
#include "Log.h"
#include "ServerConfiguration.h"
#include "brpc/channel.h"
//#include "prpc/Channel.h"
//#include "prpc/MatrixChannel.h"
//#include "sw/server/PnsChannel.h"

namespace sw {
// how to static initialization/destruction order issue
//  https://stackoverflow.com/questions/211237/static-variables-initialisation-order
// initialization order: https://en.cppreference.com/w/cpp/language/initialization
// destruction order: https://en.cppreference.com/w/cpp/utility/program/exit
// Briefly,
// (1) static initialization order within the same translation unit is well-defined, which is the same order of
//  definition
// (2) desctruction order is reverse of the initialization order

DECLARE_AND_SETUP_LOGGER(sw, ChannelManager);

static const char * NO_LOAD_BALANCER = "none";

ChannelManager::Index::Index() {
    _map.init(64);
}

bool ChannelManager::Index::add(const std::string &name, ChannelDetail* detail) {
    auto p = _map.seek(name);
    if (p) {
        return false;
    }

    _map.insert(name, detail);
    return true;
}

void ChannelManager::Index::clear() {
    _map.clear();
}

ChannelManager::ChannelManager() {
    _storage.init(64);
}

ChannelManager::~ChannelManager() {
    clear();
}

ChannelManager *ChannelManager::instance() {
    static ChannelManager instance;
    return &instance;
}

const ChannelDetail *ChannelManager::seek(const std::string &name) {
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (0 == _dbd.Read(&ptr)) {
        return ptr->seek(name);
    }
    return nullptr;
}

/*
bool ChannelManager::GetOrCreate(const proto::RpcCall & rpc_call,
                                         const char * naming_service_uri,
                                         const char * rpc_protocol,
                                         const ::google::protobuf::MethodDescriptor * method,
                                         const ChannelMeta ** channel_meta) {
    // ns_uri from app < ns_uri from conf
    std::string remote_naming_service_uri;
    if (naming_service_uri) {
        remote_naming_service_uri = naming_service_uri;
    }
    if (rpc_call.has_naming_service_uri()) {
        remote_naming_service_uri = rpc_call.naming_service_uri();
    }

    std::size_t pns_begin_at = remote_naming_service_uri.find("pns://");
    std::size_t mpns_begin_at = remote_naming_service_uri.find("mpns://");

    std::string etcd_address;
    std::string etcd_path;
    if (0 == pns_begin_at || 0 == mpns_begin_at) {
        etcd_address = ServerConfiguration::instance()->pnsEtcdAddress();
        etcd_path = ServerConfiguration::instance()->pnsEtcdPath();
    }

    // protocol from app < protocol from conf
    std::string protocol;
    if (rpc_protocol) {
        protocol = rpc_protocol;
    }

    if (rpc_call.has_protocol()) {
        protocol = rpc_call.protocol();
    }

    RpcCallingConfig config;
    config.naming_service_uri = remote_naming_service_uri;
    config.method_descriptor = method;
    config.name = remote_naming_service_uri;  // use ns_uri as name in old api
    config.options.load_balancer_name = rpc_call.load_balancer_name();  // safe with default value
    config.options.protocol = protocol;
    config.options.connect_timeout_ms = rpc_call.connect_timeout_ms();  // safe with default value
    config.options.max_retry = rpc_call.max_retry();  // safe with default value
    config.options.timeout_ms = rpc_call.timeout_ms();  // safe with default value
    config.options.etcd_address = etcd_address;
    config.options.etcd_path = etcd_path;

    const ChannelDetail * p_channel_detail = seek(config.name);
    if (p_channel_detail) {
        *channel_meta = &p_channel_detail->channel_meta;
        return true;
    }

    return Add(config, channel_meta);
}*/

ChannelDetail* ChannelManager::create_channel(const RpcCallingConfig &config) {
    // config.name and config.method_descriptor are unused.
    const std::string & rpc_calling_name = config.name;

    std::lock_guard<bthread::Mutex> lock(_lock);

    // find with lock(read/write concurrent)
    auto iter = _storage.seek(rpc_calling_name);
    if (iter) {
        return *iter;
    }

    const std::string& load_balancer_name = config.options.load_balancer_name;
    std::size_t pns_begin_at = config.naming_service_uri.find("pns://");
    std::size_t mpns_begin_at = config.naming_service_uri.find("mpns://");
    std::size_t xpns_begin_at = config.naming_service_uri.find("xpns://");
    std::size_t cpns_begin_at = config.naming_service_uri.find("cpns://");

    std::string etcd_address;
    std::string etcd_path;
    if (0 == pns_begin_at || 0 == mpns_begin_at || 0 == xpns_begin_at) {
        etcd_address = ServerConfiguration::instance()->pnsEtcdAddress();
        etcd_path = ServerConfiguration::instance()->pnsEtcdPath();

        if (!config.options.etcd_address.empty()) {
            etcd_address = config.options.etcd_address;
        }
        if (!config.options.etcd_path.empty()) {
            etcd_path = config.options.etcd_path;
        }
    }

    std::unique_ptr<::google::protobuf::RpcChannel> channel;
    ChannelType channel_type = PRPC_CHANNEL;
    do {
        if (0 == pns_begin_at) {
            //....................先编译.....
        } else if (0 == mpns_begin_at) {        
            //....................先编译.....

        } else if (0 == xpns_begin_at) {
            //....................先编译.....
        } else if (0 == cpns_begin_at) {
            // remove first char 'c' in "cpns://{Tenant}/{namespace}/{group}/{serviceName}"
            //....................先编译.....
        } else {
            std::unique_ptr<brpc::Channel> p_channel(new brpc::Channel());
            brpc::ChannelOptions options;
            options.connect_timeout_ms = config.options.connect_timeout_ms;
            options.timeout_ms = config.options.timeout_ms;
            options.max_retry = config.options.max_retry;
            options.protocol = config.options.protocol;
            if (load_balancer_name == NO_LOAD_BALANCER) {
                if (0 != p_channel->Init(config.naming_service_uri.c_str(), &options)) {
                    TERR("init brpc::Channel with server_address[%s] failed", config.naming_service_uri.c_str());
                    break;
                }
            } else {
                if (0 != p_channel->Init(config.naming_service_uri.c_str(), load_balancer_name.c_str(), &options)) {
                    TERR("init brpc::Channel with service_name[%s] failed", config.naming_service_uri.c_str());
                    break;
                }
            }
            TDEBUG("init brpc::Channel with service_name[%s@%s] successfully", config.options.protocol.c_str(),
                   config.naming_service_uri.c_str());
            channel.reset(p_channel.release());
            channel_type = BRPC_CHANNEL;
        }

        auto * p_channel_detail = new (std::nothrow)ChannelDetail();
        p_channel_detail->channel_meta.channel.swap(channel);
        p_channel_detail->channel_meta.channel_type = channel_type;
        p_channel_detail->channel_meta.remote_naming_service_uri = config.naming_service_uri;
        p_channel_detail->config = config;

        auto * inserted = _storage.insert(rpc_calling_name, p_channel_detail);
        TDEBUG("rpc_calling[%s] created successfully", rpc_calling_name.c_str());
        return *inserted;
    } while (0);
    return nullptr;
}

bool ChannelManager::Add(const RpcCallingConfig &config, const ChannelMeta ** p_channel_meta) {
    ChannelDetail * p = create_channel(config);
    if (p) {
        const std::string& name = config.name;
        std::function<size_t(Index &)> modifier = [&name, p] (Index &list) {
            if (list.add(name, p)) {
                return 1lu;
            }
            return 0lu;
        };

        size_t res = _dbd.Modify(modifier);
        if (res == 1lu) {
            TDEBUG("add rpc_calling[%s] successfully", config.name.c_str());
        } else {
            TWARN("rpc_calling[%s] already added", config.name.c_str());
        }

        if (p_channel_meta) {
            *p_channel_meta = &p->channel_meta;
        }
        return true;
    }
    return false;
}

void ChannelManager::clear() {
    auto modifier = [](Index& list) {
        const size_t size = list.size();
        list.clear();
        return size;
    };

    _dbd.Modify(modifier);
    for (auto & item : _storage) {
        delete item.second;
    }
    _storage.clear();
}

const char * channeltype2str(ChannelType type) {
    switch (type) {
        case ChannelType::BRPC_CHANNEL:
            return "BRPC_CHANNEL";
        case ChannelType::PRPC_CHANNEL:
            return "PRPC_CHANNEL";
        case ChannelType::PRPC_CHANNEL_WITH_CUSTOM_META:
            return "PRPC_CHANNEL_WITH_CUSTOM_META";
        case ChannelType::PRPC_MATRIX_CHANNEL:
            return "PRPC_MATRIX_CHANNEL";
        case ChannelType::PNS_CHANNEL:
            return "PNS_CHANNEL";
        default:
            return "UNKNOWN";
    }
}
}  // namespace sw
