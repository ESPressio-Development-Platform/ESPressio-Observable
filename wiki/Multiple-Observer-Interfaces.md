# Multiple Observer Interfaces

One observer object may implement several focused observer interfaces and register them together.

```cpp
auto registration = observable->RegisterObserverAs<
    ITemperatureObserver,
    IAlarmObserver
>(&display);
```

The registration creates typed bindings for each declared interface.

A later notification can select either interface independently:

```cpp
notification.WithObservers<IAlarmObserver>(...);
```

## Why one registration

Registering the same observer object repeatedly against the same Observable is rejected. Declaring the complete interface set in one registration keeps ownership and unregistration deterministic.

## Interface design

Prefer multiple small interfaces over one large catch-all observer contract when consumers may care about different subsets of notifications.