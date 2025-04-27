#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_SYNCMODULE_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_SYNCMODULE_H_

#include "RunnableModule.h"

namespace sw {

class SyncModule : public RunnableModule {
public:
    ModuleType get_module_type() const final {
        return ModuleType::SYNC_MODULE;
    }

    bool continue_run(RequestContext*) final {
        return true;
    }
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_SYNCMODULE_H_
