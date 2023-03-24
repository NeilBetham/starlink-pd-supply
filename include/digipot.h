/**
 * #brief Interface class for i2c digipots
 */

#include <stdint.h>

#include "i2c.h"

#pragma once


class Digipot {
public:
  Digipot(I2C& i2c_port, uint8_t addr) : _i2c_port(i2c_port), _addr(addr) {};

  void init();

  bool set_resistance(uint32_t resistance);

private:
  I2C& _i2c_port;
  uint8_t _addr;

  bool set_tap(uint8_t tap);
};

