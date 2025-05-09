#ifndef RED_SEARCH_WORKER_SW_SERVER_MODULESCHEDULER_H_
#define RED_SEARCH_WORKER_SW_SERVER_MODULESCHEDULER_H_

#include "RunStatus.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace google {
namespace protobuf {
class Message;
class RpcController;
class Closure;
}
}  // namespace google

namespace sw {

class RequestContext;
class RequestContextImpl;
class RunnableModule;
class ReloadableModule;
class RunnableElement;
class ModuleTopology;

class ModuleScheduler {
 public:
    virtual ~ModuleScheduler() {}

    virtual bool init(RequestContextImpl* p_context, ::google::protobuf::Closure*) = 0;

    virtual bool schedule(RequestContextImpl* p_context, RunnableModule *) = 0;

    virtual void reset() = 0;

    virtual RunnableModule* get_runnable_module(const char* name) = 0;

    virtual ReloadableModule* get_reloadable_module(const char* name) = 0;

    virtual bool submit(const RunnableElement &) = 0;

    static void schedule(const ModuleTopology& module_topology,
                         const google::protobuf::Message* request,
                         google::protobuf::Message* response,
                         google::protobuf::RpcController* controller,
                         google::protobuf::Closure * done,
                         const std::string& topology_picked_by_app = nullptr);

    static bool schedule(RequestContext *, RunnableModule *);

    static void schedule_done(RequestContext *);
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_MODULESCHEDULER_H_
