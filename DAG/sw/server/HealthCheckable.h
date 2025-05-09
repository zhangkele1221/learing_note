#ifndef RED_SEARCH_WORKER_SW_INTERFACE_HEALTHCHECKABLE_H_
#define RED_SEARCH_WORKER_SW_INTERFACE_HEALTHCHECKABLE_H_

namespace sw {

class HealthCheckable {
 public:
    virtual ~HealthCheckable() {}

    virtual bool checkHealth() const = 0;

    virtual const std::pair<std::string, std::string> getPnsMeta() const = 0;
};
}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_INTERFACE_HEALTHCHECKABLE_H_
