#include "../src/modules/AutoLeavePolicy.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
}

int main() {
    using namespace utility::modules::auto_leave;
    check(clamp_hearts(-5.0F) == minimum_hearts, "threshold clamps at half a heart");
    check(clamp_hearts(50.0F) == maximum_hearts, "threshold clamps at ten hearts");
    check(clamp_hearts(3.24F) == 3.0F && clamp_hearts(3.26F) == 3.5F,
        "threshold snaps to half-heart steps");
    check(clamp_hearts(NAN) == default_hearts, "invalid threshold uses safe default");

    Gate gate;
    check(!gate.update(7.0F, 3.0F, true), "health above threshold stays connected");
    check(!gate.update(6.0F, 3.0F, true), "first low sample is confirmed before leaving");
    check(gate.update(6.0F, 3.0F, true), "second low sample triggers at exact threshold");
    check(gate.fired() && !gate.update(2.0F, 3.0F, true), "leave request is one shot");
    gate.reset();
    check(!gate.fired(), "new world or re-enable rearms the gate");
    check(!gate.update(4.0F, 3.0F, false), "disabled module never triggers");
    check(!gate.update(NAN, 3.0F, true) && !gate.update(30.0F, 3.0F, true),
        "invalid native health is rejected");
    std::puts("Auto Leave half-heart threshold, confirmation and one-shot policy passed.");
}
