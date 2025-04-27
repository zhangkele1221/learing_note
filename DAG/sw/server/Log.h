#ifndef RED_SEARCH_WORKER_SW_UTIL_LOG_H_
#define RED_SEARCH_WORKER_SW_UTIL_LOG_H_

#include <iostream>  // 仅保留标准库依赖

namespace sw {

// 基础日志实现
#define TLOG(fmt, ...) \
    std::cout << "[INFO] " << __FILE__ << ":" << __LINE__ << " " << fmt << "\n"

#define TWARN(fmt, ...) \
    std::cerr << "[WARN] " << __FILE__ << ":" << __LINE__ << " " << fmt << "\n"

#define TERR(fmt, ...) \
    std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " " << fmt << "\n"

// 调试日志控制
#ifdef NDEBUG
#define TDEBUG(fmt, ...) void(0)
#else
#define TDEBUG(fmt, ...) \
    std::cout << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " " << fmt << "\n"
#endif

// 保持接口兼容性
#define DECLARE_AND_SETUP_LOGGER(n, c) /* 空实现 */

}  // namespace sw

#endif  // RED_SEARCH_WORKER_SW_UTIL_LOG_H_