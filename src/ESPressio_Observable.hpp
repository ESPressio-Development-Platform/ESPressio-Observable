#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "ESPressio_IObservable.hpp"
#include "ESPressio_IObserver.hpp"
#include "ESPressio_ObserverHandle.hpp"
#include "ESPressio_ObserverTypeKey.hpp"

namespace ESPressio {
namespace Observable {

/// Non-thread-safe Observable with explicit compiler-backed typed registration.
/// Typed dispatch performs no RTTI and no dynamic casts.
class Observable : public IUntypedObservable {
private:
    struct Registration {
        IObserverHandle* Handle = nullptr;
        IObserver* Observer = nullptr;
        const void* Identity = nullptr;
    };

    struct Binding {
        IObserverHandle* Handle = nullptr;
        ObserverTypeKey Type = nullptr;
        void* Interface = nullptr;
    };

    std::vector<Registration> _registrations;
    std::vector<Binding> _bindings;
    std::size_t _notificationDepth = 0;
    bool _needsCompaction = false;

    template<typename TFirstInterface, typename TObserver>
    static IObserver* ResolveObserverBase(TObserver* observer) {
        if constexpr (std::is_base_of_v<IObserver, TFirstInterface>) {
            return static_cast<IObserver*>(
                static_cast<TFirstInterface*>(observer)
            );
        } else {
            static_assert(
                std::is_convertible_v<TObserver*, IObserver*>,
                "Observer must expose an unambiguous IObserver base when the first registered interface does not derive from IObserver"
            );
            return static_cast<IObserver*>(observer);
        }
    }

    void Compact() {
        _registrations.erase(
            std::remove_if(
                _registrations.begin(),
                _registrations.end(),
                [](const Registration& registration) {
                    return registration.Handle == nullptr;
                }
            ),
            _registrations.end()
        );
        _bindings.erase(
            std::remove_if(
                _bindings.begin(),
                _bindings.end(),
                [](const Binding& binding) {
                    return binding.Handle == nullptr;
                }
            ),
            _bindings.end()
        );
        _needsCompaction = false;
    }

    void FinishNotification() {
        if (--_notificationDepth == 0 && _needsCompaction) {
            Compact();
        }
    }

    void RemoveHandle(IObserverHandle* handle, bool invalidate) {
        if (handle == nullptr) return;
        if (invalidate) {
            static_cast<ObserverHandle*>(handle)->InvalidateRegistration();
        }

        if (_notificationDepth > 0) {
            for (auto& registration : _registrations) {
                if (registration.Handle == handle) {
                    registration.Handle = nullptr;
                    registration.Observer = nullptr;
                    registration.Identity = nullptr;
                }
            }
            for (auto& binding : _bindings) {
                if (binding.Handle == handle) {
                    binding.Handle = nullptr;
                    binding.Interface = nullptr;
                    binding.Type = nullptr;
                }
            }
            _needsCompaction = true;
            return;
        }

        _registrations.erase(
            std::remove_if(
                _registrations.begin(),
                _registrations.end(),
                [handle](const Registration& registration) {
                    return registration.Handle == handle;
                }
            ),
            _registrations.end()
        );
        _bindings.erase(
            std::remove_if(
                _bindings.begin(),
                _bindings.end(),
                [handle](const Binding& binding) {
                    return binding.Handle == handle;
                }
            ),
            _bindings.end()
        );
    }

    template<class Callback>
    void WithObserversUntyped(Callback&& callback) {
        ++_notificationDepth;
        const std::size_t count = _registrations.size();
        try {
            for (std::size_t index = 0; index < count; ++index) {
                const Registration& registration = _registrations[index];
                if (registration.Handle != nullptr && registration.Observer != nullptr) {
                    callback(registration.Observer);
                }
            }
        } catch (...) {
            FinishNotification();
            throw;
        }
        FinishNotification();
    }

