# Changelog

All notable changes to this project are documented in this file.

The structure follows the principles of Keep a Changelog and Semantic Versioning.

## [4.0.0] - 2026-08-27

### Changed

- Replaced RTTI-based typed observer discovery with explicit registration-time typed bindings.
- Added `RegisterObserverAs<...>()` for declaring the observer interfaces used by typed notification.
- Typed `WithObservers<T>()` dispatch now visits pre-resolved interface bindings instead of performing `dynamic_cast` in the notification path.
- Moved registration and binding storage onto ESPressio-System allocator-aware containers with `ExternalPreferred` policy.
- Reworked README guidance around the actual RTTI-free 4.x API, ownership model, threading semantics and selection between Observable and Event.

### Compatibility

- **C++ RTTI is no longer required.** Applications may build ESPressio Observable with RTTI disabled.
- Code that uses typed `WithObservers<T>()` must register the corresponding interface with `RegisterObserverAs<T>()`; a legacy untyped `RegisterObserver(IObserver*)` registration only binds `IObserver`.
- The existing RAII `ObserverHandlePtr`, synchronous notification, mutation-during-notification and notification-lifetime guarantees remain in place.

## [3.0.2] - 2026-08-22

### Changed

- Published the post-migration package generation from the dedicated `ESPressio-Development-Platform` GitHub organization.
- Updated package identity and documentation to use `https://espressio.org` and the relocated repository coordinates directly.

### Compatibility

- No public API or runtime behaviour changes.
- Establishes Observable 3.0.2 as the migrated baseline for downstream ESPressio releases.

## [3.0.1] - 2026-08-20

### Changed

- Added an atomic Observer-count fast path to `ThreadSafeObservable`.
- Notifications return immediately when no Observers are registered, avoiding notification synchronization and lifetime acquisition on the zero-Observer path.
- Preserved Observable 3.0 registration, mutation-during-notification, exception and RAII handle semantics.

### Fixed

- Corrected stale `component.mk` compile-time version metadata that still identified the library as 2.0.0.

## [3.0.0] - 2026-08-13

### Changed

- Changed Observer registration ownership from raw owning handles to RAII-managed smart-pointer handles.
- Updated registration APIs to make ownership explicit and memory-safe.
- Made notification mutation/lifetime handling safe when registrations change during notification.

### Fixed

- Removed raw-handle ownership ambiguity and associated leak risks.
- Corrected Observer-registration lifetime hazards during callback/notification mutation.

## [2.0.0]

### Changed

- Established the Observable 2.x interface consumed by ESPressio Event 2.1 and other ESPressio libraries.
- Standardised the common `IObserver`-based synchronous observation contract.

## [1.x]

### Added

- Initial ESPressio Observable synchronous Observer Pattern infrastructure.

> Detailed release notes for the earliest Observable history were not published on the current GitHub Releases pages; these entries are intentionally limited to the public API lineage that can be substantiated.
