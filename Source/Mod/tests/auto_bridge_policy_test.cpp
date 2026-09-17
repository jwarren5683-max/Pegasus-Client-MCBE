#include "../src/modules/AutoBridgePolicy.hpp"

#include <cassert>
#include <iostream>

using utility::modules::auto_bridge::Cadence;
using utility::modules::auto_bridge::Input;

static Input valid() {
    return Input{true, true, true, true, true, true, true, true, true};
}

int main() {
    Cadence cadence(100);
    auto input = valid();

    assert(cadence.update(1000, input).place); // starts immediately
    assert(!cadence.update(1050, input).place); // cadence is enforced
    assert(cadence.update(1100, input).place);
    assert(!cadence.update(1090, input).place); // clock rollback resets safely

    input.modifier = false;
    assert(!cadence.update(1200, input).place);
    assert(!cadence.armed());

    input = valid();
    input.local_world = false;
    assert(!cadence.update(1300, input).place);
    input = valid();
    input.looking_down = false;
    assert(!cadence.update(1400, input).place);
    input = valid();
    input.selected_block = false;
    assert(!cadence.update(1500, input).place);

    input = valid();
    assert(cadence.update(1600, input).place); // re-arms after any failed gate
    input.forward = false;
    assert(!cadence.update(1800, input).place);

    std::cout << "AutoBridge policy tests passed (input-gated, fail-closed cadence).\n";
}

