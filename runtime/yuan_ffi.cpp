/// \file yuan_ffi.cpp
/// \brief Yuan C FFI runtime: dynamic loading + symbol lookup + raw calls.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {

struct YuanString {
    const char* data;
    std::int64_t len;
};

} // extern "C"

namespace {

thread_local std::string gLastError;

static YuanString emptyString() {
    return YuanString{"", 0};
}

static std::string toStdString(YuanString input) {
    if (!input.data || input.len <= 0) {
        return "";
    }
    return std::string(input.data, static_cast<std::size_t>(input.len));
}

static YuanString toYuanString(const std::string& input) {
    if (input.empty()) {
        return emptyString();
    }

    char* buffer = static_cast<char*>(std::malloc(input.size() + 1));
    if (!buffer) {
        return emptyString();
    }
    std::memcpy(buffer, input.data(), input.size());
    buffer[input.size()] = '\0';

    return YuanString{buffer, static_cast<std::int64_t>(input.size())};
}

static void setLastError(const std::string& message) {
    gLastError = message;
}

static void clearLastError() {
    gLastError.clear();
}

#if defined(_WIN32)
static std::string winErrorString(unsigned long code) {
    LPSTR msg = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageA(flags,
                               nullptr,
                               static_cast<DWORD>(code),
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               reinterpret_cast<LPSTR>(&msg),
                               0,
                               nullptr);
    if (!len || !msg) {
        return "windows error " + std::to_string(code);
    }
    std::string text(msg, len);
    LocalFree(msg);
    return text;
}
#endif

template <typename FnTy>
FnTy functionFromRaw(std::uintptr_t raw) {
    return reinterpret_cast<FnTy>(raw);
}

template <typename FnTy, typename... Args>
std::uintptr_t callRaw(std::uintptr_t fnRaw, Args... args) {
    if (fnRaw == 0) {
        setLastError("ffi call failed: null function pointer");
        return 0;
    }
    clearLastError();
    FnTy fn = functionFromRaw<FnTy>(fnRaw);
    return static_cast<std::uintptr_t>(fn(args...));
}

} // namespace

extern "C" std::uintptr_t yuan_ffi_open(const char* pathData, std::int64_t pathLen) {
    std::string libPath = toStdString(YuanString{pathData, pathLen});
    if (libPath.empty()) {
        setLastError("ffi_open failed: empty library path");
        return 0;
    }

#if defined(_WIN32)
    HMODULE mod = LoadLibraryA(libPath.c_str());
    if (!mod) {
        setLastError("ffi_open failed: " + winErrorString(GetLastError()));
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(mod);
#else
    (void)dlerror();
    void* handle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        setLastError(err ? err : "ffi_open failed");
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(handle);
#endif
}

extern "C" std::uintptr_t yuan_ffi_open_self() {
#if defined(_WIN32)
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) {
        setLastError("ffi_open_self failed: " + winErrorString(GetLastError()));
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(mod);
#else
    (void)dlerror();
    void* handle = dlopen(nullptr, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        const char* err = dlerror();
        setLastError(err ? err : "ffi_open_self failed");
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(handle);
#endif
}

extern "C" std::uintptr_t yuan_ffi_symbol(std::uintptr_t handle, const char* symbolData, std::int64_t symbolLen) {
    if (handle == 0) {
        setLastError("ffi_sym failed: null library handle");
        return 0;
    }

    std::string symName = toStdString(YuanString{symbolData, symbolLen});
    if (symName.empty()) {
        setLastError("ffi_sym failed: empty symbol name");
        return 0;
    }

#if defined(_WIN32)
    FARPROC proc = GetProcAddress(reinterpret_cast<HMODULE>(handle), symName.c_str());
    if (!proc) {
        setLastError("ffi_sym failed: " + winErrorString(GetLastError()));
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(proc);
#else
    (void)dlerror();
    void* sym = dlsym(reinterpret_cast<void*>(handle), symName.c_str());
    const char* err = dlerror();
    if (err) {
        setLastError(err);
        return 0;
    }
    if (!sym) {
        setLastError("ffi_sym failed: symbol not found");
        return 0;
    }
    clearLastError();
    return reinterpret_cast<std::uintptr_t>(sym);
#endif
}

extern "C" int yuan_ffi_close(std::uintptr_t handle) {
    if (handle == 0) {
        setLastError("ffi_close failed: null library handle");
        return 0;
    }

#if defined(_WIN32)
    if (!FreeLibrary(reinterpret_cast<HMODULE>(handle))) {
        setLastError("ffi_close failed: " + winErrorString(GetLastError()));
        return 0;
    }
    clearLastError();
    return 1;
#else
    if (dlclose(reinterpret_cast<void*>(handle)) != 0) {
        const char* err = dlerror();
        setLastError(err ? err : "ffi_close failed");
        return 0;
    }
    clearLastError();
    return 1;
#endif
}

extern "C" YuanString yuan_ffi_last_error() {
    return toYuanString(gLastError);
}

extern "C" std::uintptr_t yuan_ffi_cstr_len(std::uintptr_t cstrPtr) {
    if (cstrPtr == 0) {
        setLastError("ffi_cstr_len failed: null pointer");
        return 0;
    }
    clearLastError();
    const char* p = reinterpret_cast<const char*>(cstrPtr);
    return static_cast<std::uintptr_t>(std::strlen(p));
}

extern "C" std::uintptr_t yuan_ffi_call0(std::uintptr_t fn) {
    using FnTy = std::uintptr_t (*)();
    return callRaw<FnTy>(fn);
}

extern "C" std::uintptr_t yuan_ffi_call1(std::uintptr_t fn, std::uintptr_t a0) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t);
    return callRaw<FnTy>(fn, a0);
}

extern "C" std::uintptr_t yuan_ffi_call2(std::uintptr_t fn,
                                         std::uintptr_t a0,
                                         std::uintptr_t a1) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t);
    return callRaw<FnTy>(fn, a0, a1);
}

extern "C" std::uintptr_t yuan_ffi_call3(std::uintptr_t fn,
                                         std::uintptr_t a0,
                                         std::uintptr_t a1,
                                         std::uintptr_t a2) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t, std::uintptr_t, std::uintptr_t);
    return callRaw<FnTy>(fn, a0, a1, a2);
}

extern "C" std::uintptr_t yuan_ffi_call4(std::uintptr_t fn,
                                         std::uintptr_t a0,
                                         std::uintptr_t a1,
                                         std::uintptr_t a2,
                                         std::uintptr_t a3) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t);
    return callRaw<FnTy>(fn, a0, a1, a2, a3);
}

extern "C" std::uintptr_t yuan_ffi_call5(std::uintptr_t fn,
                                         std::uintptr_t a0,
                                         std::uintptr_t a1,
                                         std::uintptr_t a2,
                                         std::uintptr_t a3,
                                         std::uintptr_t a4) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t);
    return callRaw<FnTy>(fn, a0, a1, a2, a3, a4);
}

extern "C" std::uintptr_t yuan_ffi_call6(std::uintptr_t fn,
                                         std::uintptr_t a0,
                                         std::uintptr_t a1,
                                         std::uintptr_t a2,
                                         std::uintptr_t a3,
                                         std::uintptr_t a4,
                                         std::uintptr_t a5) {
    using FnTy = std::uintptr_t (*)(std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t,
                                    std::uintptr_t);
    return callRaw<FnTy>(fn, a0, a1, a2, a3, a4, a5);
}
