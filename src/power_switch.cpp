#include "power_switch.h"

#include "registers/rcc.h"
#include "registers/gpio.h"


#define CURRENT_LIMIT_RATIO 90000000 // mA * Ohm

void PowerSwitch::init() {
  // Power switches are all on port B so all init will assume port B
  RCC_IOPENR |= BIT_1;
  GPIO_B_ODR    &= ~(1 << _gpio_bit);
  GPIO_B_MODER  &= ~(0x3 << (_gpio_bit * 2));
  GPIO_B_MODER  |=  (0x1 << (_gpio_bit * 2));
  GPIO_B_OTYPER &= ~(1 << _gpio_bit);

  set_current(_current);
}

void PowerSwitch::set_current(uint32_t current) {
  // Add a little buffer to the input current limits
  _current = current + 1000;
  // Power switch expects a value between 7k and 70k ohms
  // First calculate the desired resistance based on the input current in milliamps
  uint32_t resistance = CURRENT_LIMIT_RATIO / current;

  // Clamp the resistance
  if(resistance < 7000) {
    resistance = 7000;
  }

  if(resistance > 70000) {
    resistance = 70000;
  }

  _digipot.set_resistance(resistance);
}

void PowerSwitch::set_enabled(bool enabled) {
  _enabled = enabled;
  if(enabled) {
    GPIO_B_ODR |= 1 << _gpio_bit;
  } else {
    GPIO_B_ODR &= ~(1 << _gpio_bit);
  }
}

