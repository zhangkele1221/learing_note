#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXTDATA_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXTDATA_H_

namespace sw {
class RequestContext;

/**
 * 该类的定义和RunnableModule除了run相关的函数之外是十分相似的，其生命周期和RunnableModule也是一样的，方法的调用时机也是一样
 *
 */
class RequestContextData {
 public:
    virtual ~RequestContextData() {}

    /**
     * 初始化函数。在请求到来会初始化一次。返回false会直接导致本次请求失败。
     *
     * 和RunnableModule的init是一起执行的，且在RunnableModule的init调用之前先调用
     */
    virtual bool init(RequestContext*) { return true; }

    /**
     * 重置函数。相对应于init，重置内部数据为初始值，以便于后续进行重用。
     *
     * 和RunnableModule的reset是一起执行的，且在RunnableModule的init调用之后再调用
     */
    virtual void reset() = 0;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_CONTEXTDATA_H_
