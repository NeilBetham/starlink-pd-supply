/**
 * @brief Manages a power high side power switch
 */

#include "digipot.h"

#include <stdint.h>

#pragma once


class PowerSwitch {
public:
  PowerSwitch(Digipot& digipot, uint32_t gpio_bit) : _digipot(digipot), _gpio_bit(gpio_bit) {};

  void init();

  // Set the desired current limit in milliamps
  void set_current(uint32_t current);
  void set_enabled(bool enabled);

private:
  Digipot& _digipot;
  uint32_t _gpio_bit = 0;
  uint32_t _current = 0;
  bool _enabled = false;
};
