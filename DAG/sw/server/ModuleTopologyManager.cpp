#include "ModuleTopologyManager.h"
#include <typeindex>
#include "SwApp.h"
#include "SyncModule.h"
#include "RpcComponentImpl.h"
//#include "sw/server/SessionLocalData.h"
#include "Log.h"
#include "gflags/gflags.h"

DEFINE_bool(enable_parallel_module_scheduling, false,
            "select appropriate sync modules to parallel run. Thread safety should be considered when enabling it.");
DEFINE_bool(enable_check_allow_parallel_field, false,
            "run sync modules parallelly if FLAGS_enable_parallel_module_scheduling is true "
            "and `allow_parallel` is true.");

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, ModuleTopologyManager);

// referred by `red_search_worker/sw/proto/topology_config.proto:13`
bool Equal(const proto::Module& m1, const proto::Module& m2) {
    if (m1.name() != m2.name()) {
        return false;
    }
    if (m1.factory_name() != m2.factory_name()) {
        return false;
    }
    if (m1.custom_params_size() != m2.custom_params_size()) {
        return false;
    }

    std::unordered_map<std::string, std::string> p1;
    for (const auto & p : m1.custom_params()) {
        p1[p.key()] = p.value();
    }
    std::unordered_map<std::string, std::string> p2;
    for (const auto & p : m2.custom_params()) {
        p2[p.key()] = p.value();
    }
    return (p1 == p2);
}

// use singleton to storage global bvar
class TopologyUsingCountMonitor {
 public:
    explicit TopologyUsingCountMonitor(ModuleTopologyManager * const manager, const std::string& topology_name)
        : _manager(manager), _topology_name(topology_name)
        , _status(TopologyUsingCountMonitor::print_session_using_count, this) {}

    int expose() {
        return _status.expose("topology_using_count_of_" + _topology_name);
    }

    static size_t print_session_using_count(void * arg) {
        auto * self = static_cast<TopologyUsingCountMonitor *>(arg);
        return self->_manager->topology_using_count(self->_topology_name);
    }

 private:
    ModuleTopologyManager * const _manager;  // not own
    std::string _topology_name;
    bvar::PassiveStatus<size_t> _status;
};

class ManagerStorage {
 public:
    explicit ManagerStorage(ModuleTopologyManager *const manager) : _owner(manager) {}

    ~ManagerStorage() {
        std::lock_guard<bthread::Mutex> guard(_storage_mutex);
        for (auto iter = _monitor_storage.begin(); iter != _monitor_storage.end(); ++iter) {
            delete iter->second;
        }
        for (size_t i = 0; i < _module_topology_storage.size(); ++i) {
            delete _module_topology_storage[i];
        }
    }

    bool EmplaceTopologyMonitor(const std::string& topology_name) {
        std::lock_guard<bthread::Mutex> guard(_storage_mutex);
        auto **inner = &_monitor_storage[topology_name];
        if (*inner) {
            return true;
        }

        std::unique_ptr<TopologyUsingCountMonitor> monitor(new TopologyUsingCountMonitor(_owner, topology_name));
        const int rc = monitor->expose();
        if (rc != 0) {
            return false;
        }

        *inner = monitor.release();
        TDEBUG("TopologyUsingCountMonitor emplaced for topology[%s]", topology_name.c_str());
        return true;
    }

    void Add(ModuleTopology* topology) {
        std::lock_guard<bthread::Mutex> guard(_storage_mutex);
        _module_topology_storage.push_back(topology);
    }

