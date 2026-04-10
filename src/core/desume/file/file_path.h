#pragma once

#include <sys/stat.h>
#include <string>

static inline const char* path_default_slash(void)
{
    return "/";
}

static inline int path_mkdir(const char* path)
{
    return mkdir(path, 0755);
}
