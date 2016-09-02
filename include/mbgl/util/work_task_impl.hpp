#pragma once

#include <mbgl/util/work_task.hpp>
#include <mbgl/util/run_loop.hpp>

#include <mutex>

namespace mbgl {

template <class F, class P>
class WorkTaskImpl : public WorkTask {
public:
    WorkTaskImpl(F&& f, P&& p, std::shared_ptr<std::atomic<bool>> canceled_)
      : canceled(std::move(canceled_)),
        func(std::move(f)),
        params(std::move(p)) {
    }

    void operator()() override {
        // Lock the mutex while processing so that cancel() will block.
        std::lock_guard<std::recursive_mutex> lock(mutex);
        if (!*canceled) {
            invoke(std::make_index_sequence<std::tuple_size<P>::value>{});
        }
    }

    // If the task has not yet begun, this will cancel it.
    // If the task is in progress, this will block until it completed. (Currently
    // necessary because of shared state, but should be removed.) It will also
    // cancel the after callback.
    // If the task has completed, but the after callback has not executed, this
    // will cancel the after callback.
    // If the task has completed and the after callback has executed, this will
    // do nothing.
    void cancel() override {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        *canceled = true;
    }

private:
    template <std::size_t... I>
    void invoke(std::index_sequence<I...>) {
        func(std::move(std::get<I>(std::forward<P>(params)))...);
    }

    std::recursive_mutex mutex;
    std::shared_ptr<std::atomic<bool>> canceled;

    F func;
    P params;
};

template <class Fn, class... Args>
std::shared_ptr<WorkTask> WorkTask::make(Fn&& fn, Args&&... args) {
    auto flag = std::make_shared<std::atomic<bool>>();
    *flag = false;

    auto tuple = std::make_tuple(std::move(args)...);
    return std::make_shared<WorkTaskImpl<Fn, decltype(tuple)>>(
        std::move(fn),
        std::move(tuple),
        flag);
}

template <class Fn, class Cb, class... Args>
std::shared_ptr<WorkTask> WorkTask::makeWithCallback(Fn&& fn, Cb&& callback, Args&&... args) {
    auto flag = std::make_shared<std::atomic<bool>>();
    *flag = false;

    // Create a lambda L1 that invokes another lambda L2 on the current RunLoop R, that calls
    // the callback C. Both lambdas check the flag before proceeding. L1 needs to check the flag
    // because if the request was cancelled, then R might have been destroyed. L2 needs to check
    // the flag because the request may have been cancelled after L2 was invoked but before it
    // began executing.
    auto after = [flag, current = util::RunLoop::Get(), callback1 = std::move(callback)] (auto&&... results1) {
        if (!*flag) {
            current->invoke([flag, callback2 = std::move(callback1)] (auto&&... results2) {
                if (!*flag) {
                    callback2(std::move(results2)...);
                }
            }, std::move(results1)...);
        }
    };

    auto tuple = std::make_tuple(std::move(args)..., after);
    return std::make_shared<WorkTaskImpl<Fn, decltype(tuple)>>(
        std::move(fn),
        std::move(tuple),
        flag);
}

} // namespace mbgl
