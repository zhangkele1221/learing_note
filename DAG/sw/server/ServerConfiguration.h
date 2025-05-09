#ifndef RED_SEARCH_WORKER_SW_CONFIG_SERVERCONFIGURATION_H_
#define RED_SEARCH_WORKER_SW_CONFIG_SERVERCONFIGURATION_H_

#include "SwConfig.h"

namespace sw {

class ServerConfiguration {
#define DEFINE_PRELOAD_CONFIG_ITEM_2(_type_, _name_) \
public: \
    const _type_& _name_() const { \
        return _##_name_; \
    } \
private: \
    _type_ _##_name_;

#define DEFINE_PRELOAD_CONFIG_ITEM(_name_) DEFINE_PRELOAD_CONFIG_ITEM_2(std::string, _name_)

DEFINE_PRELOAD_CONFIG_ITEM(appSoPath)
DEFINE_PRELOAD_CONFIG_ITEM(serverMethodMaxConcurrency)
DEFINE_PRELOAD_CONFIG_ITEM(pnsEtcdAddress)
DEFINE_PRELOAD_CONFIG_ITEM(pnsEtcdPath)
DEFINE_PRELOAD_CONFIG_ITEM(catDomain)
DEFINE_PRELOAD_CONFIG_ITEM(matrixPartitionIndex)
DEFINE_PRELOAD_CONFIG_ITEM(matrixPartitionCount)
DEFINE_PRELOAD_CONFIG_ITEM(serviceName)

DEFINE_PRELOAD_CONFIG_ITEM_2(int, heartbeatServerPort)
DEFINE_PRELOAD_CONFIG_ITEM_2(int, serviceServerPort)

#undef DEFINE_PRELOAD_CONFIG_ITEM
#undef DEFINE_PRELOAD_CONFIG_ITEM_2

public:
    static ServerConfiguration * instance() {
        return &_instance;
    }

    bool load(const char * configFilePath);

    bool Get(const std::string&, std::string *, const std::string& environ_key = "");

    bool GetAppConfig(const std::string&, std::string *, const std::string& environ_key = "");

private:
    static ServerConfiguration _instance;
    SwConfig _swConfig;
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_CONFIG_SERVERCONFIGURATION_H_
