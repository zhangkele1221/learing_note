#ifdef private
# undef private
# include <sstream>
# define private public
#else
# include <sstream>
#endif

#include "gtest/gtest.h"

#include "gmock/gmock.h"
#include "alog/Configurator.h"
#include "app_interface/SwApp.h"
#include "app_interface/SyncModule.h"
#include "app_interface/AsyncRpcModule.h"
#include "red_search_cppcommon/registry/factory_registry.h"
#include "red_search_cppcommon/registry/element_registry.h"
#include "putil/Thread.h"
#include "sw/config/ServerConfiguration.h"
#include "sw/server/ModuleTopologyManager.h"
#include "sw/server/SessionLocalData.h"
#include "sw/server/SequentialScheduler.h"

namespace sw {
DECLARE_int32(session_local_data_reserved_count);

class ModuleTopologyManagerTest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        alog::Configurator::configureRootLogger();
        alog::Logger::getRootLogger()->setLevel(alog::LOG_LEVEL_TRACE3);
    }

protected:
    virtual void SetUp() {}

    virtual void TearDown() {}
};

class MockSyncModule : public SyncModule {
public:
    MOCK_METHOD2(init, bool(sw::SwApp*, RequestContext*));
    MOCK_METHOD1(run, RunStatus(RequestContext*));
    MOCK_METHOD0(reset, void());
    MOCK_METHOD0(is_skipped, bool());
};

class MockAsyncRpcModule : public AsyncRpcModule {
public:
    MOCK_METHOD2(build_request, ::google::protobuf::Message*(int, int));
    MOCK_METHOD2(create_or_get_response, ::google::protobuf::Message*(int, int));
    MOCK_METHOD1(parse_response, void(int));
    MOCK_METHOD2(init, bool(sw::SwApp*, RequestContext*));
    MOCK_METHOD1(continue_run, bool(sw::RequestContext*));
    MOCK_METHOD0(reset, void());
    MOCK_METHOD0(is_skipped, bool());
};
red_search_cppcommon::FactoryRegistryImpl<RunnableModule> g_module_registry;
REGISTER_FACTORY(&g_module_registry, mock_sync_module, MockSyncModule);
REGISTER_FACTORY(&g_module_registry, mock_async_rpc_module, MockAsyncRpcModule);
red_search_cppcommon::ElementRegistryImpl<ReloadableModule> g_reloadable_module_registry;

class MockApp : public SwApp {
public:
    MOCK_METHOD1(init, bool(std::unordered_map<std::string, Component *> *));

    red_search_cppcommon::FactoryRegistry<RunnableModule>* get_runnable_module_factory_registry() override {
        return &g_module_registry;
    }

    red_search_cppcommon::ElementRegistry<ReloadableModule>* get_reloadable_module_registry() override {
        return &g_reloadable_module_registry;
    }
};

