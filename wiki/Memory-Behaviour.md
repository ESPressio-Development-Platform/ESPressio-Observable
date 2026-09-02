# Memory Behaviour

Observable uses ESPressio System allocator-aware storage for observer registrations and typed interface bindings.

The 1.0.0 baseline uses `ExternalPreferred` policy for suitable bookkeeping storage. On targets with an installed external-memory provider, this allows registration data to prefer external RAM while retaining the System-defined fallback behaviour.

## Provider capture

Allocator-aware storage captures the active System memory provider when the storage is constructed. Install the target memory provider before constructing long-lived Observable instances whose registration storage should use it.

## Hot-path design

Typed interface resolution is performed from registration-time bindings rather than RTTI/dynamic casts during notification.

## Lifetime discipline

Observers are non-owning, registration handles own only the registration, and removed entries can be invalidated before later compaction during nested/active notification. This keeps ownership explicit while supporting mutation safely.