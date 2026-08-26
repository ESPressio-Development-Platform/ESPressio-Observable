#pragma once

namespace ESPressio {

    namespace Observable {

        /// An `IObserver` is an object that can observe an `IObservable`.
        ///
        /// Typed observer relationships are declared explicitly at registration
        /// time. ESPressio Observable does not require RTTI, `typeid`, or
        /// `dynamic_cast` to discover observer interfaces at runtime.
        class IObserver {
            public:
                virtual ~IObserver() = default;
        };

    }

}
