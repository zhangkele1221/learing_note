#include <iostream>
#include <string>
//#include "client/linux/handler/exception_handler.h"
//#include "absl/debugging/failure_signal_handler.h"
//#include "alog/Configurator.h"
#include "gflags/gflags.h"
//#include "cppcommon/logging/log_initializer.h"
//#include "cppcommon/monitor/alog_cat_appender.h"
//#include "prpc/PnsClientWrapper.h"
#include "SearchWorker.h"
//#include "sw/util/file_util.h"
//#include "sw/config/AlogConfig.h"
#include "butil/file_util.h"


// NOTE: never make s_ncore extern const whose ctor seq against other
// compilation units is undefined.
const int64_t s_ncore = sysconf(_SC_NPROCESSORS_ONLN);

DEFINE_string(log, "", "log configuration path!");
DEFINE_string(config, "", "server configuration path!");

// NOTE: from some practical usage, abseil thread dump may affect output of minidump,
//       so temporarily turn if off as default.
// TODO(jinghai): need investigate further to give a better solution.
DEFINE_bool(enable_abseil_thread_dump, false, "abseil thread dump switch");
DEFINE_string(thread_dump_file_path, "/app/logs/sw_thread_dumps", "thread dump file path!");
DECLARE_bool(enable_cat_monitor);

namespace bthread {
DECLARE_int32(bthread_concurrency);
}  // namespace bthread

sw::SearchWorker* global_search_worker = NULL;
sigset_t           global_mask;
sigset_t           global_oldmask;

bool parseAndValidateArgs(int argc, char* argv[]) {
    // change defaults of flags
    bthread::FLAGS_bthread_concurrency = s_ncore + 1;
    gflags::SetUsageMessage("red_search_worker usage:");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (!FLAGS_log.empty()) {
        if (!sw::FileUtil::fileExist(FLAGS_log)) {
            std::cout << "log configuration file [" << FLAGS_log << "] is not exist!" << std::endl;
            return false;
        }
    }

    if (FLAGS_config.empty()) {
        std::cout << "server configuration is not specified!" << std::endl;
        return false;

    } else {
        if (!sw::FileUtil::fileExist(FLAGS_config)) {
            std::cout << "server configuration file [" << FLAGS_config << "] is not exist!"
                      << std::endl;
            return false;
        }
    }

    return true;
}

bool startSearchWorker(const std::string& configPath) {
    std::cout << "start search worker with config path [" << configPath << "]!" << std::endl;
    assert(!global_search_worker);
    global_search_worker = new sw::SearchWorker;

    if (!global_search_worker->init(configPath)) {
        DELETE_AND_SET_NULL(global_search_worker);
        return false;
    }

    if (!global_search_worker->start()) {
        DELETE_AND_SET_NULL(global_search_worker);
        return false;
    }

    return true;
}

void stopSearchWorker() {
    std::cout << "begin to stop search worker ..." << std::endl;
    assert(global_search_worker);
    global_search_worker->stop();
    global_search_worker->destroy();
    std::cout << "stop search worker success!" << std::endl;
}

bool configureLogger() {
    std::string fileContent = sw::AlogConfig::instance()->make_alog_config();
    alog::Configurator::configureLoggerFromString(fileContent.c_str());

    red_search_cppcommon::InitBrpcLogUsingAlog();
    red_search_cppcommon::InitGlogRedirectingToAlog();
    if (FLAGS_enable_cat_monitor) {
        //alog::Logger::getRootLogger()->addAppender(red_search_cppcommon::AlogCatAppender::instance());
    }

    std::cout << "Glog, BrpcLog, Alog initialized" << std::endl;
    return true;
}

void shutdownLogger() {
    //red_search_cppcommon::ResetBrpcLogUsingAlog();
    //red_search_cppcommon::ResetGlogRedirectingToAlog();
    if (FLAGS_enable_cat_monitor) {
    //    alog::Logger::getRootLogger()->removeAppender(red_search_cppcommon::AlogCatAppender::instance());
    }
    //alog::Logger::shutdown();
    std::cout << "Alog closed" << std::endl;
    return;
}

static void signalHandler(int signal) {
    std::cout << "receive signal [" << signal << "]!" << std::endl;
}

static inline void AsyncSignalSafePrintError(const char* msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, FLAGS_thread_dump_file_path.c_str(), FLAGS_thread_dump_file_path.length());
}

