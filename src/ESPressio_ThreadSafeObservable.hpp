#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include <ESPressio_Synchronization.hpp>
#include "ESPressio_Observable.hpp"

namespace ESPressio {
namespace Observable {

/// <summary>Thread-safe Observable using the same RTTI-free typed binding registry as <c>Observable</c>.</summary>
/// <remarks>A System-provided recursive lock preserves registration and unregistration during observer callbacks without duplicating observer storage or dispatch machinery.</remarks>
class ThreadSafeObservable : public Observable {
private:
    mutable System::Synchronization::RecursiveMutex _mutex;
    std::atomic<std::size_t> _observerCount{0};

protected:
    /// <summary>Executes a notification while serializing access to observer registrations.</summary>
    template<class Operation>
    void ExecuteNotification(Operation&& operation) {
        // Observable::ExecuteNotification owns the shared-lifetime validation.
        // Do not bypass it merely because the observer count is zero.
        std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
        Observable::ExecuteNotification(std::forward<Operation>(operation));
    }

public:
    ~ThreadSafeObservable() override {
        BeginObservableDestruction();
        std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
    }

    /// <summary>Registers an observer for an explicit set of interfaces under the Observable lock.</summary>
    /// <returns>An RAII handle whose destruction unregisters the observer.</returns>
    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
        auto handle = Observable::template RegisterObserverAs<ObserverInterfaces...>(
            observer
        );
        _observerCount.fetch_add(1, std::memory_order_release);
        return handle;
    }

    /// <summary>Registers the observer for its static interface type without runtime type discovery or RTTI.</summary>
    template<typename TObserver>
    ObserverHandlePtr RegisterObserver(TObserver* observer) {
        return RegisterObserverAs<TObserver>(observer);
    }

    /// <summary>Registers an observer through the untyped <c>IObserver</c> interface.</summary>
    ObserverHandlePtr RegisterObserver(IObserver* observer) override {
        return RegisterObserverAs<IObserver>(observer);
    }

    /// <summary>Thread-safely unregisters an observer when currently registered.</summary>
    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;
        std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
        const bool registered = Observable::IsObserverRegistered(observer);
        Observable::UnregisterObserver(observer);
        if (registered) {
            _observerCount.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    /// <summary>Thread-safely determines whether an observer has an active registration.</summary>
    bool IsObserverRegistered(IObserver* observer) override {
        if (
            observer == nullptr ||
            _observerCount.load(std::memory_order_acquire) == 0
        ) {
            return false;
        }
        std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
        return Observable::IsObserverRegistered(observer);
    }
};

} // namespace Observable
} // namespace ESPressio
