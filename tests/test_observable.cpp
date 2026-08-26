#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "ESPressio_Observable.hpp"
#include "ESPressio_ObservableWithBuckets.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

using namespace ESPressio::Observable;

static_assert(std::is_base_of<std::runtime_error, ObservableException>::value,
    "ObservableException must be a runtime_error");
static_assert(std::is_base_of<ObservableException, ObserverRegistrationException>::value,
    "Registration exceptions must be Observable exceptions");
static_assert(std::is_base_of<ObserverRegistrationException,
    InvalidObserverRegistrationException>::value,
    "Null registration must have a typed exception");
static_assert(std::is_base_of<ObserverRegistrationException,
    ObserverInterfaceMismatchException>::value,
    "Interface mismatches must be registration exceptions");
static_assert(std::is_base_of<ObserverRegistrationException,
    ObserverRegistrationConflictException>::value,
    "Registration conflicts must be registration exceptions");
static_assert(std::is_base_of<ObserverRegistrationException,
    DuplicateObserverRegistrationException>::value,
    "Duplicate registrations must be registration exceptions");
static_assert(std::is_base_of<ObservableException, ObserverHandleException>::value,
    "Handle exceptions must be Observable exceptions");
static_assert(std::is_base_of<ObserverHandleException,
    InvalidObservableHandleException>::value,
    "Invalid handle construction must have a typed exception");
static_assert(std::is_base_of<ObservableException, ObservableOwnershipException>::value,
    "Ownership failures must be Observable exceptions");
static_assert(std::is_base_of<IObservable, IUntypedObservable>::value,
    "IUntypedObservable must extend IObservable");
static_assert(std::is_base_of<IUntypedObservable, Observable>::value,
    "Observable must support untyped registration");
static_assert(std::is_base_of<IUntypedObservable, ThreadSafeObservable>::value,
    "ThreadSafeObservable must support untyped registration");
static_assert(std::is_base_of<IObservable, ObservableWithBuckets>::value,
    "ObservableWithBuckets must satisfy IObservable");
static_assert(!std::is_base_of<IUntypedObservable, ObservableWithBuckets>::value,
    "ObservableWithBuckets must not advertise untyped registration");
static_assert(!std::is_copy_constructible<IObservable>::value,
    "IObservable must not be copyable");
static_assert(!std::is_move_constructible<IObservable>::value,
    "IObservable must not be movable");
static_assert(!std::is_copy_constructible<IObserverHandle>::value,
    "IObserverHandle must not be copyable");
static_assert(!std::is_move_constructible<IObserverHandle>::value,
    "IObserverHandle must not be movable");
static_assert(std::has_virtual_destructor<IObserver>::value,
    "Observers must support destruction through IObserver");
static_assert(!std::is_constructible<ObserverHandle, IObservable*, IObserver*>::value,
    "Only Observable implementations may construct ObserverHandle");

namespace {

    struct InterfaceA {
        virtual ~InterfaceA() = default;
        virtual void OnA(int value) = 0;
    };

    struct InterfaceB {
        virtual ~InterfaceB() = default;
        virtual void OnB(int value) = 0;
    };

    struct InterfaceC {
        virtual ~InterfaceC() = default;
        virtual void OnC() = 0;
    };

    struct ObserverA final : IObserver, InterfaceA {
        int calls = 0;
        int value = 0;
        void OnA(int newValue) override { ++calls; value = newValue; }
    };

    struct ObserverAB final : IObserver, InterfaceA, InterfaceB {
        int callsA = 0;
        int callsB = 0;
        int valueA = 0;
        int valueB = 0;
        void OnA(int value) override { ++callsA; valueA = value; }
        void OnB(int value) override { ++callsB; valueB = value; }
    };

    struct PlainObserver final : IObserver {};

    struct SelfRemovingObserver final : IObserver, InterfaceA {
        ObserverHandlePtr* handle = nullptr;
        int calls = 0;
        void OnA(int) override {
            ++calls;
            if (handle != nullptr) { handle->reset(); }
        }
    };

