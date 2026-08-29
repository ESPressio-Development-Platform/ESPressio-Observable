#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_PolymorphicMemory.hpp>
#include "ESPressio_IObservable.hpp"
#include "ESPressio_IObserver.hpp"
#include "ESPressio_ObserverHandle.hpp"
#include "ESPressio_ObserverTypeKey.hpp"

namespace ESPressio {
namespace Observable {

namespace Detail {
    template<typename TObserver, typename... TInterfaces>
    struct AllObserverInterfacesConvertible;

    template<typename TObserver>
    struct AllObserverInterfacesConvertible<TObserver> : std::true_type {};

    template<typename TObserver, typename TInterface, typename... TRest>
    struct AllObserverInterfacesConvertible<TObserver, TInterface, TRest...>
        : std::integral_constant<
            bool,
            std::is_convertible<TObserver*, TInterface*>::value &&
            AllObserverInterfacesConvertible<TObserver, TRest...>::value
        > {};
}

/// <summary>Provides RTTI-free observer registration and synchronous notification dispatch.</summary>
/// <remarks>Typed observer interfaces are captured explicitly during registration. Instances must be owned by <c>std::shared_ptr</c> while notifications are executed.</remarks>
class Observable : public IUntypedObservable {
private:
    struct Registration {
        IObserverHandle* Handle;
        IObserver* Observer;
        const void* Identity;

        Registration(
            IObserverHandle* handle = nullptr,
            IObserver* observer = nullptr,
            const void* identity = nullptr
        ) : Handle(handle), Observer(observer), Identity(identity) {}
    };

    struct Binding {
        IObserverHandle* Handle;
        ObserverTypeKey Type;
        void* Interface;

        Binding(
            IObserverHandle* handle = nullptr,
            ObserverTypeKey type = nullptr,
            void* observerInterface = nullptr
        ) : Handle(handle), Type(type), Interface(observerInterface) {}
    };

    using RegistrationStorage = System::Memory::Vector<
        Registration,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;
    using BindingStorage = System::Memory::Vector<
        Binding,
        System::Memory::MemoryPolicy::ExternalPreferred
    >;

    RegistrationStorage _registrations;
    BindingStorage _bindings;
    std::size_t _notificationDepth = 0;
    bool _needsCompaction = false;

    template<typename TFirstInterface, typename TObserver>
    static IObserver* ResolveObserverBaseImpl(TObserver* observer, std::true_type) {
        return static_cast<IObserver*>(static_cast<TFirstInterface*>(observer));
    }

    template<typename TFirstInterface, typename TObserver>
    static IObserver* ResolveObserverBaseImpl(TObserver* observer, std::false_type) {
        static_assert(
            std::is_convertible<TObserver*, IObserver*>::value,
            "Observer must expose an unambiguous IObserver base when the first registered interface does not derive from IObserver"
        );
        return static_cast<IObserver*>(observer);
    }

    template<typename TFirstInterface, typename TObserver>
    static IObserver* ResolveObserverBase(TObserver* observer) {
        return ResolveObserverBaseImpl<TFirstInterface>(
            observer,
            typename std::is_base_of<IObserver, TFirstInterface>::type()
        );
    }

    template<typename TInterface, typename TObserver>
    void AddBinding(ObserverHandle* handle, TObserver* observer) {
        const ObserverTypeKey type = ObserverTypeKeyOf<TInterface>();
        for (const auto& binding : _bindings) {
            if (binding.Handle == handle && binding.Type == type) return;
        }
        _bindings.emplace_back(
            handle,
            type,
            static_cast<void*>(static_cast<TInterface*>(observer))
        );
    }

    template<typename TObserver, typename... TInterfaces>
    void AddBindings(ObserverHandle* handle, TObserver* observer) {
        const int unused[] = {0, (AddBinding<TInterfaces>(handle, observer), 0)...};
        (void)unused;
    }

    void Compact() {
        _registrations.erase(
            std::remove_if(
                _registrations.begin(), _registrations.end(),
                [](const Registration& registration) { return registration.Handle == nullptr; }
            ),
            _registrations.end()
        );
        _bindings.erase(
            std::remove_if(
                _bindings.begin(), _bindings.end(),
                [](const Binding& binding) { return binding.Handle == nullptr; }
            ),
            _bindings.end()
        );
        _needsCompaction = false;
    }

    void FinishNotification() {
        if (--_notificationDepth == 0 && _needsCompaction) Compact();
    }

