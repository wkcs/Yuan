/// \file yuan_os.cpp
/// \brief Yuan OS runtime: file/time/thread/http helpers for stdlib builtins.

#include <chrono>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern "C" {

struct YuanString {
    const char* data;
    std::int64_t len;
};

} // extern "C"

namespace {

struct YuanDirEntryData {
    std::string path;
    std::string name;
    bool isFile = false;
    bool isDir = false;
};

struct YuanDirIter {
    std::vector<YuanDirEntryData> entries;
    std::size_t index = 0;
    bool hasCurrent = false;
    YuanDirEntryData current;
};

struct YuanOSThread {
    std::thread worker;
    std::atomic<bool> finished{false};
};

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

    YuanString out;
    out.data = buffer;
    out.len = static_cast<std::int64_t>(input.size());
    return out;
}

static YuanDirIter* toDirIter(std::uintptr_t handle) {
    return reinterpret_cast<YuanDirIter*>(handle);
}

static YuanOSThread* toThreadHandle(std::uintptr_t handle) {
    return reinterpret_cast<YuanOSThread*>(handle);
}

} // namespace

extern "C" std::int64_t yuan_os_time_unix_nanos() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return static_cast<std::int64_t>(nanos);
}

extern "C" void yuan_os_sleep_nanos(std::int64_t nanos) {
    if (nanos <= 0) {
        return;
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanos));
}

extern "C" void yuan_os_yield() {
    std::this_thread::yield();
}

extern "C" std::uintptr_t yuan_os_thread_spawn(void* entry_raw, std::uintptr_t ctx) {
    using ThreadEntryFn = void (*)(std::uintptr_t);
    auto entry = reinterpret_cast<ThreadEntryFn>(entry_raw);
    if (!entry) {
        return 0;
    }

    auto* threadData = new YuanOSThread();
    try {
        threadData->worker = std::thread([threadData, entry, ctx]() {
            try {
                entry(ctx);
            } catch (...) {
            }
            threadData->finished.store(true, std::memory_order_release);
        });
    } catch (...) {
        delete threadData;
        return 0;
    }

    return reinterpret_cast<std::uintptr_t>(threadData);
}

extern "C" int yuan_os_thread_is_finished(std::uintptr_t handle) {
    YuanOSThread* threadData = toThreadHandle(handle);
    if (!threadData) {
        return 1;
    }
    return threadData->finished.load(std::memory_order_acquire) ? 1 : 0;
}

extern "C" void yuan_os_thread_join(std::uintptr_t handle) {
    YuanOSThread* threadData = toThreadHandle(handle);
    if (!threadData) {
        return;
    }

    if (threadData->worker.joinable()) {
        threadData->worker.join();
    }
    delete threadData;
}

extern "C" YuanString yuan_os_read_file(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::ifstream in(toStdString(path), std::ios::binary);
    if (!in) {
        return emptyString();
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return toYuanString(oss.str());
}

extern "C" int yuan_os_write_file(const char* pathData,
                                  std::int64_t pathLen,
                                  const char* contentData,
                                  std::int64_t contentLen) {
    YuanString path{pathData, pathLen};
    YuanString content{contentData, contentLen};
    std::ofstream out(toStdString(path), std::ios::binary | std::ios::trunc);
    if (!out) {
        return 0;
    }

    if (content.data && content.len > 0) {
        out.write(content.data, static_cast<std::streamsize>(content.len));
    }

    return out.good() ? 1 : 0;
}

extern "C" int yuan_os_exists(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(toStdString(path)), ec) && !ec ? 1 : 0;
}

extern "C" int yuan_os_is_file(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(toStdString(path)), ec) && !ec ? 1 : 0;
}

extern "C" int yuan_os_is_dir(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(toStdString(path)), ec) && !ec ? 1 : 0;
}

extern "C" int yuan_os_create_dir(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    bool ok = std::filesystem::create_directory(std::filesystem::path(toStdString(path)), ec);
    if (!ec) {
        return ok ? 1 : 0;
    }
    return 0;
}

extern "C" int yuan_os_create_dir_all(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    (void)std::filesystem::create_directories(std::filesystem::path(toStdString(path)), ec);
    return ec ? 0 : 1;
}

extern "C" int yuan_os_remove_dir(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    bool ok = std::filesystem::remove(std::filesystem::path(toStdString(path)), ec);
    return (!ec && ok) ? 1 : 0;
}

extern "C" int yuan_os_remove_file(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    std::error_code ec;
    bool ok = std::filesystem::remove(std::filesystem::path(toStdString(path)), ec);
    return (!ec && ok) ? 1 : 0;
}

extern "C" std::uintptr_t yuan_os_read_dir_open(const char* pathData, std::int64_t pathLen) {
    YuanString path{pathData, pathLen};
    auto* iter = new YuanDirIter();
    std::error_code ec;
    std::filesystem::path dirPath = std::filesystem::path(toStdString(path));
    if (dirPath.empty()) {
        dirPath = ".";
    }

    if (!std::filesystem::is_directory(dirPath, ec) || ec) {
        delete iter;
        return 0;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec)) {
        if (ec) {
            break;
        }

        YuanDirEntryData item;
        item.path = entry.path().string();
        item.name = entry.path().filename().string();

        std::error_code sec;
        auto status = entry.status(sec);
        item.isFile = !sec && std::filesystem::is_regular_file(status);
        item.isDir = !sec && std::filesystem::is_directory(status);

        iter->entries.push_back(std::move(item));
    }

    return reinterpret_cast<std::uintptr_t>(iter);
}

extern "C" int yuan_os_read_dir_next(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    if (!iter) {
        return 0;
    }

    if (iter->index >= iter->entries.size()) {
        iter->hasCurrent = false;
        return 0;
    }

    iter->current = iter->entries[iter->index++];
    iter->hasCurrent = true;
    return 1;
}

extern "C" YuanString yuan_os_read_dir_entry_path(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    if (!iter || !iter->hasCurrent) {
        return emptyString();
    }
    return toYuanString(iter->current.path);
}

extern "C" YuanString yuan_os_read_dir_entry_name(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    if (!iter || !iter->hasCurrent) {
        return emptyString();
    }
    return toYuanString(iter->current.name);
}

extern "C" int yuan_os_read_dir_entry_is_file(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    if (!iter || !iter->hasCurrent) {
        return 0;
    }
    return iter->current.isFile ? 1 : 0;
}

extern "C" int yuan_os_read_dir_entry_is_dir(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    if (!iter || !iter->hasCurrent) {
        return 0;
    }
    return iter->current.isDir ? 1 : 0;
}

extern "C" void yuan_os_read_dir_close(std::uintptr_t handle) {
    YuanDirIter* iter = toDirIter(handle);
    delete iter;
}

extern "C" YuanString yuan_os_stdin_read_line() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return emptyString();
    }
    return toYuanString(line);
}
