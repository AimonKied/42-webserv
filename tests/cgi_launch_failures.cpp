// Linux/GNU linker fault injection: production executable is never instrumented.
#include "CgiExecutor.hpp"
#include <cassert>
#include <cerrno>
#include <dirent.h>
#include <iostream>
#include <unistd.h>

namespace {
int pipeCalls = 0;
int failPipeCall = 0;
int injectedError = 0;

size_t descriptorCount() {
    DIR* directory = opendir("/proc/self/fd");
    assert(directory);
    size_t count = 0;
    while (dirent* entry = readdir(directory))
        if (entry->d_name[0] != '.') ++count;
    closedir(directory);
    return count;
}
}

extern "C" int __real_pipe(int descriptors[2]);
extern "C" int __wrap_pipe(int descriptors[2]) {
    if (++pipeCalls == failPipeCall) {
        errno = injectedError;
        return -1;
    }
    return __real_pipe(descriptors);
}

extern "C" pid_t __wrap_fork() {
    // All tests reaching fork exercise its failure path; no child is spawned.
    errno = injectedError;
    return -1;
}

int main() {
    webserv::CgiExecutor executor;
    webserv::CgiLaunch launch;
    launch.executable = "/usr/bin/python3";
    launch.scriptPath = "/unused.py";
    launch.workingDirectory = "/";
    launch.input = "request body";
    struct Case {
        int pipeCall;
        int error;
        webserv::CgiStartResult expected;
    };
    const Case cases[] = {
        {1, EMFILE, webserv::CgiStartResult::Unavailable},
        {2, ENFILE, webserv::CgiStartResult::Unavailable},
        {0, EAGAIN, webserv::CgiStartResult::Unavailable},
        {0, ENOMEM, webserv::CgiStartResult::Unavailable},
        {0, EPERM, webserv::CgiStartResult::Failed},
    };
    const size_t baseline = descriptorCount();
    for (const Case& test : cases) {
        for (int repetition = 0; repetition < 20; ++repetition) {
            pipeCalls = 0;
            failPipeCall = test.pipeCall;
            injectedError = test.error;
            assert(executor.start(42, launch) == test.expected);
            assert(executor.empty());
            assert(descriptorCount() == baseline);
        }
    }
    std::cout << "PASS: first/second pipe and fork failures classified without fd/job leaks (100 launches)\n";
}
