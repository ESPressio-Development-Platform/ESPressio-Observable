#include <cassert>
#include <memory>

#include <ESPressio_ThreadSafeObservable.hpp>

using namespace ESPressio::Observable;

namespace {
class TestObserver final : public IObserver {};
}

int main() {
    auto observable = std::make_shared<ThreadSafeObservable>();
    TestObserver observer;

    assert(!observable->HasObservers());
    assert(observable->GetObserverCount() == 0);

    auto handle = observable->RegisterObserver(&observer);
    assert(observable->HasObservers());
    assert(observable->GetObserverCount() == 1);

    handle.reset();
    assert(!observable->HasObservers());
    assert(observable->GetObserverCount() == 0);

    return 0;
}