    template<class ObserverType, class Callback>
    void WithObserversTyped(Callback&& callback) {
        const ObserverTypeKey type = ObserverTypeKeyOf<ObserverType>();
        ++_notificationDepth;
        const std::size_t count = _bindings.size();
        try {
            for (std::size_t index = 0; index < count; ++index) {
                const Binding& binding = _bindings[index];
                if (
                    binding.Handle != nullptr &&
                    binding.Interface != nullptr &&
                    binding.Type == type
                ) {
                    callback(static_cast<ObserverType*>(binding.Interface));
                }
            }
        } catch (...) {
            FinishNotification();
            throw;
        }
        FinishNotification();
    }

protected:
    class NotificationContext {
    private:
        friend class Observable;
        Observable& _observable;
        std::shared_ptr<IObservable> _notificationLifetime;

        NotificationContext(
            Observable& observable,
            std::shared_ptr<IObservable> notificationLifetime
        ) : _observable(observable),
            _notificationLifetime(std::move(notificationLifetime)) {}

    public:
        template<class Callback>
        void WithObservers(Callback&& callback) {
            _observable.WithObserversUntyped(std::forward<Callback>(callback));
        }

        template<class ObserverType, class Callback>
        void WithObservers(Callback&& callback) {
            _observable.template WithObserversTyped<ObserverType>(
                std::forward<Callback>(callback)
            );
        }
    };

    template<class Operation>
    void ExecuteNotification(Operation&& operation) {
        if (_registrations.empty()) return;
        NotificationContext context(*this, AcquireNotificationLifetime());
        operation(context);
    }

public:
    ~Observable() override {
        BeginObservableDestruction();
        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr) {
                static_cast<ObserverHandle*>(registration.Handle)->InvalidateRegistration();
            }
        }
        _bindings.clear();
        _registrations.clear();
    }

    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        static_assert(
            sizeof...(ObserverInterfaces) > 0,
            "At least one Observer interface must be specified"
        );
        static_assert(
            (std::is_convertible_v<TObserver*, ObserverInterfaces*> && ...),
            "Observer does not implement every requested Observer interface"
        );

        if (observer == nullptr) {
            throw InvalidObserverRegistrationException();
        }

        using FirstInterface = std::tuple_element_t<
            0,
            std::tuple<ObserverInterfaces...>
        >;
        IObserver* observerBase = ResolveObserverBase<FirstInterface>(observer);
        const void* identity = static_cast<const void*>(observer);

        for (const auto& registration : _registrations) {
            if (
                registration.Handle != nullptr &&
                (registration.Identity == identity || registration.Observer == observerBase)
            ) {
                throw DuplicateObserverRegistrationException();
            }
        }

        _registrations.reserve(_registrations.size() + 1);
        _bindings.reserve(_bindings.size() + sizeof...(ObserverInterfaces));

        std::unique_ptr<ObserverHandle> handle(
            new ObserverHandle(GetLifetimeControl(), observerBase)
        );
        ObserverHandle* rawHandle = handle.get();
        _registrations.push_back(Registration{rawHandle, observerBase, identity});

        try {
            (
                _bindings.push_back(Binding{
                    rawHandle,
                    ObserverTypeKeyOf<ObserverInterfaces>(),
                    static_cast<void*>(static_cast<ObserverInterfaces*>(observer))
                }),
                ...
            );
        } catch (...) {
            RemoveHandle(rawHandle, false);
            throw;
        }

        return ObserverHandlePtr(handle.release());
    }

    ObserverHandlePtr RegisterObserver(IObserver* observer) override {
        return RegisterObserverAs<IObserver>(observer);
    }

    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;
        for (const auto& registration : _registrations) {
            if (
                registration.Handle != nullptr &&
                registration.Observer == observer
            ) {
                RemoveHandle(registration.Handle, true);
                return;
            }
        }
    }

    bool IsObserverRegistered(IObserver* observer) override {
        if (observer == nullptr) return false;
        for (const auto& registration : _registrations) {
            if (
                registration.Handle != nullptr &&
                registration.Observer == observer
            ) {
                return true;
            }
        }
        return false;
    }
};

} // namespace Observable
} // namespace ESPressio
