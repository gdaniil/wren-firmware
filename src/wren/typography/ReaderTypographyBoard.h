#pragma once

namespace wren {
// Call after GPIO initialization. X4-class 4.3-inch panels use 217 PPI.
int activeBoardPpi();
}  // namespace wren
