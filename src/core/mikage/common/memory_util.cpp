// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common.h"
#include "memory_util.h"
#include "string_util.h"

#include <errno.h>
#include <stdio.h>

#if !defined(_WIN32) && defined(__aarch64__) && !defined(MAP_32BIT)
#define PAGE_MASK     (getpagesize() - 1)
#define round_page(x) ((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#endif

// This is purposely not a full wrapper for virtualalloc/mmap, but it
// provides exactly the primitive operations that Dolphin needs.

void* AllocateExecutableMemory(size_t size, bool low)
{
    static char *map_hint = 0;
#if defined(__aarch64__) && !defined(MAP_32BIT)
    // This OS has no flag to enforce allocation below the 4 GB boundary,
    // but if we hint that we want a low address it is very likely we will
    // get one.
    // An older version of this code used MAP_FIXED, but that has the side
    // effect of discarding already mapped pages that happen to be in the
    // requested virtual memory range (such as the emulated RAM, sometimes).
    if (low && (!map_hint))
        map_hint = (char*)round_page(512*1024*1024); /* 0.5 GB rounded up to the next page */
#endif
    void* ptr = mmap(map_hint, size, PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANON | MAP_PRIVATE
#if defined(__aarch64__) && defined(MAP_32BIT)
        | (low ? MAP_32BIT : 0)
#endif
        , -1, 0);

    // printf("Mapped executable memory at %p (size %ld)\n", ptr,
    //    (unsigned long)size);
    
#if defined(__FreeBSD__)
    if (ptr == MAP_FAILED)
    {
        ptr = NULL;
#else
    if (ptr == NULL)
    {
#endif    
        PanicAlert("Failed to allocate executable memory");
    }
#if defined(__aarch64__) && !defined(MAP_32BIT)
    else
    {
        if (low)
        {
            map_hint += size;
            map_hint = (char*)round_page(map_hint); /* round up to the next page */
            // printf("Next map will (hopefully) be at %p\n", map_hint);
        }
    }
#endif

#if defined(IOS_ARM64)
    if ((u64)ptr >= 0x80000000 && low == true)
        PanicAlert("Executable memory ended up above 2GB!");
#endif

    return ptr;
}

void* AllocateMemoryPages(size_t size)
{
    void* ptr = mmap(0, size, PROT_READ | PROT_WRITE,
            MAP_ANON | MAP_PRIVATE, -1, 0);

    // printf("Mapped memory at %p (size %ld)\n", ptr,
    //    (unsigned long)size);

    if (ptr == NULL)
        PanicAlert("Failed to allocate raw memory");

    return ptr;
}

void* AllocateAlignedMemory(size_t size,size_t alignment)
{
    void* ptr = NULL;
#ifdef ANDROID
    ptr = memalign(alignment, size);
#else
    if (posix_memalign(&ptr, alignment, size) != 0)
        ERROR_LOG(MEMMAP, "Failed to allocate aligned memory");
#endif

    // printf("Mapped memory at %p (size %ld)\n", ptr,
    //    (unsigned long)size);

    if (ptr == NULL)
        PanicAlert("Failed to allocate aligned memory");

    return ptr;
}

void FreeMemoryPages(void* ptr, size_t size)
{
    if (ptr)
    {
        munmap(ptr, size);
    }
}

void FreeAlignedMemory(void* ptr)
{
    if (ptr)
    {
        free(ptr);
    }
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
    mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_EXEC) : PROT_READ);
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
    mprotect(ptr, size, allowExecute ? (PROT_READ | PROT_WRITE | PROT_EXEC) : PROT_WRITE | PROT_READ);
}

std::string MemUsage()
{
    return "";
}