TEST_F(ModuleTopologyManagerTest, test_parallel_module_selection) {
    MockApp mock_app;
    ModuleTopologyManager manager;

    manager.set_app(&mock_app);
    ASSERT_TRUE(manager.is_sync_module("mock_sync_module"));
    ASSERT_FALSE(manager.is_sync_module("mock_async_rpc_module"));

    std::unordered_map<std::string, std::string> module_factories;
    module_factories.emplace("module_1", "mock_sync_module");
    module_factories.emplace("module_2", "mock_sync_module");
    module_factories.emplace("module_3", "mock_sync_module");
    module_factories.emplace("module_4", "mock_sync_module");
    module_factories.emplace("module_5", "mock_sync_module");
    module_factories.emplace("module_6", "mock_sync_module");
    module_factories.emplace("module_async", "mock_async_rpc_module");
    {
        std::unordered_set<std::string> parallel_modules;
        std::vector<RunnableElement> list_topology;
        list_topology.resize(3);
        list_topology[0] = {"module_1", "", {}};
        list_topology[1] = {"module_2", "", {"module_1"}};
        list_topology[2] = {"module_3", "", {"module_2"}};

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr, module_factories, &parallel_modules));
        ASSERT_EQ(0ul, parallel_modules.size());
    }

    {
        std::unordered_set<std::string> parallel_modules;
        std::vector<RunnableElement> list_topology;
        list_topology.resize(5);
        list_topology[0] = {"module_1", "", {}};
        list_topology[1] = {"module_2", "", {"module_1"}};
        list_topology[2] = {"module_3", "", {"module_1"}};
        list_topology[3] = {"module_4", "", {"module_1"}};
        list_topology[4] = {"module_5", "", {"module_2","module_3","module_4"}};

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr, module_factories, &parallel_modules));
        ASSERT_EQ(3ul, parallel_modules.size());
        EXPECT_EQ(parallel_modules, std::unordered_set<std::string>({"module_2","module_3","module_4"}));
    }

    // source parallel modules
    {
        std::unordered_set<std::string> parallel_modules;
        std::vector<RunnableElement> list_topology;
        list_topology.resize(5);
        list_topology[0] = {"module_1", "", {}};
        list_topology[1] = {"module_2", "", {"module_1"}};
        list_topology[2] = {"module_3", "", {}};
        list_topology[3] = {"module_4", "", {"module_2","module_3"}};
        list_topology[4] = {"module_5", "", {"module_4"}};

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr, module_factories, &parallel_modules));
        ASSERT_EQ(2ul, parallel_modules.size());
        EXPECT_EQ(parallel_modules, std::unordered_set<std::string>({"module_1","module_3"}));
    }

    // transitive dependency
    {
        std::unordered_set<std::string> parallel_modules;
        std::vector<RunnableElement> list_topology;
        list_topology.resize(6);
        list_topology[0] = {"module_1", "", {}};
        list_topology[1] = {"module_2", "", {"module_1"}};
        list_topology[2] = {"module_3", "", {"module_2"}};
        list_topology[3] = {"module_4", "", {"module_1","module_3"}};
        list_topology[4] = {"module_5", "", {"module_1"}};
        list_topology[5] = {"module_6", "", {"module_4","module_5"}};

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr, module_factories, &parallel_modules));
        ASSERT_EQ(2ul, parallel_modules.size());
        EXPECT_EQ(parallel_modules, std::unordered_set<std::string>({"module_2","module_5"}));
    }

    // brother node is async module
    {
        std::unordered_set<std::string> parallel_modules;
        std::vector<RunnableElement> list_topology;
        list_topology.resize(5);
        list_topology[0] = {"module_1", "", {}};
        list_topology[1] = {"module_2", "", {"module_1"}};
        list_topology[2] = {"module_async", "", {"module_1"}};
        list_topology[3] = {"module_4", "", {"module_1"}};
        list_topology[4] = {"module_5", "", {"module_2","module_async","module_4"}};

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr, module_factories, &parallel_modules));
        ASSERT_EQ(2ul, parallel_modules.size());
        EXPECT_EQ(parallel_modules, std::unordered_set<std::string>({"module_2","module_4"}));
    }
}


