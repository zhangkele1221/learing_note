#ifndef RED_SEARCH_WORKER_SW_APP_INTERFACE_DLMODULEINTERFACE_H_
#define RED_SEARCH_WORKER_SW_APP_INTERFACE_DLMODULEINTERFACE_H_

#include <cinttypes>

namespace sw {

// to create uuid , execute the follow script:
// uuidgen | awk -F "-" '{print "DECLARE_UUID(0x" $1 ,  ", 0x" $2 ", 0x" $3 ", 0x" $4 ", 0x" $5")"}'
#if defined(__LP64__) || defined(__64BIT__) || defined(_LP64) || (__WORDSIZE == 64)
#define DECLARE_UUID(u1, u2, u3, u4, u5)                                                                    \
public:                                                                                                     \
    static const uint64_t uuid_high = (uint64_t(u1) << 32) | (uint64_t(u2) << 16) | (uint64_t(u3));         \
    static const uint64_t uuid_low = (uint64_t(u4) << 48) | (uint64_t(u5));
#else
#define DECLARE_UUID(u1, u2, u3, u4, u5)                                                                    \
public:                                                                                                     \
    static const uint64_t uuid_high = (uint64_t(u1) << 32) | (uint64_t(u2) << 16) | (uint64_t(u3));         \
    static const uint64_t uuid_low = (uint64_t(u4) << 48) | (uint64_t(u5##ULL));
#endif

class IBaseInterface {
public:
    typedef int RETURN_VALUE;
    static const int SUCCEED = 0;

public:
    IBaseInterface() {}
    virtual ~IBaseInterface() {}
};
}  // namespace sw

#define SW_APP_INTERFACE_KEYWORD_GET_NAME          GetAppName
#define SW_APP_INTERFACE_KEYWORD_GET_VERSION       GetAppVersion
#define SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE  CreateInterface
#define SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE DestroyInterface

#ifndef SW_APP_LINK_STATIC
#define DEFINE_SW_APP(_name_, _version_, _base_type_, _concrete_type_)                                      \
    extern "C" const char *SW_APP_INTERFACE_KEYWORD_GET_NAME(void) { return _name_; }                       \
    extern "C" const char *SW_APP_INTERFACE_KEYWORD_GET_VERSION(void) { return _version_; }                 \
    extern "C" sw::IBaseInterface *SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE(uint64_t uuid_high,            \
                                                                             uint64_t uuid_low) {           \
        if (_base_type_::uuid_high == uuid_high && (_base_type_::uuid_low == uuid_low)) {                   \
            return reinterpret_cast<sw::IBaseInterface *>(                                                  \
                dynamic_cast<_base_type_ *>(new (std::nothrow) _concrete_type_()));                         \
        } else {                                                                                            \
            return NULL;                                                                                    \
        }                                                                                                   \
    }                                                                                                       \
    extern "C" void SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE(sw::IBaseInterface *p) { delete p; }

#else
#define DEFINE_SW_APP(_name_, _version_, _base_type_, _concrete_type_)                                      \
    const char *SW_APP_INTERFACE_KEYWORD_GET_NAME(void) { return _name_; }                                  \
    const char *SW_APP_INTERFACE_KEYWORD_GET_VERSION(void) { return _version_; }                            \
    sw::IBaseInterface *SW_APP_INTERFACE_KEYWORD_CREATE_INTERFACE(uint64_t uuid_high, uint64_t uuid_low) {  \
        if (_base_type_::uuid_high == uuid_high && (_base_type_::uuid_low == uuid_low)) {                   \
            return reinterpret_cast<sw::IBaseInterface *>(                                                  \
                dynamic_cast<_base_type_ *>(new (std::nothrow) _concrete_type_()));                         \
        } else {                                                                                            \
            return NULL;                                                                                    \
        }                                                                                                   \
    }                                                                                                       \
    void SW_APP_INTERFACE_KEYWORD_DESTROY_INTERFACE(sw::IBaseInterface *p) { delete p; }

#endif

#endif  // RED_SEARCH_WORKER_SW_APP_INTERFACE_DLMODULEINTERFACE_H_
