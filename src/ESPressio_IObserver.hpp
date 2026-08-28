#pragma once

namespace ESPressio {

    namespace Observable {

        /// <summary>Base interface implemented by objects that participate in ESPressio observer relationships.</summary>
        /// <remarks>Typed observer relationships are declared explicitly at registration time; ESPressio Observable does not require RTTI, <c>typeid</c>, or <c>dynamic_cast</c> to discover observer interfaces at runtime.</remarks>
        class IObserver {
            public:
                virtual ~IObserver() = default;
        };

    }

}
