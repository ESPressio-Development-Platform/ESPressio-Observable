# Getting Started

Define a focused observer interface:

```cpp
#include <ESPressio_IObserver.hpp>

class ITemperatureObserver :
    public virtual ESPressio::Observable::IObserver {
public:
    virtual ~ITemperatureObserver() = default;
    virtual void OnTemperatureChanged(float previous, float current) = 0;
};
```

Implement an Observable producer:

```cpp
#include <ESPressio_Observable.hpp>

class Thermometer final : public ESPressio::Observable::Observable {
public:
    void SetTemperature(float value) {
        if (value == _temperature) return;
        const float previous = _temperature;
        _temperature = value;

        ExecuteNotification([&](NotificationContext& notification) {
            notification.WithObservers<ITemperatureObserver>(
                [&](ITemperatureObserver* observer) {
                    observer->OnTemperatureChanged(previous, value);
                }
            );
        });
    }
private:
    float _temperature = 0.0f;
};
```

Register the exact interface that will be notified:

```cpp
auto thermometer = std::make_shared<Thermometer>();
TemperatureLogger logger;

auto registration =
    thermometer->RegisterObserverAs<ITemperatureObserver>(&logger);
```

Keep the returned registration handle alive for as long as observation should remain active.

## Important

Typed registration is explicit in the 1.0.0 baseline because dispatch is RTTI-free. See [Typed Observer Interfaces](Typed-Observer-Interfaces).