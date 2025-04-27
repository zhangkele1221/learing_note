#ifndef RED_SEARCH_WORKER_SW_SERVER_APPLOADER_H_
#define RED_SEARCH_WORKER_SW_SERVER_APPLOADER_H_

#include "DlModuleInterface.h"
#include <string>

namespace sw {

class AppLoader {
public:
    typedef const char * (*GetName)();
    typedef const char * (*GetVersion)();
    typedef sw::IBaseInterface *(*CreateInterfaceFuncType)(uint64_t, uint64_t);
    typedef void (*DestroyInterfaceFuncType)(sw::IBaseInterface *);

    virtual ~AppLoader() {}
    virtual bool load(const char *) = 0;
    virtual sw::IBaseInterface* create(uint64_t uuid_high, uint64_t uuid_low) = 0;
    virtual void destroy(sw::IBaseInterface* p) = 0;

    virtual const std::string& name() const = 0;
    virtual const std::string& version() const = 0;
};

class StaticLoader : public AppLoader {
public:
    bool load(const char *) override;
    sw::IBaseInterface* create(uint64_t uuid_high, uint64_t uuid_low) override;
    void destroy(sw::IBaseInterface* p) override;

    const std::string& name() const override { return _name; }
    const std::string& version() const override { return _version; }
private:
    CreateInterfaceFuncType _creator;
    DestroyInterfaceFuncType _destroyer;
    std::string _version;
    std::string _name;
};

typedef StaticLoader SwAppLoader;

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_SERVER_APPLOADER_H_
