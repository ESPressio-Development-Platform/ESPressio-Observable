#pragma once

#include <type_traits>

namespace ESPressio {
namespace Observable {

using ObserverTypeKey = const void*;

namespace Detail {
    template<typename T>
    inline constexpr unsigned char ObserverTypeToken = 0;
}

template<typename T>
constexpr ObserverTypeKey ObserverTypeKeyOf() noexcept {
    using Normalized = std::remove_cv_t<std::remove_reference_t<T>>;
    return static_cast<ObserverTypeKey>(&Detail::ObserverTypeToken<Normalized>);
}

} // namespace Observable
} // namespace ESPressio
