# RTTI-Free Dispatch

The 1.0.0 observer registry does not use C++ RTTI or `dynamic_cast` to discover observer interfaces during notification.

## Registration-time work

`RegisterObserverAs<TInterfaces...>()` creates explicit bindings between the observer object and the interfaces declared by the caller.

Because the compiler verifies that the concrete observer implements each requested interface, invalid registration fails at compile time.

## Notification-time work

`WithObservers<TInterface>()` visits only bindings matching the requested interface. No runtime type discovery is required.

## Extension requirements

Do not reintroduce `typeid`, `std::type_index`, `dynamic_cast`, or a build requirement for `-frtti` merely to simplify registry implementation.

If new typed-dispatch capability is needed, extend the binding/key mechanism while preserving deterministic RTTI-free lookup and compile-time registration validation.