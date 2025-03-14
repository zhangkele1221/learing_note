#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_RUNNABLEMODULE_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_RUNNABLEMODULE_H_

#include "Module.h"
#include "RunStatus.h"

namespace sw {

class RequestContext;
class SwApp;

/**
 * 该类是一个在请求中运行的模块。SW保证该类的对象不会被多线程同时调用。
 *
 * 该类可能随着拓扑的添加、删除和更新会有创建和销毁，如在内部有写全局变量或某一维度的全局变量（某Module内的static变量），请慎重处理多线程问题
 */
class RunnableModule : public Module {
public:
    /**
     * 初始化函数。在请求到来会初始化一次。返回false会直接导致本次请求失败。
     *
     * 所有RunnableModule的init是一起执行的，并且在所有RunnableModule的run开始之前。
     */
    virtual bool init(RequestContext*) {
        return true;
    }

    /**
     * 如果返回为true，则本模块的run/continue_run方法不会调用，等价于run返回RunStatus::SUCCESS，
     * continue_run返回true；
     * 而与此同时init/reset方法依旧会调用，以保证module中可以存在部分逻辑来计算是否可以skip。
     *
     * 主要适用于一些半动态的场景，在既有的静态运行拓扑中进行删减操作，比如在某些请求中该模块不需要运行。
     *
     * 如果在startup的时候无法确定运行拓扑，请使用RequestContext::submit_runnable来任意定制拓扑。
     */
    virtual bool is_skipped() {
        return false;
    }

    /**
     * 主逻辑函数
     */
    virtual RunStatus run(RequestContext*) = 0;

    /**
     * 仅为实现AsyncRpcModule接收到响应之后的运行逻辑
     */
    virtual bool continue_run(RequestContext*) = 0;

    /**
     * 重置函数。与init对应，所有RunnableModule的reset是一起执行的，并且在所有RunnableModule的run/continue_run结束之后。
     *
     * 这里根据调用时机不同会有两种实现：
     * （1）请求到来->reset->init->run->请求结束。
     * （2）请求到来->init->run->请求结束->reset。
     *
     * SW中调度器最开始的实现是第一种，主要是源于brpc的支持，后来为了支持“在发起多个rpc的情况下，失败快速返回而不是等到所有rpc结束”这个特性，
     * 需要在请求结束之后依旧能够有之前rpc
     * call的callback的context，于是才剥离了RunnableModule和request/response的生命周期，
     * 然后使得第二种实现变成了自然：用户可以在reset中放比较重的操作，而不会影响请求响应时间。不过，用户不应该强依赖此实现。
     */
    virtual void reset() = 0;
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_RUNNABLEMODULE_H_
