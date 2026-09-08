#pragma once

namespace ESPressio {

    namespace Observable {

        /// <summary>Base interface implemented by objects that participate in ESPressio observer relationships.</summary>
        /// <remarks>Typed observer relationships are declared explicitly at registration time; ESPressio Observable does not require RTTI, <c>typeid</c>, or <c>dynamic_cast</c> to discover observer interfaces at runtime.</remarks>
/**
 * ESPressio Memory Audit
 * Members: none; polymorphic/virtual-base object metadata is included in the total.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI (4-byte pointers/size_t); ESPressio stateful allocators/deleters included; ABI-sensitive STL/platform internals are identified explicitly.
 * End ESPressio Memory Audit
 */
class IObserver {
            public:
                virtual ~IObserver() = default;
        };

    }

}
