# Executing Notifications

Notifications are synchronous and execute as part of the producer's current operation.

```cpp
ExecuteNotification([&](NotificationContext& notification) {
    notification.WithObservers<IMyObserver>(
        [&](IMyObserver* observer) {
            observer->OnChanged(value);
        }
    );
});
```

The producer does not continue past `ExecuteNotification()` until the selected observer callbacks have completed.

## Callback design

Keep callbacks short unless the work intentionally belongs inside the producer operation. Long blocking callbacks directly extend producer latency.

Avoid lock-order dependencies between producer code and observer callbacks, especially when using `ThreadSafeObservable`.

## No asynchronous boundary

Observable does not queue callbacks, create workers, or schedule them later. If producer and consumer should execute independently, translate the notification into ESPressio Event or another asynchronous mechanism instead.

## Filtering by interface

`WithObservers<TInterface>()` visits only registrations containing the requested typed binding. Untyped registrations are not dynamically inspected for that interface.