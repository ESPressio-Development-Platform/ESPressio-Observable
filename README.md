# ESPressio Observable

Synchronous, typed Observer Pattern infrastructure for the ESPressio Development Platform.

ESPressio Observable is appropriate when a producer must notify one or more independent consumers **during the same operation**. It deliberately does not introduce a queue, worker, scheduler, or asynchronous boundary. For asynchronously scheduled work, use ESPressio Event instead.

## Release candidate

This working branch is being prepared for **4.0.0**. The major-version change reflects the new RTTI-free typed observer registry and the wider platform consolidation. Do not use the old 3.x documentation as an API guide for this branch.

## Key properties

- Synchronous and deterministic notification.
- Multiple observers and multiple observer interfaces per observable.
- RAII `ObserverHandlePtr` registration lifetime.
- Safe unregistration during notification.
- `Observable` and `ThreadSafeObservable` variants.
- Typed dispatch without `dynamic_cast` or C++ RTTI.
- ESPressio-System memory policies for registration storage.

**RTTI is not required.** Do not add `-frtti` or remove `-fno-rtti` for ESPressio Observable 4.x.

## Dependency

Observable depends on ESPressio-System for allocator-aware storage. During the coordinated release-candidate phase the working branches are used together; published 4.0.0 metadata will target the corresponding released System generation.

PlatformIO working-branch example:

```ini
lib_deps =
    https://github.com/ESPressio-Development-Platform/ESPressio-System.git#main
    https://github.com/ESPressio-Development-Platform/ESPressio-Observable.git#main
```

## Basic example

Suppose a thermometer synchronously reports changes to interested application components.

### 1. Define a focused observer interface

```cpp
#include <ESPressio_IObserver.hpp>

class ITemperatureObserver :
    public virtual ESPressio::Observable::IObserver {
public:
    virtual ~ITemperatureObserver() = default;
    virtual void OnTemperatureChanged(float previous, float current) = 0;
};
```

### 2. Implement the observable

```cpp
#include <ESPressio_Observable.hpp>

class Thermometer final : public ESPressio::Observable::Observable {
public:
    void SetTemperature(float value) {
        if (value == _temperature) return;

        const float previous = _temperature;
        _temperature = value;

        ExecuteNotification([&](NotificationContext& notification) {
            notification.WithObservers<ITemperatureObserver>(
                [&](ITemperatureObserver* observer) {
                    observer->OnTemperatureChanged(previous, value);
                }
            );
        });
    }

private:
    float _temperature = 0.0f;
};
```

### 3. Register the interface you intend to notify

```cpp
class TemperatureLogger final : public ITemperatureObserver {
public:
    void OnTemperatureChanged(float previous, float current) override {
        // Log, update a display, collect diagnostics, etc.
    }
};

auto thermometer = std::make_shared<Thermometer>();
TemperatureLogger logger;

auto registration =
    thermometer->RegisterObserverAs<ITemperatureObserver>(&logger);

thermometer->SetTemperature(22.5f);
```

The explicit `RegisterObserverAs<ITemperatureObserver>()` is important in 4.x. Typed notification is resolved from bindings created at registration time; it does not discover interfaces later with `dynamic_cast`.

## Why typed registration exists

Earlier Observable generations could register only the common `IObserver` base and discover a requested interface during notification using RTTI. The current design moves that work to compile time/registration time:

```text
registration
    observer pointer
       |
       +--> ITemperatureObserver binding
       +--> IAlarmObserver binding

notification WithObservers<ITemperatureObserver>()
       |
       +--> visits only matching typed bindings
```

This has three useful consequences:

1. no RTTI requirement;
2. no `dynamic_cast` in the notification hot path; and
3. a requested registration fails at compile time if the concrete observer does not implement the declared interface.

An observer implementing several interfaces can register them together:

```cpp
auto registration = thermometer->RegisterObserverAs<
    ITemperatureObserver,
    IAlarmObserver
>(&display);
```

