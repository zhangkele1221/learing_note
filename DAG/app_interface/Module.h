#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_MODULE_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_MODULE_H_

#include <string>
#include <unordered_map>

#include "app_interface/RpcComponent.h"
namespace sw {
/**
 * module的种类划分规则：
 * （1）首先根据是定时线程运行的还是rpc请求中运行的分类
 * （2）然后在请求中运行的module又根据是否有rpc进行分类
 * （3）然后rpc又可以分为同步和异步
 *                              Module
 *                    /                             \
 *              RunnableModule                  ReloadableModule
 *              /     \          \
 *  PureLogicModule SyncRpcModule AsyncRpcModule
 *
 *  在具体实现中，PureLogicModule和SyncRpcModule几乎无区别，我们叫做SyncModule
 */

enum ModuleType {
    SYNC_MODULE,
    ASYNC_RPC_MODULE,
    RELOADABLE_MODULE,
};

class SwApp;
struct ModuleProperty {
    ModuleProperty() : app(nullptr), module_params(nullptr), topology_description(nullptr) {}
    SwApp *app;
    std::string name;
    // module参数，指`../sw/proto/topology_config.proto`中的`Module.custom_params`
    //  和`../sw/proto/topology_config.proto`中的`*Topology.Element.custom_params`合并之后的结果
    //  相同的字段取值为后者覆盖前者
    const std::unordered_map<std::string, std::string> *module_params;

    // 拓扑相关参数
    std::string topology_name;
    // 当前所在拓扑的描述（不包含动态添加的元素）
    const std::vector<RunnableElement> *topology_description;
};

class Module {
public:
    virtual ModuleType get_module_type() const = 0;

    // 请override `construct`方法
    virtual void set_module_property(ModuleProperty) final {}

    /**
     * 对于同一个Module对象，仅且仅会在构造完毕之后其他方法调用之前立刻调用一次。
     * 返回bool会导致构造失败
     */
    virtual bool construct(ModuleProperty mp) {
        set_module_property(mp);
        return true;
    }

    virtual ~Module() {}
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_MODULE_H_
