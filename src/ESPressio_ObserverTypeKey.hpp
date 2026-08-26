#pragma once

#include <type_traits>

namespace ESPressio {
namespace Observable {

using ObserverTypeKey = const void*;

template<typename T>
ObserverTypeKey ObserverTypeKeyOf() noexcept {
    using WithoutReference = typename std::remove_reference<T>::type;
    using Normalized = typename std::remove_cv<WithoutReference>::type;
    static const unsigned char token = 0;
    return static_cast<ObserverTypeKey>(&token);
}

} // namespace Observable
} // namespace ESPressio
