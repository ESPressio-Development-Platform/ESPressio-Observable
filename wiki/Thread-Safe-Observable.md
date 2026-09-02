# Thread-Safe Observable

Use `ThreadSafeObservable` when registration, unregistration, or notification can occur concurrently.

```cpp
#include <ESPressio_ThreadSafeObservable.hpp>

class ConnectionMonitor final :
    public ESPressio::Observable::ThreadSafeObservable {
    // ...
};
```

Its typed registration and notification model is the same as `Observable`.

## Scope of thread safety

`ThreadSafeObservable` protects the library's observer registration and notification machinery. It does **not** automatically protect mutable fields added by your derived class.

Protect producer state according to its own access pattern.

## Callback locking

Observer callbacks are synchronous application code. Avoid designs in which callbacks acquire locks in an order that can conflict with producer-side locks.

## Choosing the variant

Use the non-thread-safe `Observable` when all registry/notification operations are constrained to one execution context and the extra synchronization is unnecessary.