#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include "ESPressio_Observable.hpp"
#include "ESPressio_ObserverTypeKey.hpp"

namespace ESPressio {
namespace Observable {

class ObservableWithBuckets : public IObservable {
private:
    struct Registration {
        IObserverHandle* Handle;
        IObserver* Observer;
        const void* Identity;
        Registration(IObserverHandle* handle = nullptr, IObserver* observer = nullptr, const void* identity = nullptr)
            : Handle(handle), Observer(observer), Identity(identity) {}
    };

    struct Binding {
        IObserverHandle* Handle;
        ObserverTypeKey Type;
        void* Interface;
        Binding(IObserverHandle* handle = nullptr, ObserverTypeKey type = nullptr, void* observerInterface = nullptr)
            : Handle(handle), Type(type), Interface(observerInterface) {}
    };

    using RegistrationStorage = System::Memory::Vector<Registration, System::Memory::MemoryPolicy::ExternalPreferred>;
    using BindingStorage = System::Memory::Vector<Binding, System::Memory::MemoryPolicy::ExternalPreferred>;
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
        static_assert(std::is_convertible<TObserver*, IObserver*>::value,
            "Observer must expose an unambiguous IObserver base when the first registered interface does not derive from IObserver");
        return static_cast<IObserver*>(observer);
    }

    template<typename TFirstInterface, typename TObserver>
    static IObserver* ResolveObserverBase(TObserver* observer) {
        return ResolveObserverBaseImpl<TFirstInterface>(observer, typename std::is_base_of<IObserver, TFirstInterface>::type());
    }

    template<typename TInterface, typename TObserver>
    void AddBinding(ObserverHandle* handle, TObserver* observer) {
        const ObserverTypeKey type = ObserverTypeKeyOf<TInterface>();
        for (const auto& binding : _bindings) {
            if (binding.Handle == handle && binding.Type == type) return;
        }
        _bindings.emplace_back(handle, type, static_cast<void*>(static_cast<TInterface*>(observer)));
    }

    template<typename TObserver, typename... TInterfaces>
    void AddBindings(ObserverHandle* handle, TObserver* observer) {
        const int unused[] = {0, (AddBinding<TInterfaces>(handle, observer), 0)...};
        (void)unused;
    }

    template<typename... TInterfaces>
    bool HasSameInterfaceSet(IObserverHandle* handle) const {
        const ObserverTypeKey requested[] = {ObserverTypeKeyOf<TInterfaces>()...};
        std::size_t uniqueRequested = 0;
        for (std::size_t index = 0; index < sizeof...(TInterfaces); ++index) {
            bool seen = false;
            for (std::size_t earlier = 0; earlier < index; ++earlier) {
                if (requested[earlier] == requested[index]) { seen = true; break; }
            }
            if (!seen) ++uniqueRequested;
        }
        std::size_t existing = 0;
        for (const auto& binding : _bindings) {
            if (binding.Handle == handle && binding.Type != nullptr) ++existing;
        }
        if (existing != uniqueRequested) return false;
        for (std::size_t index = 0; index < sizeof...(TInterfaces); ++index) {
            bool found = false;
            for (const auto& binding : _bindings) {
                if (binding.Handle == handle && binding.Type == requested[index]) { found = true; break; }
            }
            if (!found) return false;
        }
        return true;
    }

    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAsImpl(TObserver*, std::false_type) {
        throw ObserverInterfaceMismatchException();
    }

    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAsImpl(TObserver* observer, std::true_type) {
        if (observer == nullptr) throw InvalidObserverRegistrationException();
        typedef typename std::tuple_element<0, std::tuple<ObserverInterfaces...>>::type FirstInterface;
        IObserver* observerBase = ResolveObserverBase<FirstInterface>(observer);
        const void* identity = static_cast<const void*>(observer);

        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr &&
                (registration.Identity == identity || registration.Observer == observerBase)) {
                if (HasSameInterfaceSet<ObserverInterfaces...>(registration.Handle)) {
                    throw DuplicateObserverRegistrationException();
                }
                throw ObserverRegistrationConflictException();
            }
        }

