#include "digipot.h"

#include "rtt.h"

#define MAX_RESISTANCE 100000
#define MIN_RESISTANCE 325
#define WIPER_RES 256

#define VOLATILE_REG_CMD 0x11
#define NON_VOLATILE_REG_CMD 0x21
#define SWAP_NV_TO_V_REG_CMD 0x61
#define SWAP_V_TO_NV_REG_CMD 0x51


void Digipot::init() {
  uint8_t full_count = 0xFF;
  _i2c_port.write_to(_addr, VOLATILE_REG_CMD, &full_count, 1);
}

bool Digipot::set_resistance(uint32_t resistance) {
  if(resistance > MAX_RESISTANCE) {
    resistance = MAX_RESISTANCE;
  }

  // Figure out which tap we need to select
  uint8_t tap = 0;
  if(resistance > MIN_RESISTANCE) {
    tap = ((resistance - MIN_RESISTANCE) * WIPER_RES) / MAX_RESISTANCE;
  }

  // Select the wiper tap
  return set_tap(tap);
}

bool Digipot::set_tap(uint8_t tap) {
  return _i2c_port.write_to(_addr, VOLATILE_REG_CMD, &tap, 1) == 1;
}