    void RemoveHandle(IObserverHandle* handle, bool invalidate) {
        if (handle == nullptr) return;
        if (invalidate) static_cast<ObserverHandle*>(handle)->InvalidateRegistration();

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
                _registrations.begin(), _registrations.end(),
                [handle](const Registration& registration) { return registration.Handle == handle; }
            ),
            _registrations.end()
        );
        _bindings.erase(
            std::remove_if(
                _bindings.begin(), _bindings.end(),
                [handle](const Binding& binding) { return binding.Handle == handle; }
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
        const std::size_t bindingCount = _bindings.size();
        try {
            for (std::size_t index = 0; index < bindingCount; ++index) {
                const Binding& binding = _bindings[index];
                if (binding.Handle != nullptr && binding.Interface != nullptr && binding.Type == type) {
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
    /// <summary>Provides scoped access to the observers participating in one notification operation.</summary>
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
        /// <summary>Invokes a callback for every currently registered observer.</summary>
        template<class Callback>
        void WithObservers(Callback&& callback) {
            _observable.WithObserversUntyped(std::forward<Callback>(callback));
        }

        /// <summary>Invokes a callback for observers registered for the requested interface.</summary>
        /// <typeparam name="ObserverType">Observer interface to enumerate.</typeparam>
        template<class ObserverType, class Callback>
        void WithObservers(Callback&& callback) {
            _observable.template WithObserversTyped<ObserverType>(
                std::forward<Callback>(callback)
            );
        }
    };

    /// <summary>Executes a notification operation while retaining the Observable's shared lifetime.</summary>
    /// <typeparam name="Operation">Callable accepting a <c>NotificationContext&amp;</c>.</typeparam>
    template<class Operation>
    void ExecuteNotification(Operation&& operation) {
        std::shared_ptr<IObservable> lifetime = AcquireNotificationLifetime();
        if (_registrations.empty()) return;
        NotificationContext context(*this, std::move(lifetime));
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

    /// <summary>Registers an observer for an explicit set of observer interfaces without runtime type discovery.</summary>
    /// <typeparam name="ObserverInterfaces">Interfaces through which the observer may be notified.</typeparam>
    /// <typeparam name="TObserver">Concrete observer type.</typeparam>
    /// <param name="observer">Observer instance to register.</param>
    /// <returns>An RAII handle whose concrete storage prefers external memory and whose destruction unregisters the observer.</returns>
    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        static_assert(sizeof...(ObserverInterfaces) > 0, "At least one Observer interface must be specified");
        static_assert(
            Detail::AllObserverInterfacesConvertible<TObserver, ObserverInterfaces...>::value,
            "Observer does not implement every requested Observer interface"
        );
        if (observer == nullptr) throw InvalidObserverRegistrationException();

        typedef typename std::tuple_element<0, std::tuple<ObserverInterfaces...>>::type FirstInterface;
        IObserver* observerBase = ResolveObserverBase<FirstInterface>(observer);
        const void* identity = static_cast<const void*>(observer);

        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr &&
                (registration.Identity == identity || registration.Observer == observerBase)) {
                throw DuplicateObserverRegistrationException();
            }
        }

        _registrations.reserve(_registrations.size() + 1);
        _bindings.reserve(_bindings.size() + sizeof...(ObserverInterfaces));

        ObserverHandlePtr handle = System::Memory::MakePolymorphicUnique<
            IObserverHandle,
            ObserverHandle,
            System::Memory::MemoryPolicy::ExternalPreferred
        >(GetLifetimeControl(), observerBase);
        ObserverHandle* rawHandle = static_cast<ObserverHandle*>(handle.get());
        _registrations.emplace_back(rawHandle, observerBase, identity);

        try {
            AddBindings<TObserver, ObserverInterfaces...>(rawHandle, observer);
        } catch (...) {
            RemoveHandle(rawHandle, false);
            throw;
        }
        return handle;
    }

    /// <summary>Registers an observer through the untyped <c>IObserver</c> interface.</summary>
    ObserverHandlePtr RegisterObserver(IObserver* observer) override {
        return RegisterObserverAs<IObserver>(observer);
    }

    /// <summary>Unregisters an observer when it is currently registered.</summary>
    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;
        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr && registration.Observer == observer) {
                RemoveHandle(registration.Handle, true);
                return;
            }
        }
    }

    /// <summary>Determines whether an observer currently has an active registration.</summary>
    bool IsObserverRegistered(IObserver* observer) override {
        if (observer == nullptr) return false;
        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr && registration.Observer == observer) return true;
        }
        return false;
    }
};

} // namespace Observable
} // namespace ESPressio
