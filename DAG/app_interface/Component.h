#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_COMPONENT_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_COMPONENT_H_

namespace sw {

enum ComponentType {
    MONITOR_COMPONENT = 1,
    LOG_COMPONENT,
    CONFIG_COMPONENT,
    INDEX_COMPONENT,
    RPC_COMPONENT,
    NAMING_SERVICE_COMPONENT,
};

class Component {
public:
    virtual ~Component() {}

    virtual ComponentType get_component_type() const = 0;
};

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_COMPONENT_H_
