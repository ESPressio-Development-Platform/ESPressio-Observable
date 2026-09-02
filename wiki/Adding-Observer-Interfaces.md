# Adding Observer Interfaces

A new observer interface should represent one focused synchronous observation responsibility.

Typical pattern:

```cpp
class IConnectionObserver :
    public virtual ESPressio::Observable::IObserver {
public:
    virtual ~IConnectionObserver() = default;
    virtual void OnConnected() = 0;
    virtual void OnDisconnected() = 0;
};
```

## Rules

- derive virtually from `IObserver` when following the library's multi-interface pattern;
- keep callback arguments domain-focused and representation-neutral;
- document whether callbacks occur before/after the producer mutates state;
- avoid embedding transport or scheduling semantics in the observer contract;
- keep callbacks synchronous by design;
- register the interface explicitly with `RegisterObserverAs<T>()`.

## Multiple interfaces

If one observer implements several interfaces, register them together in one registration rather than registering the same object multiple times.

## When not to add an interface

If the consumer should run independently of the producer, the integration likely belongs in Event or Task rather than another Observable callback interface.