Registering the same observer object twice against the same Observable is rejected; declare its complete interface set in one registration.

## Untyped observers

`RegisterObserver(IObserver*)` remains available for genuinely untyped observation. It is equivalent to registering the `IObserver` interface itself. Use it only when notification code calls the untyped overload:

```cpp
ExecuteNotification([&](NotificationContext& notification) {
    notification.WithObservers([&](ESPressio::Observable::IObserver* observer) {
        // Common-IObserver operation only.
    });
});
```

If your notification calls `WithObservers<IMyObserver>()`, register with `RegisterObserverAs<IMyObserver>()`.

## Registration and lifetime

`ObserverHandlePtr` is an owning smart pointer to the registration handle. Destroying it unregisters the observer. You can also call `Unregister()` explicitly or call `UnregisterObserver()` on the Observable.

The observer itself is **not owned** by Observable. It must remain alive for the complete registration lifetime.

`ExecuteNotification()` acquires the Observable's notification lifetime, so notification-capable Observable instances must participate in the library's shared ownership model (normally `std::shared_ptr`). This prevents the Observable from disappearing while callbacks are executing.

Registration/unregistration during notification is supported. Removed entries are invalidated immediately for dispatch purposes and compacted when the outer notification completes.

## Thread-safe use

Use `ThreadSafeObservable` when registration, unregistration, or notification can occur concurrently:

```cpp
#include <ESPressio_ThreadSafeObservable.hpp>

class ConnectionMonitor final :
    public ESPressio::Observable::ThreadSafeObservable {
    // ...
};
```

Its typed registration and notification model is the same as `Observable`.

`ThreadSafeObservable` protects **Observable's registration/notification machinery**. It does not automatically make fields added by your derived class thread-safe. Protect your own mutable state according to its access pattern.

Observer callbacks are synchronous application code. Keep them short unless the work intentionally belongs inside the notifying operation, and avoid lock-order dependencies between callbacks and the producer.

## Memory behaviour

Observable uses ESPressio-System allocator-aware containers with `ExternalPreferred` policy for registration/binding storage. On platforms with an installed external-memory provider this allows suitable bookkeeping storage to prefer external RAM while retaining the System-defined fallback behaviour.

The provider used by allocator-aware storage is captured when that storage is constructed. On ESP32 applications that install the ESPressio ESP32 memory provider, install it before constructing long-lived Observable objects whose storage should use that provider.

## Observable versus Event

Choose Observable when this is the desired semantic:

```text
producer operation
  -> change state
  -> notify observers synchronously
  -> continue/return
```

Choose ESPressio Event when producer and consumer should be independently scheduled:

```text
producer -> dispatch event -> producer continues
                         |
                         +--> asynchronous consumer
```

It is common for a higher-level integration to observe a synchronous subsystem and translate selected notifications into Events. Observable itself does not depend on Event.

## When Observable is a good fit

Use it for lifecycle notifications, state-change callbacks, diagnostics hooks, low-overhead subsystem observation, or any one-to-many relationship where callback completion is part of the producer's operation.

Do not use it as a substitute for a work queue, as an ISR-to-task transport, or when observers may perform unbounded/blocking work that should be scheduled independently.

## Public namespace and headers

Namespace:

```cpp
ESPressio::Observable
```

Common public types include `IObserver`, `IObserverHandle`, `ObserverHandlePtr`, `IObservable`, `IUntypedObservable`, `Observable`, and `ThreadSafeObservable`.

Use the focused header for the abstraction you need; `ESPressio_Observable.hpp` provides the core non-thread-safe Observable implementation.

## Design contract

- The producer knows observer **interfaces**, never concrete consumers.
- Observers remain non-owning registrations.
- Registration lifetime is explicit and RAII-managed.
- Typed interfaces are declared at registration time.
- Notifications are synchronous.
- RTTI is not part of the dispatch mechanism.
- Thread safety of derived application state remains the derived type's responsibility.

## License

Apache License 2.0. See `LICENSE`.
