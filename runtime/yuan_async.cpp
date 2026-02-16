/// \file yuan_async.cpp
/// \brief Async runtime: scheduler + promise + step hooks used by CodeGen.

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

namespace {

using AsyncTaskFn = void (*)(void*);

struct YuanAsyncScheduler;
struct YuanPromise;

struct ScheduledTask {
    AsyncTaskFn Fn = nullptr;
    void* Ctx = nullptr;
    AsyncTaskFn Cleanup = nullptr;
};

struct PromiseContinuation {
    YuanAsyncScheduler* Scheduler = nullptr;
    AsyncTaskFn Fn = nullptr;
    void* Ctx = nullptr;
    AsyncTaskFn Cleanup = nullptr;
};

struct YuanAsyncScheduler {
    std::mutex Mu;
    std::deque<ScheduledTask> Queue;
};

enum class PromiseStatus : std::uint8_t {
    Pending = 0,
    Fulfilled = 1,
    Rejected = 2,
};

struct YuanPromise {
    std::atomic<std::uint32_t> RefCount{1};
    mutable std::mutex Mu;
    std::condition_variable Cv;
    PromiseStatus Status = PromiseStatus::Pending;
    std::uintptr_t Value = 0;
    std::uintptr_t Error = 0;
    std::vector<PromiseContinuation> Continuations;
};

thread_local YuanAsyncScheduler* g_current_scheduler = nullptr;
std::atomic<std::uint64_t> g_async_step_counter{0};

static void cleanupTask(const ScheduledTask& task) {
    if (task.Cleanup) {
        task.Cleanup(task.Ctx);
    }
}

static void enqueueTask(YuanAsyncScheduler* scheduler, ScheduledTask task) {
    if (!task.Fn) {
        cleanupTask(task);
        return;
    }

    if (!scheduler) {
        scheduler = g_current_scheduler;
    }

    if (!scheduler) {
        task.Fn(task.Ctx);
        cleanupTask(task);
        return;
    }

    std::lock_guard<std::mutex> lock(scheduler->Mu);
    scheduler->Queue.push_back(task);
}

static bool runOneTask(YuanAsyncScheduler* scheduler) {
    if (!scheduler) {
        return false;
    }

    ScheduledTask task;
    {
        std::lock_guard<std::mutex> lock(scheduler->Mu);
        if (scheduler->Queue.empty()) {
            return false;
        }
        task = scheduler->Queue.front();
        scheduler->Queue.pop_front();
    }

    task.Fn(task.Ctx);
    cleanupTask(task);
    return true;
}

static void runUntilIdle(YuanAsyncScheduler* scheduler) {
    while (runOneTask(scheduler)) {
    }
}

static void drainAndDestroyTasks(YuanAsyncScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    std::deque<ScheduledTask> remain;
    {
        std::lock_guard<std::mutex> lock(scheduler->Mu);
        remain.swap(scheduler->Queue);
    }

    for (const auto& task : remain) {
        cleanupTask(task);
    }
}

static PromiseStatus getPromiseStatus(const YuanPromise* promise) {
    if (!promise) {
        return PromiseStatus::Rejected;
    }

    std::lock_guard<std::mutex> lock(promise->Mu);
    return promise->Status;
}

static void dispatchContinuations(std::vector<PromiseContinuation>& continuations) {
    for (auto& cont : continuations) {
        enqueueTask(cont.Scheduler, {cont.Fn, cont.Ctx, cont.Cleanup});
    }
}

} // namespace

extern "C" YuanAsyncScheduler* yuan_async_scheduler_create() {
    return new YuanAsyncScheduler();
}

extern "C" void yuan_async_scheduler_destroy(YuanAsyncScheduler* scheduler) {
    if (!scheduler) {
        return;
    }
    drainAndDestroyTasks(scheduler);
    delete scheduler;
}

extern "C" void yuan_async_scheduler_set_current(YuanAsyncScheduler* scheduler) {
    g_current_scheduler = scheduler;
}

extern "C" YuanAsyncScheduler* yuan_async_scheduler_current() {
    return g_current_scheduler;
}

extern "C" void yuan_async_scheduler_enqueue(YuanAsyncScheduler* scheduler,
                                             void* fn_raw,
                                             void* ctx,
                                             void* cleanup_raw) {
    auto fn = reinterpret_cast<AsyncTaskFn>(fn_raw);
    auto cleanup = reinterpret_cast<AsyncTaskFn>(cleanup_raw);
    enqueueTask(scheduler, {fn, ctx, cleanup});
}

extern "C" int yuan_async_scheduler_run_one(YuanAsyncScheduler* scheduler) {
    return runOneTask(scheduler) ? 1 : 0;
}

extern "C" void yuan_async_scheduler_run_until_idle(YuanAsyncScheduler* scheduler) {
    runUntilIdle(scheduler);
}

extern "C" YuanPromise* yuan_promise_create() {
    return new YuanPromise();
}

extern "C" void yuan_promise_retain(YuanPromise* promise) {
    if (!promise) {
        return;
    }
    promise->RefCount.fetch_add(1, std::memory_order_relaxed);
}

extern "C" void yuan_promise_release(YuanPromise* promise) {
    if (!promise) {
        return;
    }
    if (promise->RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        std::vector<PromiseContinuation> continuations;
        {
            std::lock_guard<std::mutex> lock(promise->Mu);
            continuations.swap(promise->Continuations);
        }
        for (auto& cont : continuations) {
            if (cont.Cleanup) {
                cont.Cleanup(cont.Ctx);
            }
        }
        delete promise;
    }
}

