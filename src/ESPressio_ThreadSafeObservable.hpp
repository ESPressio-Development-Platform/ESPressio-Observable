#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include "ESPressio_Observable.hpp"

namespace ESPressio {
namespace Observable {

/// Thread-safe Observable using the same RTTI-free typed binding registry as
/// Observable. The recursive lock preserves register/unregister-during-callback
/// semantics without duplicating observer storage or dispatch machinery.
class ThreadSafeObservable : public Observable {
private:
    mutable std::recursive_mutex _mutex;
    std::atomic<std::size_t> _observerCount{0};

protected:
    template<class Operation>
    void ExecuteNotification(Operation&& operation) {
        if (_observerCount.load(std::memory_order_acquire) == 0) return;
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        Observable::ExecuteNotification(std::forward<Operation>(operation));
    }

public:
    ~ThreadSafeObservable() override {
        BeginObservableDestruction();
        std::lock_guard<std::recursive_mutex> lock(_mutex);
    }

    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        auto handle = Observable::template RegisterObserverAs<ObserverInterfaces...>(
            observer
        );
        _observerCount.fetch_add(1, std::memory_order_release);
        return handle;
    }

    ObserverHandlePtr RegisterObserver(IObserver* observer) override {
        return RegisterObserverAs<IObserver>(observer);
    }

    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        const bool registered = Observable::IsObserverRegistered(observer);
        Observable::UnregisterObserver(observer);
        if (registered) {
            _observerCount.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    bool IsObserverRegistered(IObserver* observer) override {
        if (
            observer == nullptr ||
            _observerCount.load(std::memory_order_acquire) == 0
        ) {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return Observable::IsObserverRegistered(observer);
    }
};

} // namespace Observable
} // namespace ESPressio
