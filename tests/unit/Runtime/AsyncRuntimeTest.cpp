#include "yuan/Runtime/Async.h"
#include <gtest/gtest.h>
#include <cstdint>

namespace {

void markTask(void* ctx) {
    auto* value = static_cast<int*>(ctx);
    *value = 11;
}

void resolvePromiseTask(void* ctx) {
    auto* promise = static_cast<YuanPromise*>(ctx);
    yuan_promise_resolve(promise, static_cast<std::uintptr_t>(123));
}

void continuationTask(void* ctx) {
    auto* value = static_cast<int*>(ctx);
    *value = 29;
}

} // namespace

TEST(AsyncRuntimeTest, SchedulerRunsQueuedTask) {
    YuanAsyncScheduler* scheduler = yuan_async_scheduler_create();
    ASSERT_NE(scheduler, nullptr);
    yuan_async_scheduler_set_current(scheduler);

    int value = 0;
    yuan_async_scheduler_enqueue(
        scheduler,
        reinterpret_cast<void*>(&markTask),
        &value,
        nullptr
    );

    EXPECT_EQ(yuan_async_scheduler_run_one(scheduler), 1);
    EXPECT_EQ(value, 11);
    EXPECT_EQ(yuan_async_scheduler_run_one(scheduler), 0);

    yuan_async_scheduler_set_current(nullptr);
    yuan_async_scheduler_destroy(scheduler);
}

TEST(AsyncRuntimeTest, PromiseAwaitPumpsScheduler) {
    YuanAsyncScheduler* scheduler = yuan_async_scheduler_create();
    ASSERT_NE(scheduler, nullptr);
    yuan_async_scheduler_set_current(scheduler);

    YuanPromise* promise = yuan_promise_create();
    ASSERT_NE(promise, nullptr);

    yuan_async_scheduler_enqueue(
        scheduler,
        reinterpret_cast<void*>(&resolvePromiseTask),
        promise,
        nullptr
    );

    std::uintptr_t value = 0;
    std::uintptr_t error = 0;
    int status = yuan_promise_await(promise, &value, &error);

    EXPECT_EQ(status, 1);
    EXPECT_EQ(value, static_cast<std::uintptr_t>(123));
    EXPECT_EQ(error, static_cast<std::uintptr_t>(0));

    yuan_promise_release(promise);
    yuan_async_scheduler_set_current(nullptr);
    yuan_async_scheduler_destroy(scheduler);
}

TEST(AsyncRuntimeTest, PromiseContinuationDispatched) {
    YuanAsyncScheduler* scheduler = yuan_async_scheduler_create();
    ASSERT_NE(scheduler, nullptr);
    yuan_async_scheduler_set_current(scheduler);

    YuanPromise* promise = yuan_promise_create();
    ASSERT_NE(promise, nullptr);

    int flag = 0;
    yuan_promise_then(
        promise,
        scheduler,
        reinterpret_cast<void*>(&continuationTask),
        &flag,
        nullptr
    );

    yuan_promise_resolve(promise, static_cast<std::uintptr_t>(1));
    yuan_async_scheduler_run_until_idle(scheduler);

    EXPECT_EQ(flag, 29);

    yuan_promise_release(promise);
    yuan_async_scheduler_set_current(nullptr);
    yuan_async_scheduler_destroy(scheduler);
}
