#pragma once

#include "ESPressio_Observable.hpp"

namespace ESPressio {
namespace Observable {

/// Compatibility name for the typed-registration Observable implementation.
///
/// The former implementation maintained an unordered_map of RTTI type_index
/// keys and per-type vectors. Observable now stores a compact flat binding table
/// keyed by ObserverTypeKey, so a separate bucket container would only duplicate
/// storage and allocation overhead.
class ObservableWithBuckets : public Observable {
public:
    using Observable::RegisterObserver;
    using Observable::RegisterObserverAs;
};

} // namespace Observable
} // namespace ESPressio
