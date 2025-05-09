#include "ModuleScheduler.h"
//#include "SessionLocalData.h"
#include "RequestContextImpl.h"
#include "RpcComponentImpl.h"
#include "ModuleTopologyManager.h"
#include "ComponentRegistry.h"
#include "butil/macros.h"
#include "brpc/closure_guard.h"
#include "brpc/controller.h"
#include "Log.h"
//#include "prpc/PrpcController.h"

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, ModuleScheduler);

void ModuleScheduler::schedule(const ModuleTopology& module_topology,
                               const google::protobuf::Message *request,
                               google::protobuf::Message *response,
                               google::protobuf::RpcController *controller,
                               google::protobuf::Closure *done,
                               const std::string& topology_picked_by_app) {
    brpc::ClosureGuard done_guard(done);
    auto *cntl = static_cast<brpc::Controller *>(controller);
    //alog::Logger::setLogId(cntl->log_id());
/*
    auto * p_session_local_data = module_topology.session_local_data_pool()->Borrow();
    if (!p_session_local_data) {
        TERR("borrow session_local_data for topology[%s] failed", module_topology.name.c_str());
        cntl->SetFailed("borrow session_local_data failed");
        return;
    }
    TDEBUG("session_local_data[%p] of pool[%p] borrowed", p_session_local_data,
           module_topology.session_local_data_pool());
#ifdef _SW_ENABLE_FEATURE_SESSION_LOCAL_ARENA
    p_session_local_data->arena = cntl->session_local_arena();
*/
//#endif
    //auto *context = dynamic_cast<RequestContextImpl *>(p_session_local_data->context);
    //context->log_key_value("remote_side", butil::endpoint2str(cntl->remote_side()).c_str());
    //context->log_key_value("api", cntl->method()->full_name());
    //context->log_key_value("topology_name", module_topology.name + "/" + topology_picked_by_app);
    //if (!context->init(cntl, request, response, done)) {
    if(true){
        TERR("p_context init failed");
        //cntl->SetFailed("context init failed");
        //schedule_done(context);
    } else {
        /*
        if (p_session_local_data->scheduler->schedule(p_session_local_data->context, nullptr)) {
            done_guard.release();
        } else {
            TERR("p_context schedule failed");
            cntl->SetFailed("context schedule failed");
            schedule_done(context);
        }
        */
    }
}

bool ModuleScheduler::schedule(RequestContext * p_context, RunnableModule * runnable_module) {
    //auto *context = dynamic_cast<RequestContextImpl *>(p_context);
    //auto * p_session_local_data = context->container();
    //return p_session_local_data->scheduler->schedule(context, runnable_module);
    return true;
}

void ModuleScheduler::schedule_done(RequestContext * p_context) {
    //auto *context = dynamic_cast<RequestContextImpl *>(p_context);
    //if (context && context->container()) {
        //context->reset();
        //auto *p_session_local_data = context->container();
        //auto * module_topology = p_session_local_data->factory->argument().module_topology;
        //p_session_local_data->pool->Return(p_session_local_data);
        //module_topology->decrease_reference();
        //TDEBUG("session_local_data[%p] of pool[%p] returned", p_session_local_data, p_session_local_data->pool);
    //} else {
        //TERR("context[%p] or container[%p] is nullptr", context, context ? context->container() : nullptr);
    //}
}

}  // namespace sw
