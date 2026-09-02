#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>

#include <ESPressio_Memory.hpp>
#include <ESPressio_PolymorphicMemory.hpp>
#include "ESPressio_IObserver.hpp"

namespace ESPressio {

    namespace Observable {
        class IObservable;
        class IUntypedObservable;
        class Observable;
        class ObservableWithBuckets;
        class ObserverHandle;
        class ThreadSafeObservable;

        /// <summary>Base exception for failures reported by ESPressio Observable.</summary>
        class ObservableException : public std::runtime_error {
            public:
                using std::runtime_error::runtime_error;
        };

        /// <summary>Base exception for invalid or conflicting observer registrations.</summary>
        class ObserverRegistrationException : public ObservableException {
            public:
                using ObservableException::ObservableException;
        };

        /// <summary>Thrown when attempting to register a null observer pointer.</summary>
        class InvalidObserverRegistrationException : public ObserverRegistrationException {
            public:
                InvalidObserverRegistrationException()
                    : ObserverRegistrationException("Cannot register a null Observer pointer") {}
        };

        /// <summary>Thrown when an observer does not implement every interface requested by a typed registration.</summary>
        class ObserverInterfaceMismatchException : public ObserverRegistrationException {
            public:
                ObserverInterfaceMismatchException()
                    : ObserverRegistrationException(
                        "Observer does not implement every requested Observer interface") {}
        };

        /// <summary>Thrown when an observer is already registered with a different typed interface set.</summary>
        class ObserverRegistrationConflictException : public ObserverRegistrationException {
            public:
                ObserverRegistrationConflictException()
                    : ObserverRegistrationException(
                        "Observer is already registered with a different interface set") {}
        };

        /// <summary>Thrown when an observer is registered more than once against the same observable.</summary>
        class DuplicateObserverRegistrationException : public ObserverRegistrationException {
            public:
                DuplicateObserverRegistrationException()
                    : ObserverRegistrationException(
                        "Observer is already registered with this Observable") {}
        };

        /// <summary>Base exception for invalid observer-handle operations.</summary>
        class ObserverHandleException : public ObservableException {
            public:
                using ObservableException::ObservableException;
        };

        /// <summary>Thrown when an observer handle cannot be associated with a valid observable lifetime.</summary>
        class InvalidObservableHandleException : public ObserverHandleException {
            public:
                InvalidObservableHandleException()
                    : ObserverHandleException(
                        "Cannot construct an Observer Handle without a valid Observable lifetime") {}
        };

        /// <summary>Thrown when notification is attempted without shared ownership of the observable.</summary>
        class ObservableOwnershipException : public ObservableException {
            public:
                ObservableOwnershipException()
                    : ObservableException(
                        "Observable notifications require ownership by std::shared_ptr") {}
        };

        namespace Detail {
            class ObservableLifetimeControl {
                private:
                    mutable std::mutex _mutex;
                    std::condition_variable _condition;
                    IObservable* _observable;
                    std::size_t _activeOperations = 0;
                    bool _alive = true;

                public:
                    explicit ObservableLifetimeControl(IObservable* observable)
                        : _observable(observable) {}

                    IObservable* Acquire() {
                        std::lock_guard<std::mutex> lock(_mutex);
                        if (!_alive) { return nullptr; }
                        ++_activeOperations;
                        return _observable;
                    }

                    void Release() {
                        std::lock_guard<std::mutex> lock(_mutex);
                        if (--_activeOperations == 0) {
                            _condition.notify_all();
                        }
                    }

                    IObservable* Peek() const {
                        std::lock_guard<std::mutex> lock(_mutex);
                        return _alive ? _observable : nullptr;
                    }

                    void InvalidateAndWait() {
                        std::unique_lock<std::mutex> lock(_mutex);
                        _alive = false;
                        _observable = nullptr;
                        _condition.wait(lock, [this]() {
                            return _activeOperations == 0;
                        });
                    }
            };
        }

        /// <summary>RAII-compatible registration handle that can explicitly detach an observer from its observable.</summary>
        class IObserverHandle {
            public:
                IObserverHandle() = default;
                IObserverHandle(const IObserverHandle&) = delete;
                IObserverHandle& operator=(const IObserverHandle&) = delete;
                IObserverHandle(IObserverHandle&&) = delete;
                IObserverHandle& operator=(IObserverHandle&&) = delete;
                virtual ~IObserverHandle() = default;
                /// <summary>Detaches the associated observer registration; repeated calls are harmless.</summary>
                virtual void Unregister() = 0;
                /// <summary>Gets the associated observable while the registration and observable lifetime remain valid.</summary>
                virtual IObservable* GetObservable() = 0;
                /// <summary>Gets the associated observer while the registration remains valid.</summary>
                virtual IObserver* GetObserver() = 0;
        };

        /// <summary>Owning pointer to an observer registration handle whose concrete storage is managed through ESPressio System memory policy.</summary>
        using ObserverHandlePtr =
            System::Memory::PolymorphicUniquePtr<IObserverHandle>;

        /// <summary>Base contract for observable objects with lifetime-safe observer registration.</summary>
        /// <remarks>Notification-capable implementations require ownership by <c>std::shared_ptr</c> so callbacks cannot outlive the observable during dispatch.</remarks>
        class IObservable : public std::enable_shared_from_this<IObservable> {
            private:
                friend class ObserverHandle;
                std::shared_ptr<Detail::ObservableLifetimeControl> _lifetimeControl;

            protected:
                /// <summary>Gets the lifetime-control object used by observer handles.</summary>
                std::shared_ptr<Detail::ObservableLifetimeControl> GetLifetimeControl() const {
                    return _lifetimeControl;
                }

                /// <summary>Acquires shared ownership that keeps this observable alive for the duration of a notification.</summary>
                /// <exception cref="ObservableOwnershipException">Thrown when the observable is not owned by <c>std::shared_ptr</c>.</exception>
                std::shared_ptr<IObservable> AcquireNotificationLifetime() {
                    try {
                        return shared_from_this();
                    } catch (const std::bad_weak_ptr&) {
                        throw ObservableOwnershipException();
                    }
                }

                /// <summary>Invalidates observer-handle access and waits for in-flight lifetime operations to finish.</summary>
                void BeginObservableDestruction() noexcept {
                    _lifetimeControl->InvalidateAndWait();
                }

            public:
                /// <summary>Creates an observable with a shared lifetime-control object for its registration handles.</summary>
                IObservable()
                    : _lifetimeControl(
                        System::Memory::MakeShared<
                            Detail::ObservableLifetimeControl,
                            System::Memory::MemoryPolicy::ExternalPreferred
                        >(this)) {}
                IObservable(const IObservable&) = delete;
                IObservable& operator=(const IObservable&) = delete;
                IObservable(IObservable&&) = delete;
                IObservable& operator=(IObservable&&) = delete;
                virtual ~IObservable() {
                    BeginObservableDestruction();
                }
                /// <summary>Removes the supplied observer from this observable.</summary>
                virtual void UnregisterObserver(IObserver* observer) = 0;
                /// <summary>Indicates whether the supplied observer is currently registered.</summary>
                virtual bool IsObserverRegistered(IObserver* observer) = 0;
        };

        /// <summary>Observable contract that permits registration through the untyped <c>IObserver</c> base interface.</summary>
        class IUntypedObservable : public IObservable {
            public:
                virtual ~IUntypedObservable() = default;
                /// <summary>Registers an observer using the untyped base observer interface.</summary>
                virtual ObserverHandlePtr RegisterObserver(IObserver* observer) = 0;
        };

    }

}
