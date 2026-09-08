#include <cassert>
#include <memory>

#include <ESPressio_ThreadSafeObservable.hpp>

using namespace ESPressio::Observable;

namespace {
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: 4 bytes [0 bytes dynamic allocation]
 * Members: none (empty object still occupies at least 1 byte unless empty-base optimisation applies).
 * Total Memory: 4 bytes [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * End ESPressio Memory Audit
 */
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
