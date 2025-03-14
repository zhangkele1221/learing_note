#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_ASYNCRPCMODULE_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_ASYNCRPCMODULE_H_

#include <map>

#include "app_interface/Context.h"
#include "app_interface/RunnableModule.h"

namespace google {
namespace protobuf {
class Message;
}  // namespace protobuf
}  // namespace google

namespace sw {
class AsyncRpcModule;
class RpcCallMeta {
public:
    RpcCallMeta()
        : _feature_flag(0),
          _request_code(0),
          _ns_tag_code(-1),
          _timeout_ms(-1),
          _compress_type(COMPRESS_TYPE_NONE),
          _partitioner(nullptr) {}

    /**
     * request_code。用在一致性哈希的load balancer中
     */
    void set_request_code(uint64_t rc) {
        _feature_flag |= FLAG_REQUEST_CODE;
        _request_code = rc;
    }
    uint64_t request_code() const {
        return _request_code;
    }
    bool has_request_code() const {
        return (_feature_flag & FLAG_REQUEST_CODE);
    }

    /**
     * 指定本次rpc访问的对端ns。如果设置非空的话，会覆盖rpc_call中的ns相关配置。
     */
    const std::string& naming_service_uri() const {
        return _naming_service_uri;
    }
    void set_naming_service_uri(const std::string& naming_service_uri) {
        _naming_service_uri = naming_service_uri;
    }

    /**
     * 本次请求的server的tag。对应于SwApp.h中的getPnsMeta函数返回的tag，目前如果server使用了tag，那么此字段必填。
     */
    const std::string& naming_service_tag() const {
        return _naming_service_tag;
    }
    void set_naming_service_tag(const std::string& naming_service_tag) {
        _feature_flag |= FLAG_NAMING_SERVICE_TAG;
        _naming_service_tag = naming_service_tag;
    }
    bool has_naming_service_tag() const {
        return (_feature_flag & FLAG_NAMING_SERVICE_TAG);
    }
    void set_naming_service_tag_version(const std::string& ns_tag_version) {
        _ns_tag_version = ns_tag_version;
    }
    const std::string& naming_service_tag_version() const {
        return _ns_tag_version;
    }
    void set_naming_service_tag_code(int tc) {
        _ns_tag_code = tc;
    }
    int32_t naming_service_tag_code() const {
        return _ns_tag_code;
    }

    /**
     * 指定本次rpc访问的对端方法全名。如果设置非空的话，会覆盖rpc_call中的method相关配置。
     *
     * TODO(ziqian): 如果实在有必要，可能需要添加一个设置method_descriptor的接口
     */
    void set_full_method_name(const std::string& name) {
        _full_method_name = name;
    }
    const std::string& full_method_name() const {
        return _full_method_name;
    }

    /**
     * 本次请求的超时时间（可选设置）。如有设置，则会覆盖rpc_call配置中的超时时间。
     */
    void set_timeout_ms(int32_t timeout_ms) {
        _timeout_ms = timeout_ms;
    }
    int32_t timeout_ms() const {
        return _timeout_ms;
    }

    /**
     * 本次请求的压缩方式（可选设置）。如有设置，则使用其中指定的压缩方式。
     * 默认不压缩（COMPRESS_TYPE_NONE）。
     * 可选的压缩方式有 COMPRESS_TYPE_NONE, COMPRESS_TYPE_SNAPPY,
     * COMPRESS_TYPE_GZIP, COMPRESS_TYPE_ZLIB 。
     */
    void set_compress_type(CompressType type) {
        _compress_type = type;
    }
    CompressType compress_type() const {
        return _compress_type;
    }

