#include "AppLoader.h"
#include "Log.h"



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
