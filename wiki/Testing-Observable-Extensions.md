# Testing Observable Extensions

Observable tests should protect synchronous semantics, typed dispatch, lifetime safety, and mutation behaviour.

## Registration

Test typed registration, multi-interface registration, duplicate observer rejection, untyped registration, explicit unregister, and RAII handle destruction.

## Dispatch

Verify that `WithObservers<T>()` visits only registrations explicitly bound to `T` and that no RTTI dependency is introduced.

## Mutation and reentrancy

Exercise self-unregistration, removing another observer, registration during notification, nested notification, and deferred compaction.

## Lifetime

Verify observer non-ownership and Observable notification lifetime behaviour under shared ownership.

## Thread safety

Stress concurrent register/unregister/notify operations on `ThreadSafeObservable` while keeping derived-state testing separate from registry machinery.

## Memory

Where allocator instrumentation is available, verify registration/binding storage uses the intended ESPressio System provider and does not reintroduce avoidable per-notification allocation.