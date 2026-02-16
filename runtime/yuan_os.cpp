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
#include <curl/curl.h>

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

struct CurlResult {
    int status = -1;
    std::string body;
};

struct HttpRequestKey {
    std::string method;
    std::string url;
    std::string body;
    std::string headers;
    std::uint64_t timeoutMs = 0;
    bool stream = false;
};

struct HttpRequestCache {
    bool valid = false;
    HttpRequestKey key;
    CurlResult result;
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

static std::once_flag gCurlGlobalInitOnce;
static bool gCurlGlobalInitOk = false;
static std::mutex gHttpCacheMutex;
static HttpRequestCache gHttpCache;

static bool ensureCurlGlobalInit() {
    std::call_once(gCurlGlobalInitOnce, []() {
        gCurlGlobalInitOk = (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK);
    });
    return gCurlGlobalInitOk;
}

static bool httpRequestKeyEquals(const HttpRequestKey& lhs, const HttpRequestKey& rhs) {
    return lhs.method == rhs.method &&
           lhs.url == rhs.url &&
           lhs.body == rhs.body &&
           lhs.headers == rhs.headers &&
           lhs.timeoutMs == rhs.timeoutMs &&
           lhs.stream == rhs.stream;
}

static bool takeCachedHttpResult(const HttpRequestKey& key, CurlResult* out) {
    std::lock_guard<std::mutex> lock(gHttpCacheMutex);
    if (!gHttpCache.valid || !out) {
        return false;
    }
    if (!httpRequestKeyEquals(gHttpCache.key, key)) {
        return false;
    }
    *out = gHttpCache.result;
    gHttpCache.valid = false;
    return true;
}

static void storeCachedHttpResult(const HttpRequestKey& key, const CurlResult& result) {
    std::lock_guard<std::mutex> lock(gHttpCacheMutex);
    gHttpCache.key = key;
    gHttpCache.result = result;
    gHttpCache.valid = true;
}

static std::string trimAsciiSpace(const std::string& s) {
    std::size_t begin = 0;
    while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t')) {
        ++begin;
    }
    std::size_t end = s.size();
    while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        --end;
    }
    return s.substr(begin, end - begin);
}

static std::string decodeJsonStringLiteral(const std::string& text, std::size_t quotePos) {
    if (quotePos >= text.size() || text[quotePos] != '"') {
        return "";
    }

    std::string out;
    std::size_t i = quotePos + 1;
    while (i < text.size()) {
        char ch = text[i];
        if (ch == '"') {
            return out;
        }
        if (ch != '\\') {
            out.push_back(ch);
            ++i;
            continue;
        }

        ++i;
        if (i >= text.size()) {
            break;
        }
        char esc = text[i];
        switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
                out.push_back('?');
                if (i + 4 < text.size()) {
                    i += 4;
                }
                break;
            default:
                out.push_back(esc);
                break;
        }
        ++i;
    }
    return out;
}

