#include "sw/config/ServerConfiguration.h"
#include "sw/util/file_util.h"
#include "sw/util/Log.h"
#include "putil/EnvironUtil.h"
#include "gflags/gflags.h"

DEFINE_string(service_name, "", "service name");
DECLARE_string(cat_domain);
DEFINE_bool(enable_cat_monitor, true, "enable cat monitor. Note that this affects BvarCatDumper, AlogCatAppender "
                                      "and BvarCatExtension");

namespace sw {
DECLARE_AND_SETUP_LOGGER(sw, ServerConfiguration);

template <typename T>
bool parse_value(const std::string&, T*) {
    return false;
}

template <>
bool parse_value(const std::string& raw, int* value) {
    try {
        *value = std::stoi(raw);
        return true;
    } catch (...) {
        return false;
    }
}

template <>
bool parse_value(const std::string& raw, bool *value) {
    if (raw == "true") {
        *value = true;
        return true;
    } else if (raw == "false") {
        *value = false;
        return true;
    }

    return false;
}

template <>
bool parse_value(const std::string& raw, float *value) {
    try {
        *value = std::stof(raw);
        return true;
    } catch (...) {
        return false;
    }
}


template <>
bool parse_value(const std::string& raw, int64_t *value) {
    try {
        *value = std::stol(raw);
        return true;
    } catch (...) {
        return false;
    }
}

template <>
bool parse_value(const std::string& raw, std::string *value) {
    *value = raw;
    return true;
}

ServerConfiguration ServerConfiguration::_instance;

bool ServerConfiguration::load(const char * configFilePath) {
    // leo&cat domain
    _catDomain = FLAGS_cat_domain;
    _serviceName = FLAGS_service_name;
    if (_serviceName.empty()) {
        _serviceName = putil::EnvironUtil::getEnv("SERVICE_NAME", "");
    }
    
    if (_serviceName.empty()) {
        TERR("_serviceName is empty, please specify it using gflags FLAGS_service_name or setenv SERVICE_NAME");
        return false;
    }

    if (FLAGS_enable_cat_monitor) {
        if (_catDomain.empty()) {
            TERR("CatDomain is empty, please specify it using gflags FLAGS_cat_domain");
            return false;
        }
    }

    // local config
    if (!FileUtil::fileExist(configFilePath)) {
        TERR("config path [%s] does not exist!", configFilePath);
        return false;
    }
    std::string configJsonStr;
    FileUtil::readFileContent(configFilePath, configJsonStr);
    try {
        FromJsonString(_swConfig, configJsonStr);
    } catch (std::exception &e) {
        TERR("std::exception caught [%s] when parsing local config", e.what());
        return false;
    } catch (...) {
        TERR("unknown throw caught when parsing local config");
        return false;
    }

#define OPTIONAL_PRELOAD_4(_variable_, _path_, _env_key_, _default_value_) \
    do { \
        std::string tmp; \
        if (GetAppConfig(_path_, &tmp, _env_key_) && parse_value(tmp, &_variable_)) { \
            break; \
        } \
        _variable_ = _default_value_; \
    } while (0)

    OPTIONAL_PRELOAD_4(_pnsEtcdAddress, "pns.etcd.address", "", "");
    OPTIONAL_PRELOAD_4(_pnsEtcdPath, "pns.etcd.path", "", "");
    OPTIONAL_PRELOAD_4(_appSoPath, "app.so_path", "", "");

    OPTIONAL_PRELOAD_4(_serverMethodMaxConcurrency, "server.method.max_concurrency", "", "unlimited");
    OPTIONAL_PRELOAD_4(_heartbeatServerPort, "server.heartbeat_server_port", "HEARTBEAT_SERVER_PORT", 8890);
    OPTIONAL_PRELOAD_4(_serviceServerPort, "server.service_server_port", "SERVICE_SERVER_PORT", 8899);

#undef OPTIONAL_PRELOAD_4

    return true;
}

// *.rpc.*.config used by AsyncRpcModuleWrapper
bool ServerConfiguration::Get(const std::string& name, std::string * value, const std::string &environ_key) {
    // env
    const std::string& environ_name = environ_key.empty() ? name : environ_key;
    *value = putil::EnvironUtil::getEnv(environ_name);
    if (!value->empty()) {
        TDEBUG("config[%s] loaded from Environ[%s], value is %s", name.c_str(), environ_name.c_str(), value->c_str());
        return true;
    }

    // local config
    const auto &local = _swConfig.getCustomConfig();
    auto iter = local.find(name);
    if (iter != local.end()) {
        *value = iter->second;
        TDEBUG("config[%s] loaded from LocalCustomConfig, value is %s", name.c_str(), value->c_str());
        return true;
    }

    TWARN("config[%s] not found", name.c_str());
    return false;
}

bool ServerConfiguration::GetAppConfig(const std::string &name, std::string *value, const std::string &environ_key) {
    return Get(_serviceName + "." + name, value, environ_key);
}
}  // namespace sw
