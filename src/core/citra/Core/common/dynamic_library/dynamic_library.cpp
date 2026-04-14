// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "../../../includes/libfmt.xcframework/fmt/format.h"
#include <dlfcn.h>
#include "../../include/common/dynamic_library/dynamic_library.h"
namespace Common {

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::DynamicLibrary(void* handle_) : handle{handle_} {}

DynamicLibrary::DynamicLibrary(std::string_view name, int major, int minor) {
    auto full_name = GetLibraryName(name, major, minor);
    void(Load(full_name));
}

DynamicLibrary::~DynamicLibrary() {
    if (handle) {
        dlclose(handle);
        handle = nullptr;
    }
}

bool DynamicLibrary::Load(std::string_view filename) {
    handle = dlopen(filename.data(), RTLD_LAZY);
    if (!handle) {
        load_error = dlerror();
        return false;
    }
    return true;
}

void* DynamicLibrary::GetRawSymbol(std::string_view name) const {
    return dlsym(handle, name.data());
}

std::string DynamicLibrary::GetLibraryName(std::string_view name, int major, int minor) {
#if defined(__APPLE__)
    auto prefix = name.starts_with("lib") ? "" : "lib";
    if (major >= 0 && minor >= 0) {
        return fmt::format("{}{}.{}.{}.dylib", prefix, name, major, minor);
    } else if (major >= 0) {
        return fmt::format("{}{}.{}.dylib", prefix, name, major);
    } else {
        return fmt::format("{}{}.dylib", prefix, name);
    }
#else
    auto prefix = name.starts_with("lib") ? "" : "lib";
    if (major >= 0 && minor >= 0) {
        return fmt::format("{}{}.so.{}.{}", prefix, name, major, minor);
    } else if (major >= 0) {
        return fmt::format("{}{}.so.{}", prefix, name, major);
    } else {
        return fmt::format("{}{}.so", prefix, name);
    }
#endif
}

} // namespace Common
