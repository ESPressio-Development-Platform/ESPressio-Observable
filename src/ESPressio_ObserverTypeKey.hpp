#pragma once

#include <type_traits>

namespace ESPressio {
namespace Observable {

using ObserverTypeKey = const void*;

namespace Detail {

template<typename T>
ObserverTypeKey ObserverTypeKeyStorage() noexcept {
    static const unsigned char token = 0;
    return static_cast<ObserverTypeKey>(&token);
}

} // namespace Detail

template<typename T>
ObserverTypeKey ObserverTypeKeyOf() noexcept {
    using WithoutReference = typename std::remove_reference<T>::type;
    using Normalized = typename std::remove_cv<WithoutReference>::type;
    return Detail::ObserverTypeKeyStorage<Normalized>();
}

} // namespace Observable
} // namespace ESPressio
