#ifndef RED_SEARCH_WORKER_SW_APP_DEMO_ECHOMODULE_H_
#define RED_SEARCH_WORKER_SW_APP_DEMO_ECHOMODULE_H_

#include "Context.h"
#include "SyncModule.h"
//#include "app_sdk/logger.h"
#include "EchoService.pb.h"

namespace sw {
namespace example {

class EchoModule : public SyncModule { //  SyncModule 又是继承 RunnableModule 类
public:
    virtual bool init(SwApp*, RequestContext*) {
        return true;
    }

    virtual RunStatus run(RequestContext* context) {
        auto* request = dynamic_cast<const sw::example::proto::EchoRequest*>(context->get_request());
        auto* response = dynamic_cast<sw::example::proto::EchoResponse*>(context->get_response());
        response->set_query(request->query());
        //APP_LOG(TRACE) << "echo [" << request->query() << "]";
        return RunStatus::SUCCESS;
    }

    virtual void reset() {}
};

}  // namespace example
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_DEMO_ECHOMODULE_H_
