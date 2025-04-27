#ifndef RED_SEARCH_WORKER_SW_SERVER_APPPROXY_H_
#define RED_SEARCH_WORKER_SW_SERVER_APPPROXY_H_

#include "SwApp.h"
#include <functional>
//#include "putil/LoopThread.h"

namespace sw {
class RpcComponentImpl;
class AppProxy : public SwApp {
public:
    // AppProxy DOEST NOT own inner app
    explicit AppProxy(SwApp * app);

    ~AppProxy();

    virtual bool init(std::unordered_map<std::string, Component*>* components);

    virtual red_search_cppcommon::FactoryRegistry<RunnableModule>* get_runnable_module_factory_registry() {
        return _app->get_runnable_module_factory_registry();
    }

    virtual red_search_cppcommon::ElementRegistry<ReloadableModule>* get_reloadable_module_registry() {
        return _app->get_reloadable_module_registry();
    }

    virtual int check_health() const { return _app->check_health(); }

    virtual bool loadBiz(const std::string& name, const std::string& path);

    virtual bool unloadBiz(const std::string& name);

    virtual const std::pair<std::string, ::google::protobuf::Message*> get_pns_meta() {
        return _app->get_pns_meta();
    }

    SwApp * get_inner_app() {
        return _app;
    }

    bool setup_reload_scheme();

    RpcComponentImpl* rpc_component() const {
        return _rpc_component;
    }

protected:
    bool loadTopologyFromBiz(const std::string& name, const std::string& path);
    bool unloadTopologyByName(const std::string& name);

private:
    SwApp * _app;  // not own
    RpcComponentImpl * _rpc_component;  // not own

    putil::LoopThreadPtr _reload_thread;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_APPPROXY_H_
