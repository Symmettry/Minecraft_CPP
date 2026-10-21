#include "CrashHandler.hpp"

#include <csignal>
#include <cstdlib>

#ifndef _WIN32
#include <execinfo.h>
#include <unistd.h>
#endif

namespace {
void handleFatalSignal(const int signal) {
#ifndef _WIN32
    constexpr char header[] = "\n[CrashHandler] Fatal signal; native backtrace:\n";
    write(STDERR_FILENO, header, sizeof(header) - 1);

    void* frames[64];
    const int frameCount = backtrace(frames, 64);
    backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);
#endif
    std::_Exit(128 + signal);
}
}

void installCrashHandlers() {
    std::signal(SIGSEGV, handleFatalSignal);
    std::signal(SIGABRT, handleFatalSignal);
}
