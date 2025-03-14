#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_SWAPP_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_SWAPP_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "app_interface/Component.h"
#include "app_interface/Context.h"
#include "app_interface/DataFactory.h"
#include "app_interface/DlModuleInterface.h"
#include "app_interface/ReloadableModule.h"
#include "app_interface/RunnableModule.h"
#include "cppcommon/registry/registry.h"

namespace sw {
class SwApp : public IBaseInterface {
    DECLARE_UUID(0x36efca13, 0xb4f3, 0x456f, 0x81f5, 0x4a7df90db94e)

public:
    virtual ~SwApp() {}

    /**
     * 在app初始化时仅且调用一次。传入的是组件列表，比如日志组件、监控组件、配置组件、索引组件、小字典组件等
     * @return
     */
    virtual bool init(std::unordered_map<std::string, Component*>*) = 0;

    /**
     * 获取app内部实现的session_local_data的factory。SW会调用它来动态的或reserved生成session_local_data，并填充给context
     * 返回的DataFactory由app自行释放，SW不负责释放。
     * @return
     */
    virtual DataFactory* session_local_data_factory() {
        return nullptr;
    }

    /**
     * 获取app内部的runnable_module的factory的registry。SW会在其中查找相应的module，然后调用factory生成对象。
     * 返回的Registry由app自行释放，SW不负责释放。但SW调用factory生成的对象，SW负责调用factory的destroy
     */
    virtual red_search_cppcommon::FactoryRegistry<RunnableModule>*
    get_runnable_module_factory_registry() = 0;

    /**
     * 获取reloadable_module实例的注册中心。SW框架会调用该接口来获取APP的所有的reloadable_module实例，这些
     * 实例归属于app，SW框架仅仅只是引用，并不负责管理释放。
     */
    virtual red_search_cppcommon::ElementRegistry<ReloadableModule>* get_reloadable_module_registry() = 0;

    /**
     * 加载/卸载so
     *
     * 暂未实现，且接口有可能变更
     */
    virtual bool loadBiz(const std::string& bizName, const std::string& localDataPath) {  // NOLINT
        return true;
    }
    virtual bool unloadBiz(const std::string& bizName) {  // NOLINT
        return true;
    }
    virtual bool updateBiz(const std::string& tableName) {  // NOLINT
        return true;
    }

    /**
     * return 0 means healthy, unhealthy otherwise. Note this "health" is the same with that of pns and am.
     */
    virtual int check_health() const {
        return 0;
    }

    /**
     * string will be set to pns meta, key is __PNS_WORKER_TAG, value should be like
     * "tablenameA:md5A;tablenameB:md5B" message will be serialized to json and set to pns meta, key is
     * __PNS_WORKER_META this func is thread safe caller will only read message, we suggest you return the
     * addr of a class member
     */
    virtual const std::pair<std::string, ::google::protobuf::Message*> get_pns_meta() {
        return std::make_pair<std::string, ::google::protobuf::Message*>("", nullptr);
    }
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_SWAPP_H_
