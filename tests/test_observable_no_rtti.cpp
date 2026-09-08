#include <cassert>
#include <memory>

#include "ESPressio_ThreadSafeObservable.hpp"

using namespace ESPressio::Observable;

namespace {

/**
 * ESPressio Memory Audit
 * Members: none; polymorphic interface/object includes vptr storage where not supplied by a base.
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
struct IValueObserver {
    virtual ~IValueObserver() = default;
    virtual void OnValue(int value) = 0;
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 8 bytes [0 bytes dynamic allocation]
 * Members:
 * - Calls (int): 4 bytes [0 bytes dynamic allocation]
 * - Value (int): 4 bytes [0 bytes dynamic allocation]
 * Total Memory: 16 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
struct Observer final : IObserver, IValueObserver {
    int Calls = 0;
    int Value = 0;

    void OnValue(int value) override {
        ++Calls;
        Value = value;
    }
};

/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Observable) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::RecursiveMutex) [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: sizeof(Observable) + sizeof(System::Synchronization::RecursiveMutex) + sizeof(System::Synchronization::RecursiveMutex) [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
class Source final : public ThreadSafeObservable {
public:
    void Notify(int value) {
        ExecuteNotification([&](NotificationContext& notification) {
            notification.WithObservers<IValueObserver>(
                [value](IValueObserver* observer) {
                    observer->OnValue(value);
                }
            );
        });
    }
};

}

int main() {
    auto source = std::make_shared<Source>();
    Observer observer;

    auto handle = source->RegisterObserverAs<IValueObserver>(&observer);
    assert(handle);
    source->Notify(42);
    assert(observer.Calls == 1);
    assert(observer.Value == 42);

    handle.reset();
    source->Notify(84);
    assert(observer.Calls == 1);

    return 0;
}
