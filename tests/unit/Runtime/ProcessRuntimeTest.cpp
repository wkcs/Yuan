#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
struct YuanString {
    const char* data;
    std::int64_t len;
};

void yuan_os_process_init(int argc, char** argv);
std::int64_t yuan_os_args_len();
YuanString yuan_os_arg_at(std::int64_t index);
int yuan_os_env_has(const char* keyData, std::int64_t keyLen);
YuanString yuan_os_env_get(const char* keyData, std::int64_t keyLen);
}

TEST(ProcessRuntimeTest, ProcessArgsRoundTrip) {
    char arg0[] = "prog";
    char arg1[] = "alpha";
    char arg2[] = "beta";
    char* argv[] = {arg0, arg1, arg2};

    yuan_os_process_init(3, argv);

    EXPECT_EQ(yuan_os_args_len(), 3);

    YuanString first = yuan_os_arg_at(0);
    ASSERT_EQ(first.len, 4);
    EXPECT_EQ(std::string(first.data, static_cast<std::size_t>(first.len)), "prog");

    YuanString second = yuan_os_arg_at(1);
    ASSERT_EQ(second.len, 5);
    EXPECT_EQ(std::string(second.data, static_cast<std::size_t>(second.len)), "alpha");

    YuanString outOfRange = yuan_os_arg_at(9);
    EXPECT_EQ(outOfRange.len, 0);
}

TEST(ProcessRuntimeTest, EnvHasAndGet) {
    const char* key = "YUAN_PROCESS_RUNTIME_TEST";
#if defined(_WIN32)
    _putenv_s(key, "runtime_ok");
#else
    setenv(key, "runtime_ok", 1);
#endif

    EXPECT_EQ(yuan_os_env_has(key, static_cast<std::int64_t>(std::strlen(key))), 1);

    YuanString value = yuan_os_env_get(key, static_cast<std::int64_t>(std::strlen(key)));
    EXPECT_EQ(std::string(value.data, static_cast<std::size_t>(value.len)), "runtime_ok");

    const char* missing = "YUAN_PROCESS_RUNTIME_TEST_MISSING";
    EXPECT_EQ(yuan_os_env_has(missing, static_cast<std::int64_t>(std::strlen(missing))), 0);
    YuanString missingValue = yuan_os_env_get(missing, static_cast<std::int64_t>(std::strlen(missing)));
    EXPECT_EQ(missingValue.len, 0);
}