static std::string extractOpenAIDeltaContent(const std::string& payload) {
    std::size_t deltaPos = payload.find("\"delta\"");
    if (deltaPos == std::string::npos) {
        return "";
    }
    std::size_t contentPos = payload.find("\"content\"", deltaPos);
    if (contentPos == std::string::npos) {
        return "";
    }
    std::size_t colonPos = payload.find(':', contentPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    std::size_t valuePos = colonPos + 1;
    while (valuePos < payload.size() &&
           (payload[valuePos] == ' ' || payload[valuePos] == '\t')) {
        ++valuePos;
    }
    if (valuePos >= payload.size() || payload[valuePos] != '"') {
        return "";
    }
    return decodeJsonStringLiteral(payload, valuePos);
}

static std::string extractOpenAIMessageContent(const std::string& payload) {
    std::size_t messagePos = payload.find("\"message\"");
    if (messagePos == std::string::npos) {
        return "";
    }
    std::size_t contentPos = payload.find("\"content\"", messagePos);
    if (contentPos == std::string::npos) {
        return "";
    }
    std::size_t colonPos = payload.find(':', contentPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    std::size_t valuePos = colonPos + 1;
    while (valuePos < payload.size() &&
           (payload[valuePos] == ' ' || payload[valuePos] == '\t')) {
        ++valuePos;
    }
    if (valuePos >= payload.size() || payload[valuePos] != '"') {
        return "";
    }
    return decodeJsonStringLiteral(payload, valuePos);
}

static std::string extractOpenAITextContent(const std::string& payload) {
    std::size_t textPos = payload.find("\"text\"");
    if (textPos == std::string::npos) {
        return "";
    }
    std::size_t colonPos = payload.find(':', textPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    std::size_t valuePos = colonPos + 1;
    while (valuePos < payload.size() &&
           (payload[valuePos] == ' ' || payload[valuePos] == '\t')) {
        ++valuePos;
    }
    if (valuePos >= payload.size() || payload[valuePos] != '"') {
        return "";
    }
    return decodeJsonStringLiteral(payload, valuePos);
}

struct CurlWriteState {
    std::string body;
    bool stream = false;
    bool printedAny = false;
    std::string pending;
};

static void processOpenAIStreamLine(const std::string& rawLine, CurlWriteState* state) {
    if (!state) {
        return;
    }
    std::string line = rawLine;
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line.rfind("data:", 0) != 0) {
        return;
    }

    std::string payload = trimAsciiSpace(line.substr(5));
    if (payload.empty() || payload == "[DONE]") {
        return;
    }

    std::string delta = extractOpenAIDeltaContent(payload);
    if (delta.empty()) {
        delta = extractOpenAIMessageContent(payload);
    }
    if (delta.empty()) {
        delta = extractOpenAITextContent(payload);
    }
    if (!delta.empty()) {
        std::cout << delta;
        std::cout.flush();
        state->printedAny = true;
    }
}

static void tryPrintStreamFallback(CurlWriteState* state) {
    if (!state || state->printedAny || state->body.empty()) {
        return;
    }

    // Some providers return non-SSE JSON even when stream=true.
    std::string fallback = extractOpenAIMessageContent(state->body);
    if (fallback.empty()) {
        fallback = extractOpenAIDeltaContent(state->body);
    }
    if (fallback.empty()) {
        fallback = extractOpenAITextContent(state->body);
    }
    if (!fallback.empty()) {
        std::cout << fallback;
        std::cout.flush();
        state->printedAny = true;
    }
}

static void consumeOpenAIStreamBuffer(CurlWriteState* state, bool flushAll) {
    if (!state) {
        return;
    }
    while (true) {
        std::size_t pos = state->pending.find('\n');
        if (pos == std::string::npos) {
            break;
        }
        std::string line = state->pending.substr(0, pos);
        state->pending.erase(0, pos + 1);
        processOpenAIStreamLine(line, state);
    }
    if (flushAll && !state->pending.empty()) {
        processOpenAIStreamLine(state->pending, state);
        state->pending.clear();
    }
}

static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (!userdata || !ptr) {
        return 0;
    }
    CurlWriteState* state = static_cast<CurlWriteState*>(userdata);
    const size_t bytes = size * nmemb;
    state->body.append(ptr, bytes);
    if (state->stream) {
        state->pending.append(ptr, bytes);
        consumeOpenAIStreamBuffer(state, false);
    }
    return bytes;
}

static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    if (!userdata || !buffer) {
        return 0;
    }
    std::string* out = static_cast<std::string*>(userdata);
    const size_t bytes = size * nitems;
    out->append(buffer, bytes);
    return bytes;
}

static int parseHttpStatusFromHeaders(const std::string& rawHeaders) {
    std::size_t pos = 0;
    int lastCode = -1;

    while (pos < rawHeaders.size()) {
        std::size_t end = rawHeaders.find('\n', pos);
        if (end == std::string::npos) {
            end = rawHeaders.size();
        }

        std::string line = rawHeaders.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.rfind("HTTP/", 0) == 0) {
            std::size_t sp = line.find(' ');
            if (sp != std::string::npos && sp + 3 < line.size()) {
                char c0 = line[sp + 1];
                char c1 = line[sp + 2];
                char c2 = line[sp + 3];
                if (c0 >= '0' && c0 <= '9' &&
                    c1 >= '0' && c1 <= '9' &&
                    c2 >= '0' && c2 <= '9') {
                    lastCode = (c0 - '0') * 100 + (c1 - '0') * 10 + (c2 - '0');
                }
            }
        }

        if (end == rawHeaders.size()) {
            break;
        }
        pos = end + 1;
    }

    return lastCode;
}

static int resolveHttpStatusFromResult(long statusCode, const std::string& responseHeaders, const std::string& body) {
    if (statusCode <= 0L) {
        int parsed = parseHttpStatusFromHeaders(responseHeaders);
        if (parsed > 0) {
            statusCode = static_cast<long>(parsed);
        }
    }
    if (statusCode <= 0L && !body.empty()) {
        statusCode = 200L;
    }
    return statusCode > 0L ? static_cast<int>(statusCode) : -1;
}

