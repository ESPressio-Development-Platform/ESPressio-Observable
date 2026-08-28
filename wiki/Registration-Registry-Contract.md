# Registration Registry Contract

Each registration represents one observer object plus the complete set of observer interfaces declared for that Observable.

## Required behaviour

The registry must:

- reject duplicate registration of the same observer object against one Observable;
- retain non-owning observer pointers only for the registration lifetime;
- retain typed interface bindings established during registration;
- invalidate removed registrations immediately for dispatch purposes;
- support deferred compaction when notification is active;
- preserve registration semantics in both normal and thread-safe variants.

## Handle ownership

The registration handle owns registry membership, not the observer object. Destroying/unregistering the handle removes the entry.

## Memory policy

Dynamic registration/binding storage should remain allocator-aware through ESPressio System, preserving configured platform memory policy.

## Dispatch integrity

A typed notification must never call an observer that was not explicitly registered for that interface, even when the underlying concrete class happens to implement it.