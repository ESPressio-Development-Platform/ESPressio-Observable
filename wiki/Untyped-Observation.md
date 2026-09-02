# Untyped Observation

`RegisterObserver(IObserver*)` remains available for genuinely untyped observation. It registers the common `IObserver` interface itself.

Use it only when notification code uses the untyped observer overload:

```cpp
ExecuteNotification([&](NotificationContext& notification) {
    notification.WithObservers(
        [&](ESPressio::Observable::IObserver* observer) {
            // operation valid on IObserver
        }
    );
});
```

If notification requests `WithObservers<IMyObserver>()`, the observer must instead be registered with `RegisterObserverAs<IMyObserver>()`.

## Why this distinction matters

The 1.0.0 registry is RTTI-free. An untyped registration is not later inspected dynamically to discover additional interfaces implemented by the concrete object.

Use typed registration whenever the producer has a meaningful focused observer interface.