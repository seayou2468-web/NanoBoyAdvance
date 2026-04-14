// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

#include "../include/common/memory_detect.h"
namespace Common {

// Detects the RAM and Swapfile sizes
const MemoryInfo GetMemInfo() {
    MemoryInfo mem_info{};

#if defined(__APPLE__)
    u64 ramsize;
    struct xsw_usage vmusage;
    std::size_t sizeof_ramsize = sizeof(ramsize);
    std::size_t sizeof_vmusage = sizeof(vmusage);
    // hw and vm are defined in sysctl.h
    // https://github.com/apple/darwin-xnu/blob/master/bsd/sys/sysctl.h#L471
    // sysctlbyname(const char *, void *, size_t *, void *, size_t);
    sysctlbyname("hw.memsize", &ramsize, &sizeof_ramsize, nullptr, 0);
    sysctlbyname("vm.swapusage", &vmusage, &sizeof_vmusage, nullptr, 0);
    mem_info.total_physical_memory = ramsize;
    mem_info.total_swap_memory = vmusage.xsu_total;
#elif defined(__linux__)
    struct sysinfo meminfo;
    sysinfo(&meminfo);
    mem_info.total_physical_memory = meminfo.totalram;
    mem_info.total_swap_memory = meminfo.totalswap;
#else
    mem_info.total_physical_memory = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE);
    mem_info.total_swap_memory = 0;
#endif

    return mem_info;
}

u64 GetPageSize() {
    return static_cast<u64>(sysconf(_SC_PAGESIZE));
}

} // namespace Common
