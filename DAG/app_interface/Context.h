#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXT_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXT_H_

#include <string>

namespace google {
namespace protobuf {
class Message;
}  // namespace protobuf
}  // namespace google

namespace sw {

class Component;
class RunnableElement;
class RunnableModule;
//class ReloadableModule;
class RequestContextData;

enum CompressType {
    COMPRESS_TYPE_NONE = 0,
    COMPRESS_TYPE_SNAPPY = 1,
    COMPRESS_TYPE_GZIP = 2,
    COMPRESS_TYPE_ZLIB = 3
};

class GlobalContext {
 public:
    virtual ~GlobalContext() {}

    //virtual ReloadableModule* get_reloadable_module(const char* name) = 0;

    template <typename T>
    T * get_reloadable_module(const char * name) {
    //    return static_cast<T *>(get_reloadable_module(name));
    }
};

class RequestContext : public GlobalContext {
 public:
    virtual const std::string& service_full_name() const = 0;

    virtual const std::string& method_name() const = 0;

    /**
     * 获取app提供的factory create的ContextData
     *
     * 仅仅设计了一个ContextData，实际上支持多个也是很容易的，但是出于以下几点不做支持：
     * （1）希望app更加紧密的把数据放在一起
     * （2）app开发者很容易使用组合模式来实现多个ContextData
     */
    virtual RequestContextData* session_local_data() const = 0;

    virtual RunnableModule* get_runnable_module(const char* name) = 0;

    template <typename T>
    T * get_runnable_module(const char * name) {
        return dynamic_cast<T *>(get_runnable_module(name));
    }

    virtual void set_response_compress_type(CompressType) {}

    // 动态的提交一个runnable，该方法不是线程安全的。
    virtual bool submit_runnable(const RunnableElement&) = 0;

    virtual const ::google::protobuf::Message* request() = 0;

    virtual ::google::protobuf::Message* response() = 0;

    virtual const ::google::protobuf::Message* get_request() { return request(); }

    virtual ::google::protobuf::Message* get_response() { return response(); }

    virtual void log_key_value(const std::string &key, const std::string &value) = 0;

    /**
     * 对接公司的tracker机制，对message中的字段进行埋点，支持message嵌套。
     * 一个请求仅支持调用一次，第二次和之后的调用都将直接失败。
     *
     * 其中在message第一层，以下字段是保留字段：
     * red_log_id, red_log_tag, red_app_name, red_log_time, red_log_host
     */
    virtual bool track(const ::google::protobuf::Message &) { return false; }

    /**
     * tiger trakcer日志格式每行的格式是:
     * topic tag 消息体
    */
    virtual bool track(const ::google::protobuf::Message &, const std::string& topic, const std::string& tag) { return false; }
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXT_H_
