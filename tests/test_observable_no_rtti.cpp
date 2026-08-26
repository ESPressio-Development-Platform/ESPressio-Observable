#include <cassert>
#include <memory>

#include "ESPressio_ThreadSafeObservable.hpp"

using namespace ESPressio::Observable;

namespace {

struct IValueObserver {
    virtual ~IValueObserver() = default;
    virtual void OnValue(int value) = 0;
};

struct Observer final : IObserver, IValueObserver {
    int Calls = 0;
    int Value = 0;

    void OnValue(int value) override {
        ++Calls;
        Value = value;
    }
};

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
