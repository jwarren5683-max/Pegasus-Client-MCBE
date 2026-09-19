#pragma once

namespace utility::integration {

// Read-only. Records candidate RVAs for the known 26.50 executable so native
// modules can be ported from evidence instead of reusing 26.45 addresses.
void run_compatibility_probe() noexcept;

} // namespace utility::integration
