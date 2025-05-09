#ifndef RED_SEARCH_WORKER_SW_SERVER_REQUESTCONTEXTIMPL_H_
#define RED_SEARCH_WORKER_SW_SERVER_REQUESTCONTEXTIMPL_H_

#include <cstdint>
#include <vector>
#include "RequestContextData.h"
#include "Context.h"

namespace google {
namespace protobuf {
class Message;
class Closure;
}  // namespace protobuf
}  // namespace google

namespace brpc {
class Controller;
}  // namespace brpc

namespace sw {
class ModuleScheduler;
class RunnableElement;
class SessionLocalData;

class RequestContextImpl : public RequestContext {
public:
    RequestContextImpl();

    // override
    RequestContextData* session_local_data() const override {
        return _session_local_data;
    }

    RunnableModule* get_runnable_module(const char* name) override;

    //ReloadableModule* get_reloadable_module(const char* name) override;

    bool submit_runnable(const RunnableElement &) override;

    void log_key_value(const std::string& key, const std::string& value) override {
        _key_value_pairs.emplace_back(key, value);
    }

    bool track(const ::google::protobuf::Message&) override;

    bool track(const ::google::protobuf::Message&, const std::string&, const std::string&) override;

    const ::google::protobuf::Message* request() override { return _p_request; }
    ::google::protobuf::Message* response() override { return _p_response; }
    const std::string& service_full_name() const override { return _service_full_name; }
    const std::string& method_name() const override { return _method_name; }

    // getter
    SessionLocalData * container() { //return _container; 
        return nullptr;    
    }
    brpc::Controller * controller() { return _controller; }
    int64_t latency_since_first_byte_us() const;

    void set_response_compress_type(CompressType) override;

    const std::vector<std::pair<std::string, std::string>> &get_log_pair_list() {
        return _key_value_pairs;
    }

    bool construct(SessionLocalData * container);

    bool init(brpc::Controller*          controller,
              const ::google::protobuf::Message*          request,
              ::google::protobuf::Message*                response,
              ::google::protobuf::Closure*                done);

    void reset();

 private:
    // instance local
    RequestContextData* _session_local_data;
    //SessionLocalData* _container;
    ModuleScheduler*                   _p_scheduler;

    // session local
    const ::google::protobuf::Message* _p_request;
    ::google::protobuf::Message*       _p_response;
    brpc::Controller *_controller;
    int64_t _begin_time_us;
    std::vector<std::pair<std::string, std::string>> _key_value_pairs;
    std::string _service_full_name;
    std::string _method_name;
    bool _track_written;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_REQUESTCONTEXTIMPL_H_
