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
/// <remarks>
/// Registry mutations are protected by a System recursive mutex, while a separate recursive notification mutex
/// serializes notification execution and preserves the historical guarantee that unregistration does not return while
/// another thread is still executing an observer callback. The registry mutex is released around user callbacks so
/// observer code may safely call into other synchronized subsystems without participating in a cross-component lock cycle.
/// </remarks>
class ThreadSafeObservable : public Observable {
private:
    mutable System::Synchronization::RecursiveMutex _mutex;
    mutable System::Synchronization::RecursiveMutex _notificationMutex;
    std::atomic<std::size_t> _observerCount{0};

protected:
    /// <inheritdoc/>
    void BeforeObserverCallback() override {
        // ExecuteNotification holds _notificationMutex for the entire
        // notification operation. Releasing only the registry mutex allows
        // registration/query/unregistration to proceed while user code runs,
        // while the notification mutex remains the observer-lifetime barrier.
        _mutex.unlock();
    }

    /// <inheritdoc/>
    void AfterObserverCallback() noexcept override {
        _mutex.lock();
    }

    /// <summary>Executes a notification while serializing notification lifetime separately from registry access.</summary>
    template<class Operation>
    void ExecuteNotification(Operation&& operation) {
        // Serialize complete notification operations, preserving the old
        // single-notifier semantics and giving UnregisterObserver a stable
        // lifetime barrier. Recursive use permits nested notifications from a
        // callback on the same execution context.
        std::lock_guard<System::Synchronization::RecursiveMutex> notificationLock(
            _notificationMutex
        );

        // Observable::ExecuteNotification owns shared-lifetime validation. The
        // registry mutex begins locked, is released only around each user
        // callback by the hook above, and is restored before base iteration or
        // exception unwinding resumes.
        _mutex.lock();
        try {
            Observable::ExecuteNotification(std::forward<Operation>(operation));
        } catch (...) {
            _mutex.unlock();
            throw;
        }
        _mutex.unlock();
    }

public:
    ~ThreadSafeObservable() override {
        BeginObservableDestruction();
        // Notifications own shared lifetime, so destruction normally cannot
        // begin while one is active. Taking both barriers also makes this
        // invariant explicit for handle/destruction races.
        std::lock_guard<System::Synchronization::RecursiveMutex> notificationLock(
            _notificationMutex
        );
        std::lock_guard<System::Synchronization::RecursiveMutex> registryLock(_mutex);
    }

    /// <summary>Registers an observer for an explicit set of interfaces under the Observable registry lock.</summary>
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

    /// <summary>Thread-safely unregisters an observer and waits for any already-running notification to leave user code.</summary>
    /// <remarks>
    /// Removal from the registry happens first, preventing a new callback from starting for this registration. The
    /// notification barrier is then acquired after releasing the registry lock. Because it is recursive, an observer may
    /// unregister itself (or another observer) from inside its own callback without deadlocking.
    /// </remarks>
    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;

        bool registered = false;
        {
            std::lock_guard<System::Synchronization::RecursiveMutex> lock(_mutex);
            registered = Observable::IsObserverRegistered(observer);
            Observable::UnregisterObserver(observer);
            if (registered) {
                _observerCount.fetch_sub(1, std::memory_order_acq_rel);
            }
        }

        if (registered) {
            // A concurrent notification may have copied the observer pointer
            // immediately before registry removal. Waiting on the complete
            // notification barrier ensures that callback has returned before
            // this unregistration call can hand lifetime control back to the
            // observer owner. Self-unregistration succeeds recursively.
            std::lock_guard<System::Synchronization::RecursiveMutex> notificationLock(
                _notificationMutex
            );
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