TEST_F(ModuleTopologyManagerTest, test_topological_sort) {
    ModuleTopologyManager manager;
    {
        std::vector<RunnableElement> empty_node_list;
        ASSERT_TRUE(manager.topology_sort(empty_node_list, nullptr));
    }

    {
        std::vector<RunnableElement> list_topology;
        list_topology.resize(3);
        list_topology[0] = {"0", "f0", {}};
        list_topology[1] = {"1", "f1", {"0"}};
        list_topology[2] = {"2", "f2", {"1"}};;

        ASSERT_TRUE(manager.topology_sort(list_topology, nullptr));
    }

    {
        std::vector<RunnableElement> flow_topology;
        flow_topology.resize(4);
        flow_topology[0] = {"0", "f0", {}};
        flow_topology[1] = {"1", "f1", {"0"}};
        flow_topology[2] = {"2", "f2", {"0"}};
        flow_topology[3] = {"3", "f3", {"1", "2"}};

        ASSERT_TRUE(manager.topology_sort(flow_topology, nullptr));
    }

    {
        std::vector<RunnableElement> two_sources_topology;
        two_sources_topology.resize(7);
        two_sources_topology[0] = {"0", "f0", {}};
        two_sources_topology[1] = {"1", "f1", {"0"}};
        two_sources_topology[2] = {"2", "f2", {"0"}};
        two_sources_topology[3] = {"3", "f3", {"1", "2"}};
        two_sources_topology[4] = {"4", "f4", {}};
        two_sources_topology[6] = {"6", "f6", {"5"}};
        two_sources_topology[5] = {"5", "f5", {"4"}};

        ASSERT_TRUE(manager.topology_sort(two_sources_topology, nullptr));
    }

    {
        std::vector<RunnableElement> cyclic_topology;
        cyclic_topology.resize(5);
        cyclic_topology[0] = {"0", "f0", {}};
        cyclic_topology[1] = {"1", "f1", {"0", "3"}};
        cyclic_topology[2] = {"2", "f2", {"1"}};
        cyclic_topology[3] = {"3", "f3", {"2"}};
        cyclic_topology[4] = {"4", "f4", {"3"}};

        ASSERT_FALSE(manager.topology_sort(cyclic_topology, nullptr));
    }

    {
        std::vector<RunnableElement> two_components_one_cycle_topology;
        two_components_one_cycle_topology.resize(9);
        two_components_one_cycle_topology[0] = {"0", "f0", {}};
        two_components_one_cycle_topology[1] = {"1", "f1", {"0"}};
        two_components_one_cycle_topology[2] = {"2", "f2", {"0"}};
        two_components_one_cycle_topology[3] = {"3", "f3", {"1", "2"}};
        two_components_one_cycle_topology[5] = {"5", "f5", {}};
        two_components_one_cycle_topology[6] = {"6", "f6", {"5", "8"}};
        two_components_one_cycle_topology[7] = {"7", "f7", {"6"}};
        two_components_one_cycle_topology[8] = {"8", "f8", {"7"}};

        ASSERT_FALSE(manager.topology_sort(two_components_one_cycle_topology, nullptr));
    }

    {
        std::vector<RunnableElement> three_components_topology;
        three_components_topology.resize(9);
        three_components_topology[0] = {"0", "f0", {}};
        three_components_topology[1] = {"1", "f1", {"0"}};
        three_components_topology[2] = {"2", "f2", {"0"}};
        three_components_topology[3] = {"3", "f3", {"1", "2"}};
        three_components_topology[5] = {"5", "f5", {}};
        three_components_topology[6] = {"6", "f6", {"5"}};
        three_components_topology[7] = {"7", "f7", {"5"}};
        three_components_topology[8] = {"8", "f8", {"6", "7"}};

        ASSERT_TRUE(manager.topology_sort(three_components_topology, nullptr));
    }
}

void pick_topology(const std::unordered_map<std::string, ModuleTopology *>& m, std::string name, ModuleTopology ** output) {
    auto iter = m.find(name);
    if (iter != m.end()) {
        *output = iter->second;
    }
}

TEST_F(ModuleTopologyManagerTest, test_add_topology) {
    // test add_topology at startup
    std::vector<RunnableElement> topology_one;
    topology_one.push_back({"module_1", "mock_sync_module", {}});
    topology_one.push_back({"module_2", "mock_sync_module", {"module_1"}});
    topology_one.push_back({"module_3", "mock_sync_module", {"module_2"}});

    std::vector<RunnableElement> topology_two;
    topology_two.push_back({"module_1", "mock_sync_module", {}});
    topology_two.push_back({"module_2", "mock_sync_module", {"module_1"}});
    topology_two.push_back({"module_4", "mock_sync_module", {"module_2"}});

    MockApp mock_app;
    ModuleTopologyManager manager;

    manager.set_app(&mock_app);
    ASSERT_TRUE(manager.add_module_if_not_exist("module_1", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_2", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_3", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_4", "mock_sync_module"));
    // add duplicated module
    ASSERT_TRUE(manager.add_module_if_not_exist("module_4", "mock_sync_module"));
    // add not_exist module
    ASSERT_FALSE(manager.add_module_if_not_exist("module_5", "not_exist_module"));

    std::unordered_map<std::string, std::string> topology_params;
    ASSERT_TRUE(manager.add_topology("topology_one", topology_one, std::move(topology_params)));
    ASSERT_TRUE(manager.add_topology("topology_two", topology_two, std::move(topology_params)));

    ASSERT_TRUE(manager.contain("topology_one"));
    ASSERT_TRUE(manager.contain("topology_two"));

    ASSERT_TRUE(manager.add_topology("topology_one", topology_one, std::move(topology_params)));

    ModuleTopology * module_topology = nullptr;
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("not_exist_topology"), &module_topology));
    ASSERT_TRUE(module_topology == nullptr);

    // test scheduler setup
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("topology_one"), &module_topology));
    ASSERT_TRUE(module_topology != nullptr);
    EXPECT_EQ("topology_one", module_topology->name);
    auto * session_local_data = module_topology->session_local_data_pool()->Borrow();
    EXPECT_TRUE(session_local_data != nullptr);

    auto *scheduler = dynamic_cast<SequentialScheduler *>(session_local_data->scheduler);
    ASSERT_EQ(3, scheduler->_module_list.size());
    scheduler->_module_list[0]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[1]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[2]->status = RunningElement::Status::INITIALIZED;
    EXPECT_TRUE(nullptr != scheduler->next(nullptr));
    module_topology->session_local_data_pool()->Return(session_local_data);
}