static bool buildCurlHeaders(const std::string* headers, struct curl_slist** out) {
    if (!out) {
        return false;
    }
    *out = nullptr;
    if (!headers || headers->empty()) {
        return true;
    }

    std::size_t start = 0;
    while (start <= headers->size()) {
        std::size_t end = headers->find('\n', start);
        if (end == std::string::npos) {
            end = headers->size();
        }

        std::string line = headers->substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos) {
            std::size_t last = line.find_last_not_of(" \t");
            std::string header = line.substr(first, last - first + 1);
            if (!header.empty()) {
                struct curl_slist* next = curl_slist_append(*out, header.c_str());
                if (!next) {
                    curl_slist_free_all(*out);
                    *out = nullptr;
                    return false;
                }
                *out = next;
            }
        }

        if (end == headers->size()) {
            break;
        }
        start = end + 1;
    }
    return true;
}

static CurlResult runCurlRequest(const std::string& method,
                                 const std::string& url,
                                 const std::string* body,
                                 const std::string* headers,
                                 std::uint64_t timeoutMs,
                                 bool streamResponse) {
    CurlResult result;
    if (!ensureCurlGlobalInit()) {
        result.body = "libcurl global initialization failed";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.body = "libcurl easy init failed";
        return result;
    }

    CurlWriteState writeState;
    writeState.stream = streamResponse;
    std::string responseHeaders;
    struct curl_slist* headerList = nullptr;
    if (!buildCurlHeaders(headers, &headerList)) {
        result.body = "libcurl header list allocation failed";
        curl_easy_cleanup(curl);
        return result;
    }

    (void)curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    (void)curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    (void)curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    // Avoid some Schannel/HTTP2 peer-reset edge cases on streaming endpoints.
    (void)curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    (void)curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeState);
    (void)curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
    (void)curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);

    long timeoutMsLong = 30000L;
    if (timeoutMs > 0) {
        const std::uint64_t cap = static_cast<std::uint64_t>((std::numeric_limits<long>::max)());
        timeoutMsLong = static_cast<long>(timeoutMs > cap ? cap : timeoutMs);
    }
    if (streamResponse) {
        (void)curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeoutMsLong);
        (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 0L);
        // For streaming, avoid hard total timeout. Abort only when the transfer
        // has no progress for timeoutMsLong (idle/low-speed timeout).
        (void)curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        long lowSpeedSec = timeoutMsLong / 1000L;
        if (lowSpeedSec <= 0L) {
            lowSpeedSec = 1L;
        }
        (void)curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, lowSpeedSec);
    } else {
        (void)curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMsLong);
    }

    if (headerList) {
        (void)curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    if (method == "POST") {
        const std::string emptyBody;
        const std::string& postBody = body ? *body : emptyBody;
        (void)curl_easy_setopt(curl, CURLOPT_POST, 1L);
        (void)curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postBody.c_str());
        (void)curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(postBody.size()));
    }

    CURLcode rc = curl_easy_perform(curl);
    if (writeState.stream) {
        consumeOpenAIStreamBuffer(&writeState, true);
        tryPrintStreamFallback(&writeState);
    }
    if (rc != CURLE_OK) {
        result.status = -1;
        result.body = curl_easy_strerror(rc);
        if (rc == CURLE_UNSUPPORTED_PROTOCOL) {
            if (url.rfind("https://", 0) == 0 || url.rfind("wss://", 0) == 0) {
                result.body =
                    "Unsupported protocol: HTTPS/TLS is unavailable in current libcurl build. "
                    "Rebuild Yuan with TLS-enabled libcurl (OpenSSL) or switch to system libcurl.";
            }
        }
    } else {
        long statusCode = 0L;
        CURLcode statusRc = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
        if (statusRc != CURLE_OK || statusCode <= 0L) {
            long connectCode = 0L;
            if (curl_easy_getinfo(curl, CURLINFO_HTTP_CONNECTCODE, &connectCode) == CURLE_OK &&
                connectCode > 0L) {
                statusCode = connectCode;
            }
        }
        result.status = resolveHttpStatusFromResult(statusCode, responseHeaders, writeState.body);
        result.body = std::move(writeState.body);
    }

    curl_slist_free_all(headerList);
    curl_easy_cleanup(curl);
    return result;
}

