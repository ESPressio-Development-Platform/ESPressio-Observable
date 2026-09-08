#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Memory.hpp>
#include <ESPressio_PolymorphicMemory.hpp>
#include "ESPressio_Observable.hpp"
#include "ESPressio_ObserverTypeKey.hpp"

namespace ESPressio {
namespace Observable {

/// <summary>Provides RTTI-free typed observer registration and dispatch using interface buckets.</summary>
/// <remarks>Registrations are keyed by the exact interface set supplied at registration time. Conflicting duplicate registrations are rejected.</remarks>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 20 bytes [IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources]
 * Members:
 * - _registrations (RegistrationStorage): 12 bytes [Capacity * (12 bytes) element storage]
 * - _bindings (BindingStorage): 12 bytes [Capacity * (12 bytes) element storage]
 * - _notificationDepth (std::size_t): 4 bytes [0 bytes dynamic allocation]
 * - _needsCompaction (bool): 1 bytes [0 bytes dynamic allocation]
 * Total Memory: 52 bytes [IObservable: enable_shared_from_this: embedded weak_ptr shares a control block when activated; IObservable: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; IObservable: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; IObservable: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources; _registrations: Capacity * (12 bytes) element storage; _bindings: Capacity * (12 bytes) element storage]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class ObservableWithBuckets : public IObservable {
private:
/**
 * ESPressio Memory Audit
 * Members:
 * - Handle (IObserverHandle*): 4 bytes [0 bytes dynamic allocation]
 * - Observer (IObserver*): 4 bytes [0 bytes dynamic allocation]
 * - Identity (void*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
struct Registration {
        IObserverHandle* Handle;
        IObserver* Observer;
        const void* Identity;
        Registration(IObserverHandle* handle = nullptr, IObserver* observer = nullptr, const void* identity = nullptr)
            : Handle(handle), Observer(observer), Identity(identity) {}
    };

/**
 * ESPressio Memory Audit
 * Members:
 * - Handle (IObserverHandle*): 4 bytes [0 bytes dynamic allocation]
 * - Type (ObserverTypeKey): 4 bytes [0 bytes dynamic allocation]
 * - Interface (void*): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 12 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
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
    /// <summary>Provides typed access to observers participating in one bucketed notification.</summary>
/**
 * ESPressio Memory Audit
 * Members:
 * - _observable (ObservableWithBuckets&): 4 bytes [0 bytes dynamic allocation]
 * - _notificationLifetime (std::shared_ptr<IObservable>): 8 bytes [shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: enable_shared_from_this: embedded weak_ptr shares a control block when activated; pointee: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; pointee: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; pointee: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources]
 * Total Memory: 12 bytes [_notificationLifetime: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _notificationLifetime: pointee: enable_shared_from_this: embedded weak_ptr shares a control block when activated; _notificationLifetime: pointee: _lifetimeControl: shared control block (~12+ bytes; allocate_shared may co-locate object) + object 20 bytes; _notificationLifetime: pointee: _lifetimeControl: pointee: _mutex: native synchronization state may allocate platform resources lazily; _notificationLifetime: pointee: _lifetimeControl: pointee: _condition: native condition-variable state may allocate platform synchronization resources]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * Confidence: medium; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class NotificationContext {
        friend class ObservableWithBuckets;
        ObservableWithBuckets& _observable;
        std::shared_ptr<IObservable> _notificationLifetime;
        NotificationContext(ObservableWithBuckets& observable, std::shared_ptr<IObservable> notificationLifetime)
            : _observable(observable), _notificationLifetime(std::move(notificationLifetime)) {}
    public:
        /// <summary>Invokes a callback for observers registered for the requested interface.</summary>
        template<typename ObserverType, typename Callback>
        void WithObservers(Callback&& callback) {
            _observable.template WithObserversTyped<ObserverType>(std::forward<Callback>(callback));
        }
    };

    /// <summary>Executes a typed notification while retaining the Observable's shared lifetime.</summary>
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

    /// <summary>Rejects null registration for an explicit observer interface set.</summary>
    template<typename... ObserverInterfaces>
    ObserverHandlePtr RegisterObserverAs(std::nullptr_t) {
        static_assert(sizeof...(ObserverInterfaces) > 0, "At least one Observer interface must be specified");
        throw InvalidObserverRegistrationException();
    }

    /// <summary>Registers an observer for an exact set of typed observer interfaces.</summary>
    /// <returns>An RAII handle whose concrete storage prefers external memory and whose destruction unregisters the observer.</returns>
    template<typename... ObserverInterfaces, typename TObserver>
    ObserverHandlePtr RegisterObserverAs(TObserver* observer) {
        static_assert(sizeof...(ObserverInterfaces) > 0, "At least one Observer interface must be specified");
        return RegisterObserverAsImpl<ObserverInterfaces...>(
            observer,
            typename Detail::AllObserverInterfacesConvertible<TObserver, ObserverInterfaces...>::type()
        );
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