TEST_F(ModuleTopologyManagerTest, test_add_topology_dynamically) {
    // test add_topology dynamically after startup
    std::vector<RunnableElement> topology_one;
    topology_one.push_back({"module_1", "mock_sync_module", {}});
    topology_one.push_back({"module_2", "mock_sync_module", {"module_1"}});
    topology_one.push_back({"module_3", "mock_sync_module", {"module_2"}});

    std::vector<RunnableElement> topology_two;
    topology_two.push_back({"module_1", "mock_sync_module", {}});
    topology_two.push_back({"module_2", "mock_sync_module", {"module_1"}});
    topology_two.push_back({"module_4", "mock_sync_module", {"module_2"}});

    MockApp mock_app;
    ModuleTopologyManager manager;

    manager.set_app(&mock_app);
    ASSERT_TRUE(manager.add_module_if_not_exist("module_1", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_2", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_3", "mock_sync_module"));
    ASSERT_TRUE(manager.add_module_if_not_exist("module_4", "mock_sync_module"));
    // add duplicated module
    ASSERT_TRUE(manager.add_module_if_not_exist("module_4", "mock_sync_module"));
    // add not_exist module
    ASSERT_FALSE(manager.add_module_if_not_exist("module_5", "not_exist_module"));
    manager.set_app_initialized();

    std::unordered_map<std::string, std::string> topology_params;
    ASSERT_TRUE(manager.add_topology("topology_one", topology_one, std::move(topology_params)));
    ASSERT_TRUE(manager.add_topology("topology_two", topology_two, std::move(topology_params)));

    ASSERT_TRUE(manager.contain("topology_one"));
    ASSERT_TRUE(manager.contain("topology_two"));

    ASSERT_TRUE(manager.add_topology("topology_one", topology_one, std::move(topology_params)));

    ModuleTopology * module_topology = nullptr;
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("not_exist_topology"), &module_topology));
    ASSERT_TRUE(module_topology == nullptr);

    // test scheduler setup when dynamically added
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("topology_one"), &module_topology));
    ASSERT_TRUE(module_topology != nullptr);
    EXPECT_EQ("topology_one", module_topology->name);
    auto * session_local_data = module_topology->session_local_data_pool()->Borrow();
    EXPECT_TRUE(session_local_data != nullptr);

    auto *scheduler = dynamic_cast<SequentialScheduler *>(session_local_data->scheduler);
    ASSERT_EQ(3, scheduler->_module_list.size());
    scheduler->_module_list[0]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[1]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[2]->status = RunningElement::Status::INITIALIZED;
    EXPECT_TRUE(nullptr != scheduler->next(nullptr));
    module_topology->session_local_data_pool()->Return(session_local_data);
}

