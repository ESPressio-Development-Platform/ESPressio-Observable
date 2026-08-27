#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>

#include <ESPressio_Memory.hpp>
#include "ESPressio_IObserver.hpp"

namespace ESPressio {

    namespace Observable {
        class IObservable;
        class IUntypedObservable;
        class Observable;
        class ObservableWithBuckets;
        class ObserverHandle;
        class ThreadSafeObservable;

        class ObservableException : public std::runtime_error {
            public:
                using std::runtime_error::runtime_error;
        };

        class ObserverRegistrationException : public ObservableException {
            public:
                using ObservableException::ObservableException;
        };

        class InvalidObserverRegistrationException : public ObserverRegistrationException {
            public:
                InvalidObserverRegistrationException()
                    : ObserverRegistrationException("Cannot register a null Observer pointer") {}
        };

        class ObserverInterfaceMismatchException : public ObserverRegistrationException {
            public:
                ObserverInterfaceMismatchException()
                    : ObserverRegistrationException(
                        "Observer does not implement every requested Observer interface") {}
        };

        class ObserverRegistrationConflictException : public ObserverRegistrationException {
            public:
                ObserverRegistrationConflictException()
                    : ObserverRegistrationException(
                        "Observer is already registered with a different interface set") {}
        };

        class DuplicateObserverRegistrationException : public ObserverRegistrationException {
            public:
                DuplicateObserverRegistrationException()
                    : ObserverRegistrationException(
                        "Observer is already registered with this Observable") {}
        };

        class ObserverHandleException : public ObservableException {
            public:
                using ObservableException::ObservableException;
        };

        class InvalidObservableHandleException : public ObserverHandleException {
            public:
                InvalidObservableHandleException()
                    : ObserverHandleException(
                        "Cannot construct an Observer Handle without a valid Observable lifetime") {}
        };

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

        class IObserverHandle {
            public:
                IObserverHandle() = default;
                IObserverHandle(const IObserverHandle&) = delete;
                IObserverHandle& operator=(const IObserverHandle&) = delete;
                IObserverHandle(IObserverHandle&&) = delete;
                IObserverHandle& operator=(IObserverHandle&&) = delete;
                virtual ~IObserverHandle() = default;
                virtual void Unregister() = 0;
                virtual IObservable* GetObservable() = 0;
                virtual IObserver* GetObserver() = 0;
        };

        using ObserverHandlePtr = std::unique_ptr<IObserverHandle>;

        class IObservable : public std::enable_shared_from_this<IObservable> {
            private:
                friend class ObserverHandle;
                std::shared_ptr<Detail::ObservableLifetimeControl> _lifetimeControl;

            protected:
                std::shared_ptr<Detail::ObservableLifetimeControl> GetLifetimeControl() const {
                    return _lifetimeControl;
                }

                std::shared_ptr<IObservable> AcquireNotificationLifetime() {
                    try {
                        return shared_from_this();
                    } catch (const std::bad_weak_ptr&) {
                        throw ObservableOwnershipException();
                    }
                }

                void BeginObservableDestruction() noexcept {
                    _lifetimeControl->InvalidateAndWait();
                }

            public:
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
                virtual void UnregisterObserver(IObserver* observer) = 0;
                virtual bool IsObserverRegistered(IObserver* observer) = 0;
        };

        class IUntypedObservable : public IObservable {
            public:
                virtual ~IUntypedObservable() = default;
                virtual ObserverHandlePtr RegisterObserver(IObserver* observer) = 0;
        };

    }

}
