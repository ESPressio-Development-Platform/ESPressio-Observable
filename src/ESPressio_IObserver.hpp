#pragma once

namespace ESPressio {

    namespace Observable {

        /// <summary>Base interface implemented by objects that participate in ESPressio observer relationships.</summary>
        /// <remarks>Typed observer relationships are declared explicitly at registration time; ESPressio Observable does not require RTTI, <c>typeid</c>, or <c>dynamic_cast</c> to discover observer interfaces at runtime.</remarks>
/**
 * ESPressio Memory Audit
 * Members: none; polymorphic interface/object includes vptr storage where not supplied by a base.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
class IObserver {
            public:
                virtual ~IObserver() = default;
        };

    }

}
