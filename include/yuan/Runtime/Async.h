/// \file Async.h
/// \brief C ABI for Yuan async runtime primitives.

#ifndef YUAN_RUNTIME_ASYNC_H
#define YUAN_RUNTIME_ASYNC_H

#include <cstdint>

extern "C" {

struct YuanAsyncScheduler;
struct YuanPromise;

/// ---------------------------
/// Scheduler API
/// ---------------------------
YuanAsyncScheduler* yuan_async_scheduler_create();
void yuan_async_scheduler_destroy(YuanAsyncScheduler* scheduler);
void yuan_async_scheduler_set_current(YuanAsyncScheduler* scheduler);
YuanAsyncScheduler* yuan_async_scheduler_current();
void yuan_async_scheduler_enqueue(YuanAsyncScheduler* scheduler,
                                  void* fn_raw,
                                  void* ctx,
                                  void* cleanup_raw);
int yuan_async_scheduler_run_one(YuanAsyncScheduler* scheduler);
void yuan_async_scheduler_run_until_idle(YuanAsyncScheduler* scheduler);

/// ---------------------------
/// Promise API
/// ---------------------------
YuanPromise* yuan_promise_create();
void yuan_promise_retain(YuanPromise* promise);
void yuan_promise_release(YuanPromise* promise);
int yuan_promise_status(const YuanPromise* promise); // 0=pending,1=fulfilled,2=rejected
std::uintptr_t yuan_promise_value(const YuanPromise* promise);
std::uintptr_t yuan_promise_error(const YuanPromise* promise);
void yuan_promise_then(YuanPromise* promise,
                       YuanAsyncScheduler* scheduler,
                       void* fn_raw,
                       void* ctx,
                       void* cleanup_raw);
void yuan_promise_resolve(YuanPromise* promise, std::uintptr_t value);
void yuan_promise_reject(YuanPromise* promise, std::uintptr_t error);
int yuan_promise_await(YuanPromise* promise,
                       std::uintptr_t* out_value,
                       std::uintptr_t* out_error); // 1=ok,-1=error,0=invalid

/// ---------------------------
/// CodeGen bridge hooks
/// ---------------------------
void yuan_async_suspend_point();
void yuan_async_step();
std::uint64_t yuan_async_step_count();
void yuan_async_run(void* entry_raw, void* out_slot);

} // extern "C"

#endif // YUAN_RUNTIME_ASYNC_H
