#include "sw/server/AppLoader.h"
#include "sw/util/Log.h"

#ifdef SW_APP_LINK_STATIC

extern const char * SW_APP_INTERFACE_KEYWORD_GET_NAME(void);
extern const char * SW_APP_INTERFACE_KEYWORD_GET_VERSION(void);
extern sw::IBaseInterface *SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE(uint64_t uuid_high, uint64_t uuid_low);
extern void SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE(sw::IBaseInterface *p);

namespace sw {

DECLARE_AND_SETUP_LOGGER(sw, StaticLoader);

bool StaticLoader::load(const char *) {
    // DEFINE_SW_APP("EchoServer", "0", sw::SwApp, sw::example::EchoApp);
    // 其实就是return reinterpret_cast<sw::IBaseInterface *>(dynamic_cast<_base_type_ *>(new (std::nothrow) _concrete_type_()));  
    _creator = SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE; //就是 上面的 return  DEFINE_SW_APP 这个宏函数 最后一个参数的  new好后的 指向该指针的对象的指针
    _destroyer = SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE;

    const char * name = SW_APP_INTERFACE_KEYWORD_GET_NAME();
    const char * version = SW_APP_INTERFACE_KEYWORD_GET_VERSION();

    if (name == nullptr || version == nullptr) {
        TERR("name[%p] or version[%p] is nullptr", name, version);
        return false;
    }

    _name = name;
    _version = version;

    TLOG("static linked module loaded, inner name is %s, inner version is %s",
         _name.c_str(), _version.c_str());
    return true;
}

sw::IBaseInterface* StaticLoader::create(uint64_t uuid_high, uint64_t uuid_low) {
    return _creator(uuid_high, uuid_low);
}

void StaticLoader::destroy(sw::IBaseInterface* p) {
    _destroyer(p);
}

}  // namespace sw
#else

#include <dlfcn.h>

namespace sw {

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

DECLARE_AND_SETUP_LOGGER(sw, CdlLoader);

CdlLoader::CdlLoader() : m_pHandle(nullptr), _creator(nullptr), _destroyer(nullptr) {
    int ret = pthread_rwlock_init(&m_module_rwlock, NULL);
    if (0 != ret) {
        TERR("CDlModule::CDlModule r/w lock init failed (%d).", ret);
    }
}

bool CdlLoader::load(const char *file) {
    if (!m_pHandle) {
        // not opened
        int ret = pthread_rwlock_wrlock(&m_module_rwlock);
        if (0 != ret) {
            TERR("CdlLoader::init can't get wrlock (%d).", ret);
            return false;
        }

        if (!m_pHandle) {
            m_pHandle = dlopen(file, RTLD_NOW | RTLD_GLOBAL);
            if (!m_pHandle) {
                pthread_rwlock_unlock(&m_module_rwlock);
                TERR("dlopen[%s] return %s", file, dlerror());
                return false;
            }

            const char * creator_name = TOSTRING(SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE);
            _creator = (CreateInterfaceFuncType)dlsym(m_pHandle, creator_name);
            if (!_creator) {
                pthread_rwlock_unlock(&m_module_rwlock);
                TERR("find interface[%s] from[%s] failed, return %s", creator_name, file, dlerror());
                return false;
            }

            const char * destroyer_name = TOSTRING(SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE);
            _destroyer = (DestroyInterfaceFuncType)dlsym(m_pHandle, destroyer_name);
            if (!_destroyer) {
                pthread_rwlock_unlock(&m_module_rwlock);
                TERR("find interface[%s] from[%s] failed, return %s", destroyer_name, file, dlerror());
                return false;
            }

            const char * name_getter = TOSTRING(SW_APP_INTERFACE_KEYWORD_GET_NAME);
            GetName GetSoNameFunc = (GetName)dlsym(m_pHandle, name_getter);
            if (!GetSoNameFunc) {
                pthread_rwlock_unlock(&m_module_rwlock);
                TERR("find interface[%s] from[%s] failed, return %s", name_getter, file, dlerror());
                return false;
            }
            const char* name = GetSoNameFunc();

            const char * version_getter = TOSTRING(SW_APP_INTERFACE_KEYWORD_GET_VERSION);
            GetVersion GetSoVersionFunc = (GetVersion)dlsym(m_pHandle, version_getter);
            if (!GetSoVersionFunc) {
                pthread_rwlock_unlock(&m_module_rwlock);
                TERR("find interface[%s] from[%s] failed, return %s", version_getter, file, dlerror());
                return false;
            }
            const char* version = GetSoVersionFunc();

            if (name == nullptr || version == nullptr) {
                TERR("name[%p] or version[%p] is nullptr", name, version);
                return false;
            }

            _name = name;
            _version = version;
            TLOG("dynamic module %s loaded, inner name is %s, inner version is %s", file,
                 _name.c_str(), _version.c_str());
        }

        pthread_rwlock_unlock(&m_module_rwlock);
        return true;
    }

    return false;
}

CdlLoader::~CdlLoader() {
    int ret = pthread_rwlock_wrlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::~CDlModule can't get wrlock (%d).", ret);
    }

    _creator  = NULL;
    _destroyer = NULL;
    if (m_pHandle) {
        dlclose(m_pHandle);
        m_pHandle = NULL;
    }

    ret = pthread_rwlock_unlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::~CDlModule unlock wrlock failed (%d).", ret);
    }

    ret = pthread_rwlock_destroy(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::~CDlModule destroy wrlock failed (%d).", ret);
    }
}

sw::IBaseInterface* CdlLoader::create(uint64_t uuid_high, uint64_t uuid_low) {
    int ret = pthread_rwlock_rdlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::create pthread_rwlock_rdlock failed.");
        return nullptr;
    }
    sw::IBaseInterface * p = nullptr;
    if (_creator) {
        p = _creator(uuid_high, uuid_low);
    }

    ret = pthread_rwlock_unlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::create pthread_rwlock_unlock failed.");
    }

    return p;
}

void CdlLoader::destroy(sw::IBaseInterface* p) {
    int ret = pthread_rwlock_rdlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::destroy pthread_rwlock_rdlock failed.");
        return;
    }

    if (_destroyer) {
        _destroyer(p);
    }

    ret = pthread_rwlock_unlock(&m_module_rwlock);
    if (0 != ret) {
        TERR("CDlModule::destroy pthread_rwlock_unlock failed.");
    }
}
}  // namespace sw
#endif
