# Registration and Lifetime

Observer registration lifetime is explicit and RAII-managed.

`ObserverHandlePtr` owns the registration handle. Destroying the handle unregisters the observer. Registration can also be ended explicitly through the handle or Observable API.

## Observer ownership

Observable does **not** own the observer object. The observer must remain alive for the complete registration lifetime.

```cpp
auto handle = observable->RegisterObserverAs<IMyObserver>(&observer);
// observer must outlive handle
```

## Observable lifetime during notification

`ExecuteNotification()` acquires the Observable's notification lifetime. Notification-capable Observable objects therefore participate in the library's shared-ownership model, normally through `std::shared_ptr`, so the producer cannot disappear while callbacks are executing.

## Duplicate registration

Registering the same observer object twice against the same Observable is rejected. If the object implements several observer interfaces, register its complete interface set in one registration.

## Mutation during notification

Unregistration during an active notification is supported. Removed entries become invalid for dispatch immediately and are compacted after the outer notification completes. See [Notification Reentrancy and Mutation](Notification-Reentrancy-and-Mutation).