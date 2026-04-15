// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "thread.h"
#include "common.h"

#include <unistd.h>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

#if defined(__linux__)
#include <sched.h>
#include <sys/syscall.h>
#endif

namespace Common
{

int CurrentThreadId()
{
#if defined(__APPLE__)
    return mach_thread_self();
#elif defined(__linux__)
    return static_cast<int>(syscall(SYS_gettid));
#else
    return 0;
#endif
}

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{
#if defined(__linux__)
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);

    for (int i = 0; i != sizeof(mask) * 8; ++i)
        if ((mask >> i) & 1)
            CPU_SET(i, &cpu_set);

    pthread_setaffinity_np(thread, sizeof(cpu_set), &cpu_set);
#else
    (void)thread;
    (void)mask;
#endif
}

void SetCurrentThreadAffinity(u32 mask)
{
    SetThreadAffinity(pthread_self(), mask);
}

void SleepCurrentThread(int ms)
{
    usleep(1000 * ms);
}

void SwitchCurrentThread()
{
    usleep(1000 * 1);
}

void SetCurrentThreadName(const char* szThreadName)
{
#if defined(__APPLE__)
    pthread_setname_np(szThreadName);
#else
    pthread_setname_np(pthread_self(), szThreadName);
#endif
}

} // namespace Common
