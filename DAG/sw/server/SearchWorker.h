#ifndef RED_SEARCH_WORKER_SW_SERVER_SEARCHWORKER_H_
#define RED_SEARCH_WORKER_SW_SERVER_SEARCHWORKER_H_

#include "SwApp.h"
#include "AppLoader.h"
//#include "sw/target/heartbeat/HeartBeatManager.h"
#include "RpcServer.h"
#include "RpcServiceImpl.h"
#include "HealthCheckable.h"
//#include "sw/target/WorkerTargetController.h"

namespace sw {
class SessionLocalDataFactory;
class ServerConfiguration;
class AppProxy;

class SearchWorker : public HealthCheckable {
public:
    SearchWorker();
    ~SearchWorker();

    bool init(const std::string& configPath);
    void destroy();

    bool start();
    void stop();
    bool checkHealth() const override;
    const std::pair<std::string, std::string> getPnsMeta() const override;

private:
    explicit SearchWorker(const SearchWorker&);
    SearchWorker& operator=(const SearchWorker&);

    bool initWorkerApp();

    bool                         _started;
    RpcServer*             _rpcServer;
    //WorkerTargetController*      _workerTargetController;

    SwAppLoader _appLoader;// bool StaticLoader::load(const char *)  这个会调用静态app的 模块 
    AppProxy *_appProxy;
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_SEARCHWORKER_H_
