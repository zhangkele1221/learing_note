#include "sw/server/SearchWorker.h"
#include "app_interface/SwApp.h"
#include "json2pb/pb_to_json.h"
#include "cppcommon/monitor/cat_initializer.h"
#include "sw/common/ComponentRegistry.h"
#include "sw/config/ServerConfiguration.h"
#include "sw/server/AppProxy.h"
#include "sw/server/Monitor.h"
#include "sw/server/SessionLocalData.h"
#include "sw/server/ChannelManager.h"
#include "sw/server/component/RpcComponentImpl.h"
#include "sw/util/Log.h"
#include <string>

DECLARE_bool(enable_cat_monitor);
namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, SearchWorker);

SearchWorker::SearchWorker() {
    _started                = false;
    _rpcServer              = nullptr;
    _workerTargetController = nullptr;
    _appProxy               = nullptr;
}

SearchWorker::~SearchWorker() {
    stop();
    destroy();
}

bool SearchWorker::init(const std::string& configPath) {
    if (!ServerConfiguration::instance()->load(configPath.c_str())) {
        TERR("load ServerConfiguration failed");
        return false;
    }

    const std::string& catDomain = ServerConfiguration::instance()->catDomain();
    if (FLAGS_enable_cat_monitor && !red_search_cppcommon::InitCatClient(catDomain.c_str())) {
        TERR("init catClient with domain[%s] failed", catDomain.c_str());
        return false;
    }
    TDEBUG("Catclient initialized");

    if (!initWorkerApp()) {
        return false;
    }
    TDEBUG("WorkerApp initialized");

#ifdef _SW_ENABLE_FEATURE_SESSION_LOCAL_ARENA
    SessionLocalArenaPool::instance()->Reserve();
#endif
    return true;
}

// reverse of init
void SearchWorker::destroy() {
    if (_appProxy) {
        _appProxy->rpc_component()->module_topology_manager()->clear();
        auto *app = _appProxy->get_inner_app();
        DELETE_AND_SET_NULL(_appProxy);
        _appLoader.destroy(app);
    }
    ChannelManager::instance()->clear();
}

bool SearchWorker::start() {
    if (_started) {
        return true;
    }
    brpc::ServerOptions serverOptions;
    serverOptions.method_max_concurrency = ServerConfiguration::instance()->serverMethodMaxConcurrency();
    serverOptions.num_threads            = -1;
#ifdef _SW_ENABLE_FEATURE_SESSION_LOCAL_ARENA
    // compile error if //bazel:brpc.arena.patch is not applied
    // brpc本身对Request和Response的构造都没有采用arena构造，这个patch是为了让brpc支持使用arena来构造Request和Response，
    // 优化在Request和Response比较大的情况下的内存使用。
    serverOptions.session_local_arena_factory = SessionLocalArenaPool::instance();
#endif
    _rpcServer = new (std::nothrow)RpcServer();
    if (!_rpcServer->start(ServerConfiguration::instance()->serviceServerPort(), _appProxy, serverOptions)) {
        TERR("start search worker failed!");
        return false;
    }
    TDEBUG("start rpc server success.");

    _workerTargetController = new WorkerTargetController(_appProxy, this,
            ServerConfiguration::instance()->heartbeatServerPort());
    if (!_workerTargetController->start()) {
        TERR("start search worker failed!");
        return false;
    }
    TDEBUG("start heartbeat server success.");

    TLOG("start search worker success!");
    _started = true;
    return true;
}

// reverse of start
void SearchWorker::stop() {
    if (!_started) {
        return;
    }
    if (_rpcServer) {
        _rpcServer->stop();
        DELETE_AND_SET_NULL(_rpcServer);
    }
    if (_workerTargetController) {
        _workerTargetController->stop();
        DELETE_AND_SET_NULL(_workerTargetController);
    }
    _started = false;
}

bool SearchWorker::initWorkerApp() {
    const std::string& modulePath = ServerConfiguration::instance()->appSoPath();
    if (!_appLoader.load(modulePath.c_str())) {
        TERR("load SwApp failed");
        return false;
    }

    gflags::SetVersionString(_appLoader.version() + "@" + _appLoader.name());

    auto* app    = _appLoader.create(SwApp::uuid_high, SwApp::uuid_low);
    auto* swApp = dynamic_cast<SwApp*>(app);
    if (!swApp) {
        _appLoader.destroy(app);
        TERR("dynamic_cast IBaseInterface to SwApp failed");
        return false;
    }
    _appProxy = new (std::nothrow) AppProxy(swApp);
    TLOG("_appProxy build done");
    if (!_appProxy->init(&ComponentRegistry::instance()->get_inner_map())) {//REGISTER_COMPONENT 所有这里的 主键就是通过这个宏函数传进去的
        return false;
    }
    TLOG("_appProxy init done");
    if (!_appProxy->setup_reload_scheme()) {//这里还是挺重要的...............继续分析这里..........
        return false;
    }
    TLOG("_appProxy setup_reload_scheme done");
    // 这里Validate而不是直接在add_topology的时候Validate的原因是：
    // Validate需要app init，以确保module中依赖的app.init中的一些逻辑能够正常工作。
    if (!_appProxy->rpc_component()->module_topology_manager()->ValidateAndReserveSessionLocalDataOnStartup()) {
        return false;
    }
    TLOG("_appProxy ValidateAndReserveSessionLocalDataOnStartup done");
    return true;
}

bool SearchWorker::checkHealth() const {
    return _rpcServer != nullptr && _rpcServer->isRunning() &&
        _appProxy != nullptr && (0 == _appProxy->check_health());
}

const std::pair<std::string, std::string> SearchWorker::getPnsMeta() const {
    std::pair<std::string, std::string>                        result;
    const std::pair<std::string, ::google::protobuf::Message*> meta = _appProxy->get_pns_meta();
    if (meta.second) {
        std::string             data;
        std::string             error;
        json2pb::Pb2JsonOptions options;
        options.bytes_to_base64 = true;
        if (!json2pb::ProtoMessageToJson(*meta.second, &result.second, options, &error)) {
            TERR("format pns meta message to json failed: %s", error.c_str());
        }
    }
    result.first = meta.first;
    return result;
}

}  // namespace sw
