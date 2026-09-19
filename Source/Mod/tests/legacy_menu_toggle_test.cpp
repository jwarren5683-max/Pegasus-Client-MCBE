#include "../src/integration/LegacyMenuToggle.hpp"
#include <cassert>
#include <iostream>

int main() {
    utility::integration::LegacyMenuToggle toggle;
    assert(toggle.visible());

    assert(!toggle.update('X', true, true));
    assert(toggle.visible());

    assert(toggle.update('C', true, true));
    assert(!toggle.visible());

    assert(!toggle.update('C', true, true));
    assert(!toggle.visible());

    assert(!toggle.update('C', false, true));
    assert(toggle.update('C', true, true));
    assert(toggle.visible());
    assert(!toggle.update('C', false, true));

    assert(!toggle.update('C', true, false));
    assert(toggle.visible());
    assert(!toggle.update('C', true, true));
    assert(toggle.visible());
    assert(!toggle.update('C', false, true));
    assert(toggle.update('C', true, true));
    assert(!toggle.visible());

    std::cout << "Legacy menu C-key toggle policy passed.\n";
}
