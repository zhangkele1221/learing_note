#ifndef RED_SEARCH_WORKER_SW_SERVER_MODULETOPOLOGYMANAGER_H_
#define RED_SEARCH_WORKER_SW_SERVER_MODULETOPOLOGYMANAGER_H_

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <bthread/mutex.h>  // 添加此行
#include <mutex>            // 原有的标准库头文件

//#include "bthread/mutex.h"
#include <butil/containers/doubly_buffered_data.h>
#include "topology_config.pb.h"
#include "RpcComponent.h"

namespace sw {
class SwApp;
//class SessionLocalDataPool;


// 模块拓扑结构，描述一个拓扑的配置和运行时状态
class ModuleTopology {
 public:
    ModuleTopology();
    ~ModuleTopology();
    
    // 引用计数管理（线程安全）
    void increase_reference() { _reference_count.fetch_add(1, std::memory_order_release); }
    void decrease_reference() { _reference_count.fetch_sub(1, std::memory_order_release); }
    int reference_count() const { return _reference_count.load(std::memory_order_acquire); }
    
    // 会话本地数据池（用于存储会话相关的数据）
    //SessionLocalDataPool* session_local_data_pool() const { return _session_local_data_pool; }
    //void own_session_local_data_pool(SessionLocalDataPool * pool);

    // 拓扑名称、描述、并行模块列表、自定义参数
    std::string name;
    std::vector<RunnableElement> desc;                  // 拓扑中的可运行模块列表
    std::unordered_set<std::string> parallel_modules;   // 可并行运行的模块名集合
    std::unordered_map<std::string, std::string> custom_params; // 拓扑级自定义参数

 private:
    //SessionLocalDataPool * _session_local_data_pool; // 会话本地数据池
    std::atomic_int _reference_count;                // 引用计数器（线程安全）
};


// 仅仅用作ModuleTopologyManager内部存储使用，不对外提供接口；如有需要请访问ModuleTopologyManager._dbd
class ManagerStorage;

// 模块拓扑管理器：负责管理所有拓扑配置的加载、存储和访问
class ModuleTopologyManager {
public:

    // 内部索引类，封装拓扑的存储和操作（线程安全）
    class Index {
        friend class ModuleTopologyManager;
    public:
        Index();

        const ModuleTopology * seek(const std::string& name) const;

        bool shallow_copy_all_topology_desc(
                std::unordered_map<std::string, std::vector<RunnableElement>*> *output) const;

        bool deep_copy_all_topology_desc(
                std::unordered_map<std::string, std::vector<RunnableElement>> *output) const;

        bool deep_copy_topology_desc(const std::string& name, std::vector<RunnableElement> *output) const;

        bool deep_copy_topology_params(const std::string& name, std::unordered_map<std::string, std::string> *) const;

        bool shallow_copy_all_topology_params(
                std::unordered_map<std::string, std::unordered_map<std::string, std::string> *> *output) const;

        size_t add(const std::string& name, ModuleTopology *);

        size_t replace(const std::string& name, ModuleTopology *, std::unordered_set<ModuleTopology *> * replaced);

        size_t remove(const std::string& name, std::unordered_set<ModuleTopology*> * removed);

        void clear(std::unordered_set<ModuleTopology *>* removed);

        size_t size() const { return _map.size(); }

    private:
        std::unordered_map<std::string, ModuleTopology *> _map;
    };
    ModuleTopologyManager();
    ~ModuleTopologyManager();

    // 拓扑排序：验证模块依赖关系，生成执行顺序
    bool topology_sort(const std::vector<RunnableElement> &topology,
                       std::vector<RunnableElement> *sorted_topology,
                       const std::unordered_map<std::string, std::string>& factories
                                    = std::unordered_map<std::string, std::string>(),
                       std::unordered_set<std::string> *parallel_modules = nullptr);

    /*
    bool topology_sort(
       const ::google::protobuf::RepeatedPtrField<proto::RunnableTopology_Element> &topology,
       std::vector<RunnableElement> *sorted_topology,
       const std::unordered_map<std::string, std::string> &factories,
       std::unordered_set<std::string> *parallel_modules = nullptr);
    */

    bool ValidateAndReserveSessionLocalDataOnStartup() const;

    // global-module related methods. They are non-thread-safe, and usually called on startup.
    // 全局模块管理（非线程安全，仅在启动时调用）
    bool add_module_if_not_exist(const std::string& name, const std::string& factory);
    bool add_module(const proto::Module& module);

    // topology related methods. They are thread-safe, and maybe call at anytime.
    // thread-safe, pass outputs by value
    // 拓扑管理接口（线程安全）
    size_t topology_using_count(const std::string& topology_name) const;
    bool contain(const std::string& topology_name) const;
    bool DeepCopyAllTopologyDesc(std::unordered_map<std::string, std::vector<RunnableElement>> * output) const;
    bool DeepCopyTopologyDesc(const std::string& topology_name, std::vector<RunnableElement>* output) const;
    bool DeepCopyTopologyParams(const std::string& topology_name, std::unordered_map<std::string, std::string> *) const;

    // thread-safe to read topology list, however, it's not safe to modify *ModuleTopology
    template <typename... Args>
    bool ReadTopologyList(void (*fn)(const std::unordered_map<std::string, ModuleTopology *>&, Args...),
            Args... args) const {
        butil::DoublyBufferedData<Index>::ScopedPtr ptr;
        if (_dbd.Read(&ptr) == 0) {
            fn(ptr->_map, args...);
            return true;
        }

        return false;
    }

    // thread-safe if add_module is only invoked at startup
    bool add_topology(const std::string &name, const std::vector<RunnableElement> &topology,
            std::unordered_map<std::string, std::string> && topology_params);
    //bool add_topology(const proto::TopologyConfigWithModuleDefinition& topology_with_local_module);
    void remove_topology(const std::string& name);
    void clear();

    // TODO(ziqian): make these settings more explicit
    // 应用相关设置
    void set_app(SwApp * app) { _app = app; } // 设置关联的应用程序

    void set_app_initialized() { _app_initialized = true; }// 标记应用初始化完成

    bool is_sync_module(const std::string& name) const;

private:
    bool has_exactly_global_module(const std::string& name, const std::string& factory_name,
            const std::unordered_map<std::string, std::string>& module_params);
    void wait_and_delete(const std::unordered_set<ModuleTopology *>& to_delete);
    bool _app_initialized;
    SwApp *_app;
    // global module factories which may be used in any topology
    // Dynamically adding one is not supported
    std::unordered_map<std::string, proto::Module> _global_modules;

    mutable butil::DoublyBufferedData<Index> _dbd;

    ManagerStorage* _storage;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_MODULETOPOLOGYMANAGER_H_
