#ifndef RED_SEARCH_WORKER_SW_SERVER_RPCSERVER_H_
#define RED_SEARCH_WORKER_SW_SERVER_RPCSERVER_H_

#include <brpc/server.h>

namespace sw {

class RpcServiceImpl;
class AppProxy;

class RpcServer {
public:
    RpcServer() {}
    ~RpcServer() {}

public:
    bool isRunning();
    bool start(int32_t port, AppProxy *, const brpc::ServerOptions& options);
    void stop();

private:
    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

    bool addService(AppProxy *);
    brpc::Server _server;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_RPCSERVER_H_
