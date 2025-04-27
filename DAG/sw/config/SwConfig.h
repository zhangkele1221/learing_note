#ifndef RED_SEARCH_WORKER_SW_CONFIG_SWCONFIG_H_
#define RED_SEARCH_WORKER_SW_CONFIG_SWCONFIG_H_

#include <map>
#include "putil/legacy/jsonizable.h"

namespace sw {

class SwConfig : public putil::legacy::Jsonizable {
public:
    void Jsonize(putil::legacy::Jsonizable::JsonWrapper& json) {
        json.Jsonize("custom_config", _customConfig, {});
    }

    const std::map<std::string, std::string> & getCustomConfig() const {
        return _customConfig;
    }

private:
    std::map<std::string, std::string> _customConfig;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_CONFIG_SWCONFIG_H_