TEST_F(ModuleTopologyManagerTest, test_add_topology_with_local_module) {
    // test module params of local_module
    MockApp mock_app;
    ModuleTopologyManager manager;

    manager.set_app(&mock_app);
    {
        proto::Module module_1;
        module_1.set_name("module_1");
        module_1.set_factory_name("mock_sync_module");
        auto *param = module_1.add_custom_params();
        param->set_key("global_param");
        param->set_value("global_value");
        ASSERT_TRUE(manager.add_module(module_1));
    }
    manager.set_app_initialized();

    ASSERT_FALSE(manager.add_module_if_not_exist("module_2", "mock_sync_module"));

    {
        proto::TopologyConfigWithModuleDefinition packed;
        packed.mutable_runnable_topology()->set_name("topology_one");
        auto *topology_param = packed.mutable_runnable_topology()->add_custom_params();
        topology_param->set_key("topology_param_1");
        topology_param->set_value("topology_value_1");

        auto * module = packed.add_module();
        module->set_name("local_module_2");
        module->set_factory_name("mock_sync_module");
        auto *module_param = module->add_custom_params();
        module_param->set_key("local_param");
        module_param->set_value("local_value");

        module = packed.add_module();
        module->set_name("local_module_3");
        module->set_factory_name("mock_sync_module");

        auto * element = packed.mutable_runnable_topology()->add_element();
        element->set_name("module_1");
        module_param = element->add_custom_params();
        module_param->set_key("local_param_1");
        module_param->set_value("local_value_1");

        element = packed.mutable_runnable_topology()->add_element();
        element->set_name("local_module_2");
        element->add_dependency("module_1");

        element = packed.mutable_runnable_topology()->add_element();
        element->set_name("local_module_3");
        element->add_dependency("local_module_2");

        ASSERT_TRUE(manager.add_topology(packed));
    }

    ASSERT_TRUE(manager.contain("topology_one"));

    ModuleTopology * module_topology = nullptr;
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("not_exist_topology"), &module_topology));
    ASSERT_TRUE(module_topology == nullptr);

    // test scheduler setup
    ASSERT_TRUE(manager.ReadTopologyList(pick_topology, std::string("topology_one"), &module_topology));
    ASSERT_TRUE(module_topology != nullptr);
    EXPECT_EQ("topology_one", module_topology->name);

    const std::unordered_map<std::string, std::string> expected_params_of_topology = {
            {"topology_param_1", "topology_value_1"},
    };
    EXPECT_EQ(expected_params_of_topology, module_topology->custom_params);

    {
        ASSERT_EQ(3lu, module_topology->desc.size());
        std::unordered_map<std::string, std::string> module_params = {
                {"global_param", "global_value"},
                {"local_param_1", "local_value_1"},
        };
        EXPECT_EQ("module_1", module_topology->desc[0].name);
        EXPECT_EQ(module_params, module_topology->desc[0].module_params);
        module_params = {
                {"local_param", "local_value"},
        };
        EXPECT_EQ("local_module_2", module_topology->desc[1].name);
        EXPECT_EQ(module_params, module_topology->desc[1].module_params);
    }

    auto * session_local_data = module_topology->session_local_data_pool()->Borrow();
    EXPECT_TRUE(session_local_data != nullptr);

    auto *scheduler = dynamic_cast<SequentialScheduler *>(session_local_data->scheduler);
    ASSERT_EQ(3, scheduler->_module_list.size());
    scheduler->_module_list[0]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[1]->status = RunningElement::Status::INITIALIZED;
    scheduler->_module_list[2]->status = RunningElement::Status::INITIALIZED;
    EXPECT_TRUE(nullptr != scheduler->next(nullptr));
    module_topology->session_local_data_pool()->Return(session_local_data);
}

TEST_F(ModuleTopologyManagerTest, test_add_module) {
    {
        // add_module_if_not_exist
        MockApp app;
        ModuleTopologyManager manager;
        manager.set_app(&app);
        ASSERT_TRUE(manager.add_module_if_not_exist("m1", "mock_sync_module"));
        EXPECT_TRUE(manager.add_module_if_not_exist("m1", "mock_sync_module"));
    }

    {
        // add_module
        MockApp app;
        ModuleTopologyManager manager;
        manager.set_app(&app);
        proto::Module module;
        module.set_name("m1");
        module.set_factory_name("mock_sync_module");
        ASSERT_TRUE(manager.add_module(module));
        EXPECT_FALSE(manager.add_module(module));
    }
}