    void Delete(ModuleTopology *topology) {
        std::lock_guard<bthread::Mutex> guard(_storage_mutex);
        for (auto iter = _module_topology_storage.begin(); iter != _module_topology_storage.end();) {
            if (topology == *iter) {
                delete topology;
                iter = _module_topology_storage.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    size_t num_topologies() const { return _module_topology_storage.size(); }

 private:
    ModuleTopologyManager * const _owner;
    std::vector<ModuleTopology *> _module_topology_storage;
    std::unordered_map<std::string, TopologyUsingCountMonitor *> _monitor_storage;
    bthread::Mutex _storage_mutex;
};

/*
SessionLocalDataPool * CreateSessionLocalDataPool(const std::string &name, sw::DataFactory *app_sld_factory,
        const void *arg, bool test_and_reserve = false) {
    std::unique_ptr<SessionLocalDataFactory> factory;
    factory.reset(new (std::nothrow)SessionLocalDataFactory(app_sld_factory, arg));

    std::unique_ptr<SessionLocalDataPool> pool;
    pool.reset(new (std::nothrow)SessionLocalDataPool(factory.release(), true));
    if (test_and_reserve) {
        if (!pool->Test()) {
            TERR("test pool[%s] failed", name.c_str());
            return nullptr;
        }
        pool->Reserve();
        TDEBUG("%lu SessionLocalData reserved successfully", pool->size());
    }
    return pool.release();
}*/


ModuleTopology::ModuleTopology() : /*_session_local_data_pool(nullptr),*/ _reference_count(0) {}

ModuleTopology::~ModuleTopology() {
    //delete _session_local_data_pool;
}

/*
void ModuleTopology::own_session_local_data_pool(SessionLocalDataPool *pool) {
    delete _session_local_data_pool;
    _session_local_data_pool = pool;
}
*/

ModuleTopologyManager::Index::Index() {
    _map.reserve(16);
}

const ModuleTopology *ModuleTopologyManager::Index::seek(const std::string &name) const {
    auto iter = _map.find(name);
    return iter == _map.end() ? nullptr : iter->second;
}

bool ModuleTopologyManager::Index::shallow_copy_all_topology_desc(
        std::unordered_map<std::string, std::vector<RunnableElement>*> *output) const {
    for (const auto& iter : _map) {
        output->emplace(iter.first, &iter.second->desc);
    }
    return true;
}

bool ModuleTopologyManager::Index::deep_copy_all_topology_desc(
        std::unordered_map<std::string, std::vector<RunnableElement>> *output) const {
    for (const auto& iter : _map) {
        output->emplace(iter.first, iter.second->desc);
    }
    return true;
}

bool ModuleTopologyManager::Index::deep_copy_topology_desc(const std::string &name,
        std::vector<RunnableElement>* output) const {
    auto iter = _map.find(name);
    if (iter == _map.end()) {
        return false;
    }

    *output = iter->second->desc;
    return true;
}

bool ModuleTopologyManager::Index::deep_copy_topology_params(const std::string &name,
        std::unordered_map<std::string, std::string> *output) const {
    auto iter = _map.find(name);
    if (iter == _map.end()) {
        return false;
    }

    *output = iter->second->custom_params;
    return true;
}

bool ModuleTopologyManager::Index::shallow_copy_all_topology_params(
        std::unordered_map<std::string, std::unordered_map<std::string, std::string> *> *output) const {
    for (const auto& iter : _map) {
        output->emplace(iter.first, &iter.second->custom_params);
    }
    return true;
}

size_t ModuleTopologyManager::Index::add(const std::string &name, ModuleTopology *t) {
    return _map.emplace(name, t).second ? 1lu: 0lu;
}

size_t ModuleTopologyManager::Index::replace(const std::string &name, ModuleTopology * t,
                                             std::unordered_set<ModuleTopology *> * replaced) {
    ModuleTopology ** inner = &_map[name];
    if (*inner) {
        replaced->insert(*inner);
    }
    *inner = t;
    return 1lu;
}

size_t ModuleTopologyManager::Index::remove(const std::string &name,
        std::unordered_set<ModuleTopology*> * removed) {
    auto iter = _map.find(name);
    if (iter == _map.end()) {
        return 0lu;
    }
    removed->insert(iter->second);
    _map.erase(iter);
    return 1lu;
}

void ModuleTopologyManager::Index::clear(std::unordered_set<ModuleTopology *>* removed) {
    for (auto & pair : _map) {
        removed->insert(pair.second);
    }
    _map.clear();
}

ModuleTopologyManager::ModuleTopologyManager() : _app_initialized(false), _app(nullptr)
    , _storage(new ManagerStorage(this)) {}

ModuleTopologyManager::~ModuleTopologyManager() {
    delete _storage;
}

bool ModuleTopologyManager::ValidateAndReserveSessionLocalDataOnStartup() const {
    if (!_app_initialized) {
        return false;
    }
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (0 != _dbd.Read(&ptr)) {
        return false;
    }

    for (const auto & item : ptr->_map) {
        //bool validation_success = item.second->session_local_data_pool()->Test();
        //if (!validation_success) {
            TERR("validate topology[%s] failed", item.first.c_str());
          //  return false;
        //}
    }

    for (const auto & item : ptr->_map) {
        //item.second->session_local_data_pool()->Reserve();
        //const size_t size = item.second->session_local_data_pool()->size();
        TDEBUG("%lu SessionLocalData of topology[%s] reserved successfully", size, item.first.c_str());
    }
    return true;
}


size_t ModuleTopologyManager::topology_using_count(const std::string &topology_name) const {
    
    /*
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (0 == _dbd.Read(&ptr)) {
        auto* t = ptr->seek(topology_name);
        if (t) {
            return static_cast<size_t>(t->reference_count());
        }
    }
    */
    return 0lu;
}

bool ModuleTopologyManager::contain(const std::string &topology_name) const {
    // 创建一个双缓冲数据的智能指针（自动管理缓冲区生命周期）
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    
    // 尝试读取双缓冲数据（_dbd是DoublyBufferedData<Index>类型的成员变量）
    // Read() 方法返回0表示成功获取读缓冲区指针
    if (0 == _dbd.Read(&ptr)) { 
        // 在当前的 Index 对象中查找指定名称的拓扑
        // ptr 此时持有当前活跃的读缓冲区的索引数据
        return ptr->seek(topology_name) != nullptr; 
    }
    
    // 如果读取失败（理论上不会发生，除非系统错误），默认返回false
    return false;
}

bool ModuleTopologyManager::DeepCopyAllTopologyDesc(
        std::unordered_map<std::string, std::vector<RunnableElement>> *output) const {
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (_dbd.Read(&ptr) == 0) {
        return ptr->deep_copy_all_topology_desc(output);
    }

    return false;
}

bool ModuleTopologyManager::DeepCopyTopologyDesc(const std::string &topology_name,
                                                 std::vector<RunnableElement> *output) const {
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (_dbd.Read(&ptr) == 0) {
        return ptr->deep_copy_topology_desc(topology_name, output);
    }
    return false;
}

bool ModuleTopologyManager::DeepCopyTopologyParams(const std::string &topology_name,
                                                   std::unordered_map<std::string, std::string> *output) const {
    butil::DoublyBufferedData<Index>::ScopedPtr ptr;
    if (_dbd.Read(&ptr) == 0) {
        return ptr->deep_copy_topology_params(topology_name, output);
    }
    return false;
}

bool ModuleTopologyManager::is_sync_module(const std::string& factory_name) const {
    try {
        auto &bases = _app->get_runnable_module_factory_registry()
            ->type_traits_registry().base_type_index(factory_name.c_str());
        // This needs __RED_ENABLE_TR2__ in red_search_cppcommon/registry/registry_inl.h
        if (std::find(bases.begin(), bases.end(), std::type_index(typeid(SyncModule))) != bases.end()) {
            return true;
        }
    } catch (std::runtime_error& e) {
        return false;
    }
    return false;
}

/*
拓扑排序 (topology_sort)
功能：验证模块依赖关系是否无环，生成拓扑执行顺序。
逻辑：
遍历所有模块，收集依赖关系。
使用类似 Kahn 算法的拓扑排序，确保无循环依赖。
根据并行调度标志 (enable_parallel_module_scheduling)，标记可并行运行的同步模块。
*/
bool ModuleTopologyManager::topology_sort(const std::vector<RunnableElement> &topology,
                                    std::vector<RunnableElement> *sorted_topology,
                                    const std::unordered_map<std::string, std::string> &factories,
                                    std::unordered_set<std::string> *parallel_modules) {
    
    /*
    ::google::protobuf::RepeatedPtrField<proto::RunnableTopology_Element> converted;
    for (auto & e : topology) {
        auto * one = converted.Add();
        one->set_name(e.name);
        for (auto & d : e.dependencies) {
            one->add_dependency(d);
        }
    }
    */
    return true;// 先这样编译
    //return topology_sort(converted, sorted_topology, factories, parallel_modules);
}

/*
bool ModuleTopologyManager::topology_sort(
        const ::google::protobuf::RepeatedPtrField<proto::RunnableTopology_Element> &topology,
        std::vector<RunnableElement> *sorted_topology,
        const std::unordered_map<std::string, std::string>& factories,
        std::unordered_set<std::string> *parallel_modules) {
    std::unordered_set<std::string> removed;
    std::vector<std::string> sync_modules;
    std::vector<std::string> async_modules;
    std::vector<std::string> sync_modules_allow_parallel;
    std::string factory_name;
    while (removed.size() != static_cast<size_t>(topology.size())) {
        bool zero_upstream_node_found = false;
        for (int i = 0; i < topology.size(); ++i) {
            if (removed.find(topology[i].name()) == removed.end()) {
                bool upstream_all_removed = true;
                for (int j = 0; j < topology[i].dependency().size(); ++j) {
                    if (removed.find(topology[i].dependency()[j]) == removed.end()) {
                        upstream_all_removed = false;
                        break;
                    }
                }

                if (upstream_all_removed) {
                    zero_upstream_node_found = true;
                    auto iter = factories.find(topology[i].name());
                    if (iter != factories.end()) {
                        factory_name = iter->second;
                    } else {
                        factory_name = "";
                    }
                    if (parallel_modules) {
                        if (is_sync_module(factory_name)) {
                            sync_modules.push_back(topology[i].name());
                            if (FLAGS_enable_check_allow_parallel_field && topology[i].allow_parallel()) {
                                sync_modules_allow_parallel.push_back(topology[i].name());
                            }
                        } else {
                            async_modules.push_back(topology[i].name());
                        }
                    } else {
                        removed.insert(topology[i].name());
                    }
                    if (sorted_topology) {
                        RunnableElement element;
                        element.name = topology[i].name();
                        element.factory_name = std::move(factory_name);
                        element.dependencies = std::vector<std::string>(topology[i].dependency().begin(),
                                topology[i].dependency().end());
                        sorted_topology->push_back(element);
                    }
                }
            }
        }
        if (parallel_modules) {
            if (FLAGS_enable_check_allow_parallel_field) {
                parallel_modules->insert(sync_modules_allow_parallel.begin(), sync_modules_allow_parallel.end());
            } else if (sync_modules.size() > 1) {
                parallel_modules->insert(sync_modules.begin(), sync_modules.end());
            }
            removed.insert(sync_modules.begin(), sync_modules.end());
            removed.insert(async_modules.begin(), async_modules.end());
            sync_modules.clear();
            async_modules.clear();
            sync_modules_allow_parallel.clear();
        }

        if (!zero_upstream_node_found) {
            return false;
        }
    }

    return true;
}
*/

bool
ModuleTopologyManager::has_exactly_global_module(const std::string &name,
        const std::string &factory_name,
        const std::unordered_map<std::string, std::string>& module_params) {
    auto iter = _global_modules.find(name);
    if (iter == _global_modules.end()) {
        return false;
    }
    if (iter->second.factory_name() != factory_name) {
        return false;
    }

    if (static_cast<int>(module_params.size()) != iter->second.custom_params_size()) {
        return false;
    }
    std::unordered_map<std::string, std::string> stored_params;
    for (const auto & p : iter->second.custom_params()) {
        stored_params[p.key()] = p.value();
    }

    return stored_params == module_params;
}

bool ModuleTopologyManager::add_module_if_not_exist(const std::string &name, const std::string &factory) {
    if (has_exactly_global_module(name, factory, {})) {
        return true;
    }

    proto::Module module;
    module.set_name(name);
    module.set_factory_name(factory);

    return add_module(module);
}

bool ModuleTopologyManager::add_module(const proto::Module &module) {
    if (_app_initialized) {
        TERR("Dynamically add module after app is initialized is not supported");
        return false;
    }
    if (!_app->get_runnable_module_factory_registry()->contain(module.factory_name().c_str())) {
        TERR("factory[%s] not found in runnable_module factory registry", module.factory_name().c_str());
        return false;
    }
    std::unordered_map<std::string, std::string> params;
    for (auto & i : module.custom_params()) {
        params.emplace(i.key(), i.value());
    }
    auto iter = _global_modules.emplace(module.name(), module);
    if (!iter.second) {
        TERR("module[%s] already exist", module.name().c_str());
        return false;
    }
    return true;
}

/*
添加拓扑 (add_topology)
流程：
模块定义校验：检查全局与本地模块是否冲突。
依赖关系校验：确保所有依赖的模块已定义。
生成排序结果：调用 topology_sort 生成有序模块列表。
创建会话数据池：为拓扑分配会话本地数据池。
更新双缓冲索引：替换或新增拓扑配置。
监控初始化：注册拓扑使用次数的监控指标。
*/
/*
bool ModuleTopologyManager::add_topology(const proto::TopologyConfigWithModuleDefinition& packed) {
    const auto &topology_name = packed.runnable_topology().name();

    // 1. module definitions
    // check redefinitions of topology-local module against global module definitions
    for (const auto & one_module : packed.module()) {
        auto iter = _global_modules.find(one_module.name());
        if (iter != _global_modules.end() && !Equal(iter->second, one_module)) {
            TERR("redefinition of module[%s] in topology[%s], another definition is in global scope",
                    one_module.name().c_str(), topology_name.c_str());
            return false;
        }
    }

    // check module definition uniqueness
    std::unordered_map<std::string, std::string> module_factories;
    for (const auto & one_module : packed.module()) {
        if (module_factories.find(one_module.name()) != module_factories.end()) {
            TERR("redefinition of module[%s] in topology[%s], another definition is in topology scope",
                 one_module.name().c_str(), topology_name.c_str());
            return false;
        }
        module_factories[one_module.name()] = one_module.factory_name();
    }
    for (const auto & m : _global_modules) {
        module_factories.emplace(m.first, m.second.factory_name());
    }

    // module params
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> module_params;
    for (const auto &one_module : packed.module()) {
        for (const auto &p : one_module.custom_params()) {
            module_params[one_module.name()][p.key()] = p.value();
        }
    }
    for (const auto & m : _global_modules) {
        std::unordered_map<std::string, std::string> params;
        for (const auto & p : m.second.custom_params()) {
            params[p.key()] = p.value();
        }
        module_params.emplace(m.first, std::move(params));
    }

    // 2. element reference
    // check there is at least one element in topology
    if (packed.runnable_topology().element_size() <= 0) {
        TERR("no elements found in topology[%s]", topology_name.c_str());
        return false;
    }

    // check element defined and unique
    std::unordered_map<std::string, int> module_name_set;
    for (const auto &element : packed.runnable_topology().element()) {
        if (module_factories.find(element.name()) == module_factories.end()) {
            TERR("element[%s] of topology[%s] not found in module definitions", element.name().c_str(),
                    topology_name.c_str());
            return false;
        }
        module_name_set[element.name()]++;
        if (module_name_set[element.name()] > 1) {
            TERR("Duplicated module[%s] found in topology[%s]", element.name().c_str(), topology_name.c_str());
            return false;
        }
    }

    // check element dependency existence
    for (const auto &element : packed.runnable_topology().element()) {
        for (const auto &dep : element.dependency()) {
            if (module_name_set.find(dep) == module_name_set.end()) {
                TERR("dependency[%s] of module[%s] not found in topology[%s]", dep.c_str(), element.name().c_str(),
                     topology_name.c_str());
                return false;
            }
        }
    }

    // element params
    for (const auto & e : packed.runnable_topology().element()) {
        for (const auto & p : e.custom_params()) {
            module_params[e.name()][p.key()] = p.value();
        }
    }

    // 3. topology related
    // check cycle in topology
    std::vector<RunnableElement> sorted_topology;
    std::unordered_set<std::string> parallel_modules;
    bool sort_result;
    if (FLAGS_enable_parallel_module_scheduling) {
        sort_result = topology_sort(packed.runnable_topology().element(), &sorted_topology,
                                    module_factories, &parallel_modules);
    } else {
        sort_result = topology_sort(packed.runnable_topology().element(), &sorted_topology, module_factories);
    }
    if (!sort_result) {
        TERR("loop found in topology[%s]", topology_name.c_str());
        return false;
    }
    for (auto & e : sorted_topology) {
        auto iter = module_params.find(e.name);
        if (iter != module_params.end()) {
            e.module_params.swap(iter->second);
        }
    }

    std::unordered_map<std::string, std::string> topology_params;
    for (const auto & p : packed.runnable_topology().custom_params()) {
        topology_params[p.key()] = p.value();
    }

    // declare using_count monitor, reuse if there is already one
    if (!_storage->EmplaceTopologyMonitor(topology_name)) {
        TERR("expose TopologyUsingCountMonitor for topology[%s] failed", topology_name.c_str());
        return false;
    }

    std::unique_ptr<ModuleTopology> module_topology(new (std::nothrow)ModuleTopology());
    module_topology->name = topology_name;
    module_topology->desc.swap(sorted_topology);
    module_topology->parallel_modules.swap(parallel_modules);
    module_topology->custom_params.swap(topology_params);

    SessionLocalDataFactoryArgument argument;
    argument.app = _app;
    argument.topology_name = topology_name;
    argument.module_topology = module_topology.get();
    // create, test, then reserve. failed ones will not be inserted into internal storage.
    std::unique_ptr<SessionLocalDataPool> pool(CreateSessionLocalDataPool(topology_name,
            _app->session_local_data_factory(), &argument, _app_initialized));
    if (!pool) {
        TERR("create SessionLocalDataPool for topology[%s] failed", topology_name.c_str());
        return false;
    }
    module_topology->own_session_local_data_pool(pool.release());

    std::unordered_set<ModuleTopology *> replaced;
    auto * p_module_topology = module_topology.get();
    std::function<size_t(Index &)> modifier = [&topology_name, p_module_topology, &replaced] (Index &list) {
        // You can add duplicated topology repeatedly, previous `add`s are overwritten
        return list.replace(topology_name, p_module_topology, &replaced);
    };
    _dbd.Modify(modifier);
    _storage->Add(module_topology.release());
    TDEBUG("add topology[%s] with %d elements successfully", topology_name.c_str(),
            packed.runnable_topology().element_size());
    wait_and_delete(replaced);
    return true;
}*/

bool ModuleTopologyManager::add_topology(const std::string &name, const std::vector<RunnableElement> &desc,
        std::unordered_map<std::string, std::string> && topology_params) {
    proto::TopologyConfigWithModuleDefinition packed;
    auto *topology = packed.mutable_runnable_topology();
    topology->set_name(name);
    for (auto& p : topology_params) {
        auto *param = topology->add_custom_params();
        param->set_key(p.first);
        param->set_value(p.second);
    }
    for (const auto & e : desc) {
        auto * m = packed.add_module();
        m->set_name(e.name);
        m->set_factory_name(e.factory_name);
        for (const auto & p : e.module_params) {
            auto *param = m->add_custom_params();
            param->set_key(p.first);
            param->set_value(p.second);
        }
    }
    for (const auto & e : desc) {
        auto *element = topology->add_element();
        element->set_name(e.name);
        for (const auto & d : e.dependencies) {
            element->add_dependency(d);
        }
    }

    return true;// 下面先注释编译....
    // return add_topology(packed);
}

// replace/remove的主要思路是：
// （1）把ModuleTopology变成一个instance local的变量，允许两个同名的ModuleTopology同时存在。
// ModuleTopology内部不可以包含对任意维全局变量的写操作，如果有，则必须保证线程安全和正确的生命周期。
// 特别的，ModuleTopology上有对session_local_data_count统计量的declare是线程安全的。ModuleTopology
// 中具体实例化的module instance（这部分是app中的user code）中通常包含的对统计量的declare是线程安全的；
// 通常这部分user code中不应该包含对全局变量的写操作。
// （2）ModuleTopologyManager维持对请求线程的视图，保证拓扑切换、删除、添加等是原子性的；对于拓扑及其相关
// instance local的资源销毁则交给ModuleStorage。拓扑的切换和拓扑的释放是两个独立的过程，没有依赖关系，
// 也不需要有锁来保证无并发等。拓扑释放时会等到请求中对其的引用计数为0。

/*
删除拓扑 (remove_topology 和 wait_and_delete)
逻辑：
从索引中移除拓扑，并记录待删除的实例。
异步等待引用计数归零后，释放资源（避免并发访问问题）。
*/
void ModuleTopologyManager::remove_topology(const std::string& name) {
    // TODO(ziqian): default topology must not be removed
    std::unordered_set<ModuleTopology *> removed;
    auto modifier = [&name, &removed] (Index &list) {
        return list.remove(name, &removed);
    };
    if (1lu == _dbd.Modify(modifier)) {
        TDEBUG("topology[%s] removed successfully", name.c_str());
    } else {
        TWARN("topology[%s] does not exist", name.c_str());
    }

    wait_and_delete(removed);
}

void ModuleTopologyManager::wait_and_delete(const std::unordered_set<ModuleTopology *>& to_delete) {
    for (auto *topology : to_delete) {
        const std::string topology_name = topology->name;
        // spin waiting reference count decrease to zero
        while (topology->reference_count() > 0) {}
        _storage->Delete(topology);
        TDEBUG("topology[%s] was deleted", topology_name.c_str());
    }
}

void ModuleTopologyManager::clear() {
    std::unordered_set<ModuleTopology *> removed;
    /*
    auto modifier = [&removed](Index &list) {
        const size_t size = list.size();
        list.clear(&removed);
        return size;
    };
    const auto n_removed = _dbd.Modify(modifier);
    */
    wait_and_delete(removed);
    assert(_storage->num_topologies() == 0);
}
}  // namespace sw
