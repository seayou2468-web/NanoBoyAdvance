// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "thread.h"
#include "common.h"


#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace Common
{

int CurrentThreadId()
{
#if defined(__APPLE__)
    return mach_thread_self();
#else
    return 0;
#endif
}

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{
    (void)thread;
    (void)mask;
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