static CurlResult runHttpRequestCached(const std::string& method,
                                       const std::string& url,
                                       const std::string* body,
                                       const std::string* headers,
                                       std::uint64_t timeoutMs,
                                       bool streamResponse) {
    HttpRequestKey key;
    key.method = method;
    key.url = url;
    key.body = body ? *body : std::string{};
    key.headers = headers ? *headers : std::string{};
    key.timeoutMs = timeoutMs;
    key.stream = streamResponse;

    CurlResult cached;
    if (takeCachedHttpResult(key, &cached)) {
        return cached;
    }

    CurlResult fresh = runCurlRequest(method, url, body, headers, timeoutMs, streamResponse);
    storeCachedHttpResult(key, fresh);
    return fresh;
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

extern "C" int yuan_os_http_get_status_ex(const char* urlData,
                                          std::int64_t urlLen,
                                          const char* headersData,
                                          std::int64_t headersLen,
                                          std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("GET", urlText, nullptr, &headersText, timeoutMs, false);
    return result.status;
}

extern "C" YuanString yuan_os_http_get_body_ex(const char* urlData,
                                               std::int64_t urlLen,
                                               const char* headersData,
                                               std::int64_t headersLen,
                                               std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("GET", urlText, nullptr, &headersText, timeoutMs, false);
    return toYuanString(result.body);
}

extern "C" int yuan_os_http_post_status_ex(const char* urlData,
                                           std::int64_t urlLen,
                                           const char* bodyData,
                                           std::int64_t bodyLen,
                                           const char* headersData,
                                           std::int64_t headersLen,
                                           std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("POST", urlText, &bodyText, &headersText, timeoutMs, false);
    return result.status;
}

extern "C" YuanString yuan_os_http_post_body_ex(const char* urlData,
                                                std::int64_t urlLen,
                                                const char* bodyData,
                                                std::int64_t bodyLen,
                                                const char* headersData,
                                                std::int64_t headersLen,
                                                 std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("POST", urlText, &bodyText, &headersText, timeoutMs, false);
    return toYuanString(result.body);
}

extern "C" int yuan_os_http_post_status_ex2(const char* urlData,
                                            std::int64_t urlLen,
                                            const char* bodyData,
                                            std::int64_t bodyLen,
                                            const char* headersData,
                                            std::int64_t headersLen,
                                            std::uint64_t timeoutMs,
                                            int stream) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("POST",
                                             urlText,
                                             &bodyText,
                                             &headersText,
                                             timeoutMs,
                                             stream != 0);
    return result.status;
}

extern "C" YuanString yuan_os_http_post_body_ex2(const char* urlData,
                                                 std::int64_t urlLen,
                                                 const char* bodyData,
                                                 std::int64_t bodyLen,
                                                 const char* headersData,
                                                 std::int64_t headersLen,
                                                  std::uint64_t timeoutMs,
                                                  int stream) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    YuanString headers{headersData, headersLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    std::string headersText = toStdString(headers);
    CurlResult result = runHttpRequestCached("POST",
                                             urlText,
                                             &bodyText,
                                             &headersText,
                                             timeoutMs,
                                             stream != 0);
    return toYuanString(result.body);
}

extern "C" int yuan_os_http_get_status(const char* urlData, std::int64_t urlLen, std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    std::string urlText = toStdString(url);
    CurlResult result = runHttpRequestCached("GET", urlText, nullptr, nullptr, timeoutMs, false);
    return result.status;
}

extern "C" YuanString yuan_os_http_get_body(const char* urlData, std::int64_t urlLen, std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    std::string urlText = toStdString(url);
    CurlResult result = runHttpRequestCached("GET", urlText, nullptr, nullptr, timeoutMs, false);
    return toYuanString(result.body);
}

extern "C" int yuan_os_http_post_status(const char* urlData,
                                        std::int64_t urlLen,
                                        const char* bodyData,
                                        std::int64_t bodyLen,
                                        std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    CurlResult result = runHttpRequestCached("POST", urlText, &bodyText, nullptr, timeoutMs, false);
    return result.status;
}

extern "C" YuanString yuan_os_http_post_body(const char* urlData,
                                             std::int64_t urlLen,
                                             const char* bodyData,
                                             std::int64_t bodyLen,
                                             std::uint64_t timeoutMs) {
    YuanString url{urlData, urlLen};
    YuanString body{bodyData, bodyLen};
    std::string urlText = toStdString(url);
    std::string bodyText = toStdString(body);
    CurlResult result = runHttpRequestCached("POST", urlText, &bodyText, nullptr, timeoutMs, false);
    return toYuanString(result.body);
}
