# Typed Observer Interfaces

Typed observer interfaces make the producer depend on an abstraction rather than a concrete consumer.

```cpp
class IAlarmObserver :
    public virtual ESPressio::Observable::IObserver {
public:
    virtual ~IAlarmObserver() = default;
    virtual void OnAlarmChanged(bool active) = 0;
};
```

## Registration-time binding

Register with the interface that notification code will request:

```cpp
auto handle = observable->RegisterObserverAs<IAlarmObserver>(&display);
```

Later:

```cpp
notification.WithObservers<IAlarmObserver>(
    [](IAlarmObserver* observer) {
        observer->OnAlarmChanged(true);
    }
);
```

The binding is established when the observer is registered. Dispatch does not discover interfaces later with `dynamic_cast`.

## Compile-time checking

`RegisterObserverAs<TInterface>()` requires the concrete observer to implement the declared interface. Invalid bindings fail at compile time.

## Design guidance

Keep interfaces focused around one observation responsibility. A producer should know the observer interface it needs, not the concrete class consuming the notification.