class ModuleTopologyManagerMultiThreadTest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        alog::Configurator::configureRootLogger();
        alog::Logger::getRootLogger()->setLevel(alog::LOG_LEVEL_TRACE3);
        FLAGS_session_local_data_reserved_count = 10;
    }

    ModuleTopologyManagerMultiThreadTest() : _manager_1_stop(0) {}

    static void pick_topology(const std::unordered_map<std::string, ModuleTopology *>& m, int i, ModuleTopology**t) {
        auto iter = m.find(std::string("test_") + std::to_string(i));
        if (iter != m.end()) {
            iter->second->increase_reference();
            *t = iter->second;
        }
    }

    void run_seek_topology(int id) {
        while (1) {
            int stopped = _manager_1_stop.load(std::memory_order_acquire);
            if (stopped == 1) {
                break;
            }
            EXPECT_TRUE(_manager_1.contain("test_default"));

            for (int i = 0; i < 30; ++i) {
                ModuleTopology *topology = nullptr;
                _manager_1.ReadTopologyList(ModuleTopologyManagerMultiThreadTest::pick_topology, i, &topology);
                ASSERT_TRUE(topology != nullptr);

                usleep(1000);
                const std::string read_topology_name = topology->name;
                printf("simulating request with topology[%s]\n", read_topology_name.c_str());
                topology->decrease_reference();
            }

            for (int i = 30; i < 60; ++i) {
                ModuleTopology *topology = nullptr;
                _manager_1.ReadTopologyList(ModuleTopologyManagerMultiThreadTest::pick_topology, i, &topology);
                if (topology) {
                    usleep(1000);
                    const std::string read_topology_name = topology->name;
                    printf("simulating request with topology[%s]\n", read_topology_name.c_str());
                    topology->decrease_reference();
                } else {
                    printf("request with not exist[test_%d]\n", i);
                }
            }
        }
        printf("request_thread [%d] exit\n", id);
    }

    void replace_topology(const std::string& name) {
        std::vector<RunnableElement> topology;
        RunnableElement element;
        element.name = "module_1";
        element.factory_name = "mock_sync_module";
        topology.push_back(element);
        std::unordered_map<std::string, std::string> topology_params;
        EXPECT_TRUE(_manager_1.add_topology(name, topology, std::move(topology_params)));
    }

    void run_replace_topology(int id) {
        while (1) {
            int stopped = _manager_1_stop.load(std::memory_order_acquire);
            if (stopped == 1) {
                break;
            }
            for (int i = 0; i < 60; ++i) {
                replace_topology(std::string("test_") + std::to_string(i));
                usleep(10000);
            }
        }
        printf("replace_thread [%d] exit\n", id);
    }

    void run_remove_topology(int id) {
        while (1) {
            int stopped = _manager_1_stop.load(std::memory_order_acquire);
            if (stopped == 1) {
                break;
            }
            for (int i = 30; i < 60; ++i) {
                _manager_1.remove_topology("test_" + std::to_string(i));
                usleep(10000);
            }
        }
        printf("remove_thread [%d] exit\n", id);
    }

protected:
    void SetUp() override {
        _manager_1.set_app(&_app);
        proto::Module module_1;
        module_1.set_name("module_1");
        module_1.set_factory_name("mock_sync_module");
        ASSERT_TRUE(_manager_1.add_module(module_1));

        _manager_1.set_app_initialized();

        ASSERT_TRUE(_manager_1.ValidateAndReserveSessionLocalDataOnStartup());
    }

    virtual void TearDown() {}

    MockApp _app;

    std::atomic_int _manager_1_stop;
    ModuleTopologyManager _manager_1;
};

TEST_F(ModuleTopologyManagerMultiThreadTest, test_multi_thread) {
    {
        // add initial topologies at startup
        replace_topology("test_default");
        for (int i = 0; i < 30; ++i) {
            replace_topology("test_" + std::to_string(i));
        }

        // Request threads that are using topologies
        std::vector<putil::ThreadPtr> request_thread_list;
        for (int i = 0; i < 30; ++i) {
            auto thread = putil::Thread::createThread(
                    std::bind(&ModuleTopologyManagerMultiThreadTest::run_seek_topology, this, i));
            printf("request_thread[%ld, %d] started\n", thread->getId(), i);
            request_thread_list.push_back(thread);
        }

        sleep(1);

        // Background threads that are add/replace topologies
        std::vector<putil::ThreadPtr> update_thread_list;
        for (int i = 0; i < 5; ++i) {
            auto thread = putil::Thread::createThread(
                    std::bind(&ModuleTopologyManagerMultiThreadTest::run_replace_topology, this, i));
            printf("replace_thread[%ld, %d] started\n", thread->getId(), i);
            update_thread_list.push_back(thread);
        }

        // Background threads that are removing topologies
        std::vector<putil::ThreadPtr> remove_thread_list;
        for (int i = 0; i < 5; ++i) {
            auto thread = putil::Thread::createThread(
                    std::bind(&ModuleTopologyManagerMultiThreadTest::run_remove_topology, this, i));
            printf("remove_thread[%ld, %d] started\n", thread->getId(), i);
            remove_thread_list.push_back(thread);
        }

        sleep(30);
        _manager_1_stop.store(1, std::memory_order_release);
    }
}
}
