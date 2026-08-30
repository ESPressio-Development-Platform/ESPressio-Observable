#include <atomic>
#include <cassert>
#include <chrono>
#include <thread>

#include "ESPressio_ThreadSafeObservable.hpp"

using namespace ESPressio::Observable;

namespace {

struct TestObserver final : IObserver {
    std::atomic<uint32_t> Calls{0};
};

class TestObservable final : public ThreadSafeObservable {
public:
    template<typename Callback>
    void Notify(Callback&& callback) {
        ExecuteNotification([&](NotificationContext& context) {
            context.WithObservers(std::forward<Callback>(callback));
        });
    }
};

void TestRegistryAccessDoesNotWaitForCallback() {
    auto observable = std::make_shared<TestObservable>();
    TestObserver first;
    TestObserver second;
    auto firstHandle = observable->RegisterObserver(&first);

    std::atomic<bool> callbackEntered{false};
    std::atomic<bool> releaseCallback{false};
    std::atomic<bool> queryFinished{false};
    std::atomic<bool> registrationFinished{false};
    ObserverHandlePtr secondHandle;

    std::thread notifier([&] {
        observable->Notify([&](IObserver* observer) {
            if (observer != &first) return;
            callbackEntered.store(true, std::memory_order_release);
            while (!releaseCallback.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        });
    });

    while (!callbackEntered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::thread query([&] {
        assert(observable->IsObserverRegistered(&first));
        queryFinished.store(true, std::memory_order_release);
    });

    std::thread registration([&] {
        secondHandle = observable->RegisterObserver(&second);
        registrationFinished.store(true, std::memory_order_release);
    });

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    while (
        (!queryFinished.load(std::memory_order_acquire) ||
         !registrationFinished.load(std::memory_order_acquire)) &&
        std::chrono::steady_clock::now() < deadline
    ) {
        std::this_thread::yield();
    }

    // These operations would remain blocked if the observer callback still
    // executed while holding the registry mutex.
    assert(queryFinished.load(std::memory_order_acquire));
    assert(registrationFinished.load(std::memory_order_acquire));

    releaseCallback.store(true, std::memory_order_release);
    query.join();
    registration.join();
    notifier.join();

    // A registration created during a notification is deliberately excluded
    // from that already-fixed notification set.
    assert(second.Calls.load(std::memory_order_relaxed) == 0);

    firstHandle.reset();
    secondHandle.reset();
}

void TestConcurrentUnregisterRemainsLifetimeBarrier() {
    auto observable = std::make_shared<TestObservable>();
    TestObserver observer;
    auto handle = observable->RegisterObserver(&observer);

    std::atomic<bool> callbackEntered{false};
    std::atomic<bool> releaseCallback{false};
    std::atomic<bool> unregisterReturned{false};

    std::thread notifier([&] {
        observable->Notify([&](IObserver* current) {
            if (current != &observer) return;
            callbackEntered.store(true, std::memory_order_release);
            while (!releaseCallback.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        });
    });

    while (!callbackEntered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::thread unregisterer([&] {
        handle->Unregister();
        unregisterReturned.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(!unregisterReturned.load(std::memory_order_acquire));

    releaseCallback.store(true, std::memory_order_release);
    notifier.join();
    unregisterer.join();

    assert(unregisterReturned.load(std::memory_order_acquire));
    assert(!observable->IsObserverRegistered(&observer));
    handle.reset();
}

void TestSelfUnregisterStillWorks() {
    auto observable = std::make_shared<TestObservable>();
    TestObserver observer;
    ObserverHandlePtr handle = observable->RegisterObserver(&observer);

    observable->Notify([&](IObserver* current) {
        if (current != &observer) return;
        observer.Calls.fetch_add(1, std::memory_order_relaxed);
        handle.reset();
    });

    assert(observer.Calls.load(std::memory_order_relaxed) == 1);
    assert(!observable->IsObserverRegistered(&observer));

    observable->Notify([&](IObserver*) {
        observer.Calls.fetch_add(1, std::memory_order_relaxed);
    });
    assert(observer.Calls.load(std::memory_order_relaxed) == 1);
}

} // namespace

int main() {
    TestRegistryAccessDoesNotWaitForCallback();
    TestConcurrentUnregisterRemainsLifetimeBarrier();
    TestSelfUnregisterStillWorks();
    return 0;
}