        _registrations.reserve(_registrations.size() + 1);
        _bindings.reserve(_bindings.size() + sizeof...(ObserverInterfaces));
        std::unique_ptr<ObserverHandle> handle(new ObserverHandle(GetLifetimeControl(), observerBase));
        ObserverHandle* rawHandle = handle.get();
        _registrations.emplace_back(rawHandle, observerBase, identity);
        try {
            AddBindings<TObserver, ObserverInterfaces...>(rawHandle, observer);
        } catch (...) {
            RemoveHandle(rawHandle, false);
            throw;
        }
        return ObserverHandlePtr(handle.release());
    }

    void Compact() {
        _registrations.erase(std::remove_if(_registrations.begin(), _registrations.end(),
            [](const Registration& registration) { return registration.Handle == nullptr; }), _registrations.end());
        _bindings.erase(std::remove_if(_bindings.begin(), _bindings.end(),
            [](const Binding& binding) { return binding.Handle == nullptr; }), _bindings.end());
        _needsCompaction = false;
    }

    void FinishNotification() { if (--_notificationDepth == 0 && _needsCompaction) Compact(); }

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
                    binding.Type = nullptr;
                    binding.Interface = nullptr;
                }
            }
            _needsCompaction = true;
            return;
        }
        _registrations.erase(std::remove_if(_registrations.begin(), _registrations.end(),
            [handle](const Registration& registration) { return registration.Handle == handle; }), _registrations.end());
        _bindings.erase(std::remove_if(_bindings.begin(), _bindings.end(),
            [handle](const Binding& binding) { return binding.Handle == handle; }), _bindings.end());
    }

    template<typename ObserverType, typename Callback>
    void WithObserversTyped(Callback&& callback) {
        const ObserverTypeKey type = ObserverTypeKeyOf<ObserverType>();
        ++_notificationDepth;
        const std::size_t count = _bindings.size();
        try {
            for (std::size_t index = 0; index < count; ++index) {
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
    class NotificationContext {
        friend class ObservableWithBuckets;
        ObservableWithBuckets& _observable;
        std::shared_ptr<IObservable> _notificationLifetime;
        NotificationContext(ObservableWithBuckets& observable, std::shared_ptr<IObservable> notificationLifetime)
            : _observable(observable), _notificationLifetime(std::move(notificationLifetime)) {}
    public:
        template<typename ObserverType, typename Callback>
        void WithObservers(Callback&& callback) {
            _observable.template WithObserversTyped<ObserverType>(std::forward<Callback>(callback));
        }
    };

    template<typename Operation>
    void ExecuteNotification(Operation&& operation) {
        std::shared_ptr<IObservable> lifetime = AcquireNotificationLifetime();
        if (_registrations.empty()) return;
        NotificationContext context(*this, std::move(lifetime));
        operation(context);
    }

public:
    ~ObservableWithBuckets() override {
        BeginObservableDestruction();
        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr) static_cast<ObserverHandle*>(registration.Handle)->InvalidateRegistration();
        }
        _bindings.clear();
        _registrations.clear();
    }

    template<typename... ObserverInterfaces>
    ObserverHandlePtr RegisterObserverAs(std::nullptr_t) {
        static_assert(sizeof...(ObserverInterfaces) > 0, "At least one Observer interface must be specified");
        throw InvalidObserverRegistrationException();
    }

    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        static_assert(sizeof...(ObserverInterfaces) > 0, "At least one Observer interface must be specified");
        return RegisterObserverAsImpl<ObserverInterfaces...>(
            observer,
            typename Detail::AllObserverInterfacesConvertible<TObserver, ObserverInterfaces...>::type()
        );
    }

    void UnregisterObserver(IObserver* observer) override {
        if (observer == nullptr) return;
        for (const auto& registration : _registrations) {
            if (registration.Handle != nullptr && registration.Observer == observer) {
                RemoveHandle(registration.Handle, true);
                return;
            }
        }
    }

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
