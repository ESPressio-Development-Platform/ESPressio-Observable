#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>

#include <ESPressio_Memory.hpp>
#include "ESPressio_IObservable.hpp"
#include "ESPressio_IObserver.hpp"

namespace ESPressio {

    namespace Observable {

        /// <summary>RAII registration handle that safely disconnects an observer from its Observable.</summary>
        /// <remarks>The handle tracks Observable lifetime independently so destruction remains safe if the Observable has already been destroyed.</remarks>
        class ObserverHandle : public IObserverHandle {
            private:
                friend class Observable;
                friend class ObservableWithBuckets;
                friend class ThreadSafeObservable;

                std::shared_ptr<Detail::ObservableLifetimeControl> _lifetimeControl;
                std::atomic<IObserver*> _observer;
                std::atomic<bool> _registered{true};

                static std::shared_ptr<Detail::ObservableLifetimeControl>
                GetValidatedLifetimeControl(IObservable* observable) {
                    if (observable == nullptr) throw InvalidObservableHandleException();
                    return observable->GetLifetimeControl();
                }

                static std::shared_ptr<Detail::ObservableLifetimeControl>
                GetValidatedLifetimeControl(
                    std::shared_ptr<Detail::ObservableLifetimeControl> lifetimeControl) {
                    if (!lifetimeControl) throw InvalidObservableHandleException();
                    return lifetimeControl;
                }

                static IObserver* GetValidatedObserver(IObserver* observer) {
                    if (observer == nullptr) throw InvalidObserverRegistrationException();
                    return observer;
                }

                void InvalidateRegistration() noexcept {
                    _registered.store(false);
                    _observer.store(nullptr);
                }

            private:
                ObserverHandle(IObservable* observable, IObserver* observer)
                    : ObserverHandle(GetValidatedLifetimeControl(observable), observer) {}

                ObserverHandle(
                    std::shared_ptr<Detail::ObservableLifetimeControl> lifetimeControl,
                    IObserver* observer)
                    : _lifetimeControl(GetValidatedLifetimeControl(std::move(lifetimeControl))),
                      _observer(GetValidatedObserver(observer)) {}

            public:
                static void* operator new(std::size_t bytes) {
                    return System::Memory::GetProvider().Allocate(
                        bytes,
                        alignof(ObserverHandle),
                        System::Memory::MemoryPolicy::ExternalPreferred
                    );
                }

                static void operator delete(void* pointer) noexcept {
                    System::Memory::GetProvider().Deallocate(
                        pointer,
                        sizeof(ObserverHandle),
                        alignof(ObserverHandle),
                        System::Memory::MemoryPolicy::ExternalPreferred
                    );
                }

                ObserverHandle(const ObserverHandle&) = delete;
                ObserverHandle& operator=(const ObserverHandle&) = delete;
                ObserverHandle(ObserverHandle&&) = delete;
                ObserverHandle& operator=(ObserverHandle&&) = delete;

                ~ObserverHandle() noexcept override {
                    try { Unregister(); } catch (...) {}
                }

                /// <summary>Unregisters the associated observer once; subsequent calls are no-ops.</summary>
                void Unregister() override {
                    IObserver* observer = _observer.load();
                    if (!_registered.exchange(false)) return;

                    IObservable* observable = _lifetimeControl->Acquire();
                    if (observable == nullptr) {
                        _observer.store(nullptr);
                        return;
                    }

                    try {
                        observable->UnregisterObserver(observer);
                    } catch (...) {
                        _lifetimeControl->Release();
                        _observer.store(observer);
                        _registered.store(true);
                        throw;
                    }
                    _lifetimeControl->Release();
                    _observer.store(nullptr);
                }

                /// <summary>Returns the associated Observable while the registration and Observable are still alive.</summary>
                /// <returns>The Observable pointer, or <c>nullptr</c> after unregistration or Observable destruction.</returns>
                IObservable* GetObservable() override {
                    if (!_registered.load()) return nullptr;
                    return _lifetimeControl->Peek();
                }

                /// <summary>Returns the registered observer while this handle remains active.</summary>
                /// <returns>The observer pointer, or <c>nullptr</c> once the registration has been invalidated.</returns>
                IObserver* GetObserver() override {
                    return _observer.load();
                }
        };

    }

}