extern "C" int yuan_promise_status(const YuanPromise* promise) {
    switch (getPromiseStatus(promise)) {
        case PromiseStatus::Pending:
            return 0;
        case PromiseStatus::Fulfilled:
            return 1;
        case PromiseStatus::Rejected:
            return 2;
    }
    return 2;
}

extern "C" std::uintptr_t yuan_promise_value(const YuanPromise* promise) {
    if (!promise) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(promise->Mu);
    return promise->Value;
}

extern "C" std::uintptr_t yuan_promise_error(const YuanPromise* promise) {
    if (!promise) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(promise->Mu);
    return promise->Error;
}

extern "C" void yuan_promise_then(YuanPromise* promise,
                                  YuanAsyncScheduler* scheduler,
                                  void* fn_raw,
                                  void* ctx,
                                  void* cleanup_raw) {
    auto fn = reinterpret_cast<AsyncTaskFn>(fn_raw);
    auto cleanup = reinterpret_cast<AsyncTaskFn>(cleanup_raw);
    if (!promise || !fn) {
        if (cleanup) {
            cleanup(ctx);
        }
        return;
    }

    PromiseContinuation continuation{scheduler, fn, ctx, cleanup};
    bool dispatchNow = false;
    {
        std::lock_guard<std::mutex> lock(promise->Mu);
        if (promise->Status == PromiseStatus::Pending) {
            promise->Continuations.push_back(continuation);
        } else {
            dispatchNow = true;
        }
    }

    if (dispatchNow) {
        enqueueTask(continuation.Scheduler, {continuation.Fn, continuation.Ctx, continuation.Cleanup});
    }
}

extern "C" void yuan_promise_resolve(YuanPromise* promise, std::uintptr_t value) {
    if (!promise) {
        return;
    }

    std::vector<PromiseContinuation> continuations;
    {
        std::lock_guard<std::mutex> lock(promise->Mu);
        if (promise->Status != PromiseStatus::Pending) {
            return;
        }
        promise->Status = PromiseStatus::Fulfilled;
        promise->Value = value;
        promise->Error = 0;
        continuations.swap(promise->Continuations);
    }

    promise->Cv.notify_all();
    dispatchContinuations(continuations);
}

extern "C" void yuan_promise_reject(YuanPromise* promise, std::uintptr_t error) {
    if (!promise) {
        return;
    }

    std::vector<PromiseContinuation> continuations;
    {
        std::lock_guard<std::mutex> lock(promise->Mu);
        if (promise->Status != PromiseStatus::Pending) {
            return;
        }
        promise->Status = PromiseStatus::Rejected;
        promise->Error = error;
        promise->Value = 0;
        continuations.swap(promise->Continuations);
    }

    promise->Cv.notify_all();
    dispatchContinuations(continuations);
}

extern "C" int yuan_promise_await(YuanPromise* promise,
                                  std::uintptr_t* out_value,
                                  std::uintptr_t* out_error) {
    if (!promise) {
        return 0;
    }

    for (;;) {
        PromiseStatus status = PromiseStatus::Pending;
        std::uintptr_t value = 0;
        std::uintptr_t error = 0;
        bool shouldPumpScheduler = false;
        {
            std::unique_lock<std::mutex> lock(promise->Mu);
            status = promise->Status;
            if (status == PromiseStatus::Pending) {
                if (g_current_scheduler) {
                    shouldPumpScheduler = true;
                } else {
                    promise->Cv.wait(lock, [&] { return promise->Status != PromiseStatus::Pending; });
                    status = promise->Status;
                }
            }
            value = promise->Value;
            error = promise->Error;
        }

        if (status == PromiseStatus::Pending && shouldPumpScheduler) {
            if (!runOneTask(g_current_scheduler)) {
                std::unique_lock<std::mutex> lock(promise->Mu);
                if (promise->Status == PromiseStatus::Pending) {
                    promise->Cv.wait(lock, [&] { return promise->Status != PromiseStatus::Pending; });
                }
            }
            continue;
        }

        if (status == PromiseStatus::Fulfilled) {
            if (out_value) {
                *out_value = value;
            }
            if (out_error) {
                *out_error = 0;
            }
            return 1;
        }
        if (status == PromiseStatus::Rejected) {
            if (out_value) {
                *out_value = 0;
            }
            if (out_error) {
                *out_error = error;
            }
            return -1;
        }
    }
}

extern "C" void yuan_async_suspend_point() {
    g_async_step_counter.fetch_add(1, std::memory_order_relaxed);
    if (g_current_scheduler) {
        (void)runOneTask(g_current_scheduler);
    }
}

extern "C" void yuan_async_step() {
    yuan_async_suspend_point();
}

extern "C" std::uint64_t yuan_async_step_count() {
    return g_async_step_counter.load(std::memory_order_relaxed);
}

extern "C" void yuan_async_run(void* entry_raw, void* out_slot) {
    auto entry = reinterpret_cast<AsyncTaskFn>(entry_raw);
    if (!entry) {
        return;
    }

    YuanAsyncScheduler* scheduler = g_current_scheduler;
    bool ownsScheduler = false;
    if (!scheduler) {
        scheduler = yuan_async_scheduler_create();
        ownsScheduler = true;
    }

    YuanAsyncScheduler* prev = g_current_scheduler;
    g_current_scheduler = scheduler;
    entry(out_slot);
    runUntilIdle(scheduler);
    g_current_scheduler = prev;

    if (ownsScheduler) {
        yuan_async_scheduler_destroy(scheduler);
    }
}
