/**
 * #brief Interface class for i2c digipots
 */

#include <stdint.h>

#include "i2c.h"

#pragma once


class Digipot {
public:
  Digipot(I2C& i2c_port, uint8_t addr);

  bool set_tap(uint32_t tap);

private:
  I2C& _i2c_port;
  uint8_t _addr;
}

