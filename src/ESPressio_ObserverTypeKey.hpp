#pragma once

#include <type_traits>

namespace ESPressio {
namespace Observable {

/// <summary>Opaque process-local token uniquely identifying a normalized observer interface type without RTTI.</summary>
using ObserverTypeKey = const void*;

namespace Detail {

template<typename T>
ObserverTypeKey ObserverTypeKeyStorage() noexcept {
    static const unsigned char token = 0;
    return static_cast<ObserverTypeKey>(&token);
}

} // namespace Detail

/// <summary>Gets the stable process-local type key for an observer interface after removing reference and cv qualifiers.</summary>
/// <typeparam name="T">Observer interface type to identify.</typeparam>
template<typename T>
ObserverTypeKey ObserverTypeKeyOf() noexcept {
    using WithoutReference = typename std::remove_reference<T>::type;
    using Normalized = typename std::remove_cv<WithoutReference>::type;
    return Detail::ObserverTypeKeyStorage<Normalized>();
}

} // namespace Observable
} // namespace ESPressio
