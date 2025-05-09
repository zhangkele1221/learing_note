// app_sdk/ModuleRegistry.cpp
#include "ModuleRegistry.h"

namespace sw {
// 正确定义静态成员
ModuleRegistry* ModuleRegistry::_instance = nullptr;

/*
ModuleRegistry* ModuleRegistry::instance() {
    if (!_instance) {
        _instance = new ModuleRegistry();
    }
    return _instance;
}
*/

}  // namespace sw