    /**
     * 用于支持软分片，即向同一个server分片发送多个请求。该函数指针会在适当的情况下，
     *
     * 传入参数分别为当前module和server物理分片数、返回值代表在某个分片上要发送的请求数，返回值不必要覆盖所有的物理分片，
     * 并且某分片上发送的请求数可以是0。
     */
    typedef std::map<size_t /* physical_partition_index */, size_t /* rpc_count */> (*partitioner_type)(
        AsyncRpcModule* /* current_module */, size_t /* physical_partition_count */);
    void set_partitioner(partitioner_type partitioner) {
        _partitioner = partitioner;
    }
    partitioner_type partitioner() const {
        return _partitioner;
    }

    void clear() {
        _request_code = 0u;
        _feature_flag = 0u;
        _naming_service_tag.clear();
        _ns_tag_version.clear();
        _ns_tag_code = -1;
        _timeout_ms = -1;
        _compress_type = COMPRESS_TYPE_NONE;
        _partitioner = nullptr;
        _naming_service_uri.clear();
        _full_method_name.clear();
    }

private:
    static const uint64_t FLAG_REQUEST_CODE = 0x01;
    static const uint64_t FLAG_NAMING_SERVICE_TAG = 0x02;
    uint64_t _feature_flag;
    uint64_t _request_code;
    std::string _naming_service_tag;
    std::string _ns_tag_version;
    int32_t _ns_tag_code;

    int32_t _timeout_ms;
    CompressType _compress_type;
    partitioner_type _partitioner;
    std::string _naming_service_uri;
    std::string _full_method_name;
};

struct RemoteSideMeta {
    RemoteSideMeta()
        : partition_internal_index(0),
          partition_index(0),
          partition_count(1),
          naming_service_custom_meta(nullptr) {}

    // 本次请求属于当前分片的第几个请求（从0开始）
    uint32_t partition_internal_index;
    // remote server分片id
    uint32_t partition_index;
    // remote server分片总数
    uint32_t partition_count;

    // 要请求的server在naming service上附属的meta信息（如果有的话）
    const ::google::protobuf::Message* naming_service_custom_meta;

    // 当前请求的对端server的tag（如果有的话）
    std::string naming_service_tag;
};

class AsyncRpcModule : public RunnableModule {
public:
    virtual ~AsyncRpcModule() {}

    ModuleType get_module_type() const final {
        return ModuleType::ASYNC_RPC_MODULE;
    }

    virtual std::string rpc_calling_name() const {
        return "";
    }

    virtual RpcCallMeta* rpc_call_meta() {
        return nullptr;
    }

    /**
     * 在进行rpc调用之前进行调用。可以用来计算rpc_calling_name()的返回和rpc_call_meta()的返回
     */
    virtual bool pre_rpc_call(RequestContext*) {
        return true;
    }

    virtual bool runner_skipped() {
        return false;
    }

    /**
     * 对每一个要发送的请求调用一次
     */
    virtual ::google::protobuf::Message* build_request(const RemoteSideMeta& remote_side_meta) {
        return build_request(remote_side_meta.partition_index, remote_side_meta.partition_count);
    }

    // @deprecated
    virtual ::google::protobuf::Message* build_request(int partition_index, int partition_count) {
        return nullptr;
    }

    /**
     * 对每一个要发送的请求调用一次
     * not fully supported now(2019-10-17）
     */
    virtual ::google::protobuf::Message* create_or_get_response(const RemoteSideMeta& remote_side_meta) {
        return create_or_get_response(remote_side_meta.partition_index, remote_side_meta.partition_count);
    }

    // @deprecated
    virtual ::google::protobuf::Message* create_or_get_response(int partition_index, int partition_count) {
        return nullptr;
    }

    /**
     * 对每一个成功返回的response调用一次。对调用失败的请求不调用该函数
     *
     * not fully supported now(2019-10-17）
     */
    virtual void parse_response(const RemoteSideMeta& remote_side_meta) {
        return parse_response(remote_side_meta.partition_index);
    }

    // @deprecated
    virtual void parse_response(int partition_index) {}

    RunStatus run(RequestContext*) final {
        return RunStatus::SUCCESS;
    }
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_ASYNCRPCMODULE_H_
