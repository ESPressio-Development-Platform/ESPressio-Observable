# Notification Reentrancy and Mutation

Observable supports registration/unregistration changes while a notification is active.

## Immediate dispatch invalidation

When an observer is removed during notification, it must stop being eligible for subsequent dispatch in that notification as soon as the removal is recorded.

## Deferred compaction

Physical cleanup/compaction of registry storage can be deferred until the outer notification completes. This avoids invalidating traversal state while still giving unregistration immediate semantic effect.

## Nested notifications

Extension code must preserve correct outer/inner notification lifetime accounting so compaction does not occur while any notification context still depends on registry iteration state.

## Thread-safe variant

`ThreadSafeObservable` adds synchronization around registry/notification machinery, but callbacks remain arbitrary application code. Do not hold locks in a way that creates unavoidable callback reentrancy or lock-order deadlocks.

## Testing

Exercise self-unregistration, unregistering another observer, registration during notification, nested notification, and teardown during/after notification.