    class TestObservable final : public Observable {
        public:
            void NotifyAll(const std::function<void(IObserver*)>& callback) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers(callback);
                });
            }

            void NotifyA(int value) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceA>(
                        [value](InterfaceA* observer) { observer->OnA(value); });
                });
            }

            void NotifyAndThen(const std::function<void()>& callback, bool& completed) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers(
                        [&](IObserver*) { callback(); });
                    completed = true;
                });
            }

            std::function<void(int)> DeferredNotifyA() {
                std::function<void(int)> deferred;
                ExecuteNotification([&](NotificationContext& notification) {
                    deferred = [notification](int value) mutable {
                        notification.WithObservers<InterfaceA>(
                            [value](InterfaceA* observer) { observer->OnA(value); });
                    };
                });
                return deferred;
            }
    };

    class ThrowingUnregisterObservable final : public Observable {
        public:
            bool throwOnUnregister = true;

            void UnregisterObserver(IObserver* observer) override {
                if (throwOnUnregister) {
                    throw std::runtime_error("unregistration failure");
                }
                Observable::UnregisterObserver(observer);
            }
    };

    class TestThreadSafeObservable final : public ThreadSafeObservable {
        public:
            void NotifyAll(const std::function<void(IObserver*)>& callback) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers(callback);
                });
            }

            void NotifyA(int value) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceA>(
                        [value](InterfaceA* observer) { observer->OnA(value); });
                });
            }
    };

    class TestBucketObservable final : public ObservableWithBuckets {
        public:
            void NotifyA(int value) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceA>(
                        [value](InterfaceA* observer) { observer->OnA(value); });
                });
            }

            void NotifyB(int value) {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceB>(
                        [value](InterfaceB* observer) { observer->OnB(value); });
                });
            }

            void NotifyC() {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceC>(
                        [](InterfaceC* observer) { observer->OnC(); });
                });
            }

            void NotifyThrow() {
                ExecuteNotification([&](NotificationContext& notification) {
                    notification.WithObservers<InterfaceA>([](InterfaceA*) {
                        throw std::runtime_error("bucket callback failure");
                    });
                });
            }
    };

    void TestObservableRegistrationAndDispatch() {
        auto observable = std::make_shared<TestObservable>();
        ObserverA observerA;
        PlainObserver plain;

        bool nullThrown = false;
        try { observable->RegisterObserver(nullptr); }
        catch (const InvalidObserverRegistrationException&) { nullThrown = true; }
        assert(nullThrown);

        ObserverHandlePtr handleA = observable->RegisterObserverAs<InterfaceA>(&observerA);
        bool duplicateThrown = false;
        try { observable->RegisterObserverAs<InterfaceA>(&observerA); }
        catch (const DuplicateObserverRegistrationException&) { duplicateThrown = true; }
        assert(duplicateThrown);
        ObserverHandlePtr plainHandle = observable->RegisterObserver(&plain);
        assert(handleA->GetObservable() == observable.get());
        assert(handleA->GetObserver() == &observerA);
        assert(observable->IsObserverRegistered(&observerA));
        assert(observable->IsObserverRegistered(&plain));
        assert(!observable->IsObserverRegistered(nullptr));

        int allCalls = 0;
        observable->NotifyAll([&](IObserver*) { ++allCalls; });
        assert(allCalls == 2);
        observable->NotifyA(42);
        assert(observerA.calls == 1 && observerA.value == 42);

        observable->UnregisterObserver(&plain);
        assert(!observable->IsObserverRegistered(&plain));
        assert(plainHandle->GetObserver() == nullptr);
        assert(plainHandle->GetObservable() == nullptr);
        plainHandle.reset();

        handleA->Unregister();
        handleA->Unregister();
        assert(!observable->IsObserverRegistered(&observerA));
        assert(handleA->GetObserver() == nullptr);
        handleA.reset();
    }

    void TestObservableExceptionsAndOwnership() {
        TestObservable unmanaged;
        bool ownershipThrown = false;
        try { unmanaged.NotifyAll([](IObserver*) {}); }
        catch (const ObservableOwnershipException&) { ownershipThrown = true; }
        assert(ownershipThrown);

        auto observable = std::make_shared<TestObservable>();
        PlainObserver observer;
        ObserverHandlePtr handle = observable->RegisterObserver(&observer);
        bool callbackThrown = false;
        try {
            observable->NotifyAll([](IObserver*) {
                throw std::runtime_error("callback failure");
            });
        } catch (const std::runtime_error&) { callbackThrown = true; }
        assert(callbackThrown);
        assert(observable->IsObserverRegistered(&observer));
        handle.reset();
    }

    void TestSharedNotificationLifetime() {
        auto observable = std::make_shared<TestObservable>();
        PlainObserver observer;
        ObserverHandlePtr handle = observable->RegisterObserver(&observer);
        bool operationCompleted = false;
        std::weak_ptr<TestObservable> weakObservable = observable;

        observable->NotifyAndThen([&]() {
            observable.reset();
            assert(!weakObservable.expired());
        }, operationCompleted);

        assert(operationCompleted);
        assert(weakObservable.expired());
        assert(handle->GetObservable() == nullptr);
        handle.reset();
    }

    void TestHandleOutlivesObservable() {
        PlainObserver observer;
        ObserverHandlePtr handle;
        {
            auto observable = std::make_shared<TestObservable>();
            handle = observable->RegisterObserver(&observer);
        }
        assert(handle->GetObservable() == nullptr);
        handle->Unregister();
        assert(handle->GetObserver() == nullptr);
        handle.reset();
    }

    void TestUnregisterExceptionRestoresHandle() {
        auto observable = std::make_shared<ThrowingUnregisterObservable>();
        PlainObserver observer;
        ObserverHandlePtr handle = observable->RegisterObserver(&observer);
        bool thrown = false;
        try { handle->Unregister(); }
        catch (const std::runtime_error&) { thrown = true; }
        assert(thrown);
        assert(handle->GetObservable() == observable.get());
        assert(handle->GetObserver() == &observer);
        assert(observable->IsObserverRegistered(&observer));
        observable->throwOnUnregister = false;
        handle.reset();
        assert(!observable->IsObserverRegistered(&observer));
    }

    void TestRetainedNotificationContext() {
        auto observable = std::make_shared<TestObservable>();
        ObserverA observer;
        ObserverHandlePtr handle = observable->RegisterObserverAs<InterfaceA>(&observer);
        std::weak_ptr<TestObservable> weakObservable = observable;
        std::function<void(int)> deferred = observable->DeferredNotifyA();
        observable.reset();
        assert(!weakObservable.expired());
        deferred(91);
        assert(observer.calls == 1 && observer.value == 91);
        deferred = std::function<void(int)>();
        assert(weakObservable.expired());
        handle.reset();
    }

    void TestThreadSafeReentrancyAndExceptions() {
        auto observable = std::make_shared<TestThreadSafeObservable>();
        PlainObserver first;
        PlainObserver second;
        PlainObserver third;
        ObserverHandlePtr firstHandle = observable->RegisterObserver(&first);
        ObserverHandlePtr secondHandle = observable->RegisterObserver(&second);
        ObserverHandlePtr thirdHandle;
        int calls = 0;

        bool nullThrown = false;
        try { observable->RegisterObserver(nullptr); }
        catch (const InvalidObserverRegistrationException&) { nullThrown = true; }
        assert(nullThrown);
        bool duplicateThrown = false;
        try { observable->RegisterObserver(&first); }
        catch (const DuplicateObserverRegistrationException&) { duplicateThrown = true; }
        assert(duplicateThrown);

        observable->NotifyAll([&](IObserver* observer) {
            ++calls;
            if (observer == &first) {
                secondHandle.reset();
                thirdHandle = observable->RegisterObserver(&third);
            }
        });
        assert(calls == 1);
        assert(!observable->IsObserverRegistered(&second));
        assert(observable->IsObserverRegistered(&third));

        calls = 0;
        observable->NotifyAll([&](IObserver*) { ++calls; });
        assert(calls == 2);

        bool callbackThrown = false;
        try {
            observable->NotifyAll([](IObserver*) {
                throw std::logic_error("expected");
            });
        } catch (const std::logic_error&) { callbackThrown = true; }
        assert(callbackThrown);
        assert(observable->IsObserverRegistered(&first));

        firstHandle.reset();
        thirdHandle.reset();
    }

    void TestThreadSafeTypedFiltering() {
        auto observable = std::make_shared<TestThreadSafeObservable>();
        ObserverA observerA;
        PlainObserver plain;
        ObserverHandlePtr handleA = observable->RegisterObserverAs<InterfaceA>(&observerA);
        ObserverHandlePtr plainHandle = observable->RegisterObserver(&plain);
        observable->NotifyA(73);
        assert(observerA.calls == 1 && observerA.value == 73);
        observable->UnregisterObserver(nullptr);
        assert(!observable->IsObserverRegistered(nullptr));
        handleA.reset();
        plainHandle.reset();
    }

    void TestThreadSafeConcurrentUnregister() {
        auto observable = std::make_shared<TestThreadSafeObservable>();
        PlainObserver observer;
        ObserverHandlePtr handle = observable->RegisterObserver(&observer);
        std::atomic<bool> callbackEntered{false};
        std::atomic<bool> releaseCallback{false};
        std::atomic<bool> unregisterFinished{false};

        std::thread notifier([&]() {
            observable->NotifyAll([&](IObserver*) {
                callbackEntered.store(true);
                while (!releaseCallback.load()) { std::this_thread::yield(); }
            });
        });
        while (!callbackEntered.load()) { std::this_thread::yield(); }

        std::thread unregisterer([&]() {
            handle->Unregister();
            unregisterFinished.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        assert(!unregisterFinished.load());
        releaseCallback.store(true);
        notifier.join();
        unregisterer.join();
        assert(unregisterFinished.load());
        assert(!observable->IsObserverRegistered(&observer));
        handle.reset();
    }

    void TestThreadSafeStress() {
        auto observable = std::make_shared<TestThreadSafeObservable>();
        ObserverA observer;
        ObserverHandlePtr handle = observable->RegisterObserverAs<InterfaceA>(&observer);
        std::atomic<bool> stop{false};

        std::thread notifier([&]() {
            while (!stop.load()) { observable->NotifyA(7); }
        });
        std::thread reader([&]() {
            for (int index = 0; index < 1000; ++index) {
                (void)observable->IsObserverRegistered(&observer);
            }
            stop.store(true);
        });
        reader.join();
        notifier.join();
        handle->Unregister();
        handle.reset();
    }

    void TestConcurrentHandleAndObservableDestruction() {
        for (int iteration = 0; iteration < 250; ++iteration) {
            PlainObserver observer;
            auto observable = std::make_shared<TestThreadSafeObservable>();
            ObserverHandlePtr handle = observable->RegisterObserver(&observer);
            std::atomic<bool> start{false};
            std::thread destroyHandle([&start, handle = std::move(handle)]() mutable {
                while (!start.load()) { std::this_thread::yield(); }
                handle.reset();
            });
            std::thread destroyObservable([&]() {
                while (!start.load()) { std::this_thread::yield(); }
                observable.reset();
            });
            start.store(true);
            destroyHandle.join();
            destroyObservable.join();
        }
    }

    void TestBucketRegistrationAndDispatch() {
        auto observable = std::make_shared<TestBucketObservable>();
        ObserverAB observer;

        bool nullThrown = false;
        try { observable->RegisterObserverAs<InterfaceA>(nullptr); }
        catch (const InvalidObserverRegistrationException&) { nullThrown = true; }
        assert(nullThrown);

        bool mismatchThrown = false;
        try { observable->RegisterObserverAs<InterfaceC>(&observer); }
        catch (const ObserverInterfaceMismatchException&) { mismatchThrown = true; }
        assert(mismatchThrown);
        assert(!observable->IsObserverRegistered(&observer));

        ObserverHandlePtr handle =
            observable->RegisterObserverAs<InterfaceA, InterfaceB, InterfaceA>(&observer);
        bool duplicateThrown = false;
        try { observable->RegisterObserverAs<InterfaceB, InterfaceA>(&observer); }
        catch (const DuplicateObserverRegistrationException&) { duplicateThrown = true; }
        assert(duplicateThrown);
        assert(observable->IsObserverRegistered(&observer));
        observable->NotifyA(12);
        observable->NotifyB(34);
        observable->NotifyC();
        assert(observer.callsA == 1 && observer.valueA == 12);
        assert(observer.callsB == 1 && observer.valueB == 34);

        bool conflictThrown = false;
        try { observable->RegisterObserverAs<InterfaceA>(&observer); }
        catch (const ObserverRegistrationConflictException&) { conflictThrown = true; }
        assert(conflictThrown);

        observable->UnregisterObserver(&observer);
        observable->UnregisterObserver(&observer);
        assert(!observable->IsObserverRegistered(&observer));
        assert(handle->GetObserver() == nullptr);
        observable->NotifyA(56);
        assert(observer.callsA == 1);
        handle.reset();

        ObserverA observerA;
        ObserverHandlePtr handleA = observable->RegisterObserverAs<InterfaceA>(&observerA);
        handleA->Unregister();
        handleA->Unregister();
        assert(!observable->IsObserverRegistered(&observerA));
        handleA.reset();
    }

    void TestBucketExceptionsAndOwnership() {
        auto observable = std::make_shared<TestBucketObservable>();
        ObserverA observer;
        ObserverHandlePtr handle = observable->RegisterObserverAs<InterfaceA>(&observer);
        bool callbackThrown = false;
        try {
            observable->NotifyThrow();
        } catch (const std::runtime_error&) { callbackThrown = true; }
        assert(callbackThrown);
        assert(observable->IsObserverRegistered(&observer));
        handle.reset();

        TestBucketObservable unmanaged;
        bool ownershipThrown = false;
        try { unmanaged.NotifyA(1); }
        catch (const ObservableOwnershipException&) { ownershipThrown = true; }
        assert(ownershipThrown);
    }

    void TestMutationDuringNotification() {
        {
            auto observable = std::make_shared<TestObservable>();
            SelfRemovingObserver observer;
            ObserverHandlePtr handle = observable->RegisterObserverAs<InterfaceA>(&observer);
            observer.handle = &handle;
            observable->NotifyA(1);
            observable->NotifyA(2);
            assert(observer.calls == 1);
            assert(!observable->IsObserverRegistered(&observer));
        }

        {
            auto observable = std::make_shared<TestBucketObservable>();
            SelfRemovingObserver observer;
            ObserverHandlePtr handle =
                observable->RegisterObserverAs<InterfaceA>(&observer);
            observer.handle = &handle;
            observable->NotifyA(1);
            observable->NotifyA(2);
            assert(observer.calls == 1);
            assert(!observable->IsObserverRegistered(&observer));
        }
    }

}

int main() {
    TestObservableRegistrationAndDispatch();
    TestObservableExceptionsAndOwnership();
    TestSharedNotificationLifetime();
    TestHandleOutlivesObservable();
    TestUnregisterExceptionRestoresHandle();
    TestRetainedNotificationContext();
    TestThreadSafeReentrancyAndExceptions();
    TestThreadSafeTypedFiltering();
    TestThreadSafeConcurrentUnregister();
    TestThreadSafeStress();
    TestConcurrentHandleAndObservableDestruction();
    TestBucketRegistrationAndDispatch();
    TestBucketExceptionsAndOwnership();
    TestMutationDuringNotification();
}
