#ifndef RED_SEARCH_WORKER_SW_SERVER_CHANNELMANAGER_H_
#define RED_SEARCH_WORKER_SW_SERVER_CHANNELMANAGER_H_

#include "brpc/channel_base.h"
#include "bthread/mutex.h"
#include "butil/containers/flat_map.h"
#include "butil/containers/doubly_buffered_data.h"
//#include "sw/proto/rpc_call_config.pb.h"
#include "RpcComponentImpl.h"

namespace sw {

enum ChannelType {
    PRPC_MATRIX_CHANNEL,
    PRPC_CHANNEL,
    PRPC_CHANNEL_WITH_CUSTOM_META,
    BRPC_CHANNEL,
    PNS_CHANNEL,
};

struct ChannelMeta {
    ChannelMeta() : channel(nullptr), channel_type(PRPC_CHANNEL) {}

    std::unique_ptr<::google::protobuf::RpcChannel> channel;
    ChannelType channel_type;
    std::string remote_naming_service_uri;
};

struct ChannelDetail {
    ChannelDetail() : config(), channel_meta() {}

    RpcCallingConfig config;
    ChannelMeta channel_meta;
};

class ChannelManager {
public:
    class Index {
    public:
        Index();
        // with read lock
        const ChannelDetail* seek(const std::string& name) const {
            auto p = _map.seek(name);
            return p ? *p : nullptr;
        }

        // with write lock
        bool add(const std::string& name, ChannelDetail* detail);

        // with write lock
        void clear();

        size_t size() {
            return _map.size();
        }

    private:
        butil::FlatMap<std::string, ChannelDetail*> _map;
    };

    ~ChannelManager();
    // ensure thread-safe
    static ChannelManager* instance();

    // it's safe to return raw pointer since it's only deleted when process exits
    const ChannelDetail * seek(const std::string& name);

    bool Add(const RpcCallingConfig &config, const ChannelMeta **);
    
    /*bool GetOrCreate(const proto::RpcCall &,
                             const char * naming_service_uri, const char * protocol,
                             const ::google::protobuf::MethodDescriptor * method,
                             const ChannelMeta **);
    */

    void clear();

private:
    ChannelManager();
    ChannelManager(const ChannelManager &);
    void operator=(const ChannelManager &);

    ChannelDetail * create_channel(const RpcCallingConfig &config);

    butil::DoublyBufferedData<Index> _dbd;
    bthread::Mutex _lock;
    butil::FlatMap<std::string, ChannelDetail*> _storage;
};

const char * channeltype2str(ChannelType type);
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_CHANNELMANAGER_H_
