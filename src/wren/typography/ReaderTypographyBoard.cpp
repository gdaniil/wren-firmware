#include "ReaderTypographyBoard.h"

#include <HalGPIO.h>

#include "ReaderTypography.h"

namespace wren {
int activeBoardPpi() { return gpio.deviceIsX3() ? X3_PPI : X4_PPI; }
}  // namespace wren