// NOTE: This method should be async-signal-safe.
//       refer https://man7.org/linux/man-pages/man7/signal-safety.7.html for async-signal-safe.
static void WriteToThreadDumpFile(const char* msg) {
    if (!FLAGS_thread_dump_file_path.empty() && msg != nullptr) {
        // open file in async-signal-safe way.
        int thread_dump_file_fd = open(FLAGS_thread_dump_file_path.c_str(),
                O_WRONLY | O_SYNC | O_CREAT | O_APPEND, 0644);
        if (thread_dump_file_fd < 0) {
            AsyncSignalSafePrintError("Failed create thread dump file ");
            return;
        }

        // write file in async-signal-safe way.
        const size_t len = strlen(msg);
        size_t offset = 0;
        do {
            int bytes_written = write(thread_dump_file_fd, msg + offset, len - offset);
            if (bytes_written < 0) {
                AsyncSignalSafePrintError("Failed write backtrace to ");
                // close file in async-signal-safe way.
                if (close(thread_dump_file_fd) < 0) {
                    AsyncSignalSafePrintError("Failed close thread dump file ");
                }
                return;
            }
            offset += bytes_written;
        } while (offset < len);

        // close file in async-signal-safe way.
        if (close(thread_dump_file_fd) < 0) {
            AsyncSignalSafePrintError("Failed close thread dump file ");
            return;
        }
    }
}

void registerFailureSignalHandlerForQuickThreadDump() {
    if (FLAGS_enable_abseil_thread_dump) {
        // register {SIGSEGV, SIGILL, SIGFPE, SIGABRT, SIGTERM, SIGBUS, SIGTRAP} handler from abseil
        //   for quick thread dump.
        // refer https://note.red.net/doc/215480791750717440 for more details.
        absl::FailureSignalHandlerOptions options;
        options.call_previous_handler = true;
        options.writerfn = WriteToThreadDumpFile;
        absl::InstallFailureSignalHandler(options);
    }
}

bool registerSignalHandler() {
    sigemptyset(&global_mask);
    sigaddset(&global_mask, SIGINT);
    sigaddset(&global_mask, SIGTERM);
    sigaddset(&global_mask, SIGUSR1);
    sigaddset(&global_mask, SIGUSR2);
    sigaddset(&global_mask, SIGPIPE);

    if (0 != pthread_sigmask(SIG_BLOCK, &global_mask, &global_oldmask)) {
        std::cout << "set signal mask failed!" << std::endl;
        return false;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, signalHandler);
    signal(SIGUSR2, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // NOTE: this method should be called after registration of all signal handlers !!
    registerFailureSignalHandlerForQuickThreadDump();
    return true;
}

void waitSignal() {
    int error;
    int signo;

    for (;;) {
        error = sigwait(&global_mask, &signo);
        std::cout << "receive signal: " << signo << std::endl;

        if (error != 0) {
            continue;
        }

        switch (signo) {
            case SIGUSR1:
            case SIGUSR2:
            case SIGINT:
            case SIGTERM:
                return;

            default:
                continue;
        }
    }
}

int sw_main(int argc, char* argv[]) {
    google_breakpad::CanaryExceptionHandler eh;
    if (!parseAndValidateArgs(argc, argv)) {
        return -1;
    }

    configureLogger();

    if (!registerSignalHandler()) {
        std::cout << "start search worker failed!" << std::endl;
        return -1;
    }

    if (startSearchWorker(FLAGS_config)) {
        std::cout << "start search worker success!" << std::endl;
        waitSignal();
        stopSearchWorker();

        //prpc::PnsClientWrapper::GetInstance()->clear();
        /**
         * DO NOT close alog because some background thread may be using it now.
         */
//                shutdownLogger();
        //alog::Logger::flushAll();

        // 移除cat appender，因为alog和cat会有析构顺序问题
        //red_search_cppcommon::ResetAlog();

        return 0;
    } else {
        std::cout << "start search worker failed!" << std::endl;

        //prpc::PnsClientWrapper::GetInstance()->clear();
        /**
         * DO NOT close alog because some background thread may be using it now.
         */
//                shutdownLogger();
        //alog::Logger::flushAll();

        // 移除cat appender，因为alog和cat会有析构顺序问题
        //red_search_cppcommon::ResetAlog();
        return -1;
    }
}
