# Optimisations

## 2026-08-27

- **#17** Added ESPressio-System as the platform-neutral memory abstraction dependency.
- **#17** Moved flat observer registration and typed-binding tables to `ExternalPreferred` allocator-backed storage.
- **#17** Moved `ObservableLifetimeControl` shared allocation to external-preferred storage.
- **#17** Added class-specific external-preferred allocation for `ObserverHandle` without changing the public `ObserverHandlePtr` API.
- **#17** Replaced temporary record construction followed by `push_back` with direct `emplace_back` where registration ownership is already final.
- **#17** Preserved RTTI-free registration/dispatch and re-entrant notification semantics.
- **#17** Made the root CMake integration portable across ESP-IDF component builds and ordinary host `FetchContent` consumers discovered during coordinated validation.
