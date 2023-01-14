/**
 * @brief Basic I2C driver
 * @note This is a blocking driver
 * @note Defaults to Fast Mode+
 * @note Defaults to master mode
 * @note Defaults to 7 bit addresses
 */

#include <stdint.h>

#pragma once

class I2C {
public:
  I2C(uint8_t i2c_number) : _i2c_number(i2c_number) {}

  void init();

  int write_to(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len);
  int read_from(uint8_t addr, uint8_t reg, uint8_t* data, uint32_t len);

private:
  uint8_t _i2c_number;
  uint32_t _base_addr = 0;
};
