#include "status_light.h"

#include "registers/rcc.h"
#include "registers/gpio.h"

void status_light::init() {
  RCC_IOPENR |= BIT_1;
  GPIO_B_MODER  &= ~(0x00003F00);
  GPIO_B_MODER  |=   0x00001500;
  GPIO_B_OTYPER |=   0x00000070;
  GPIO_B_ODR    |=   0x00000070;
}

void status_light::set_color(uint32_t red, uint32_t green, uint32_t blue) {
  if(red > 0) {
    GPIO_B_ODR &= ~BIT_4;
  } else {
    GPIO_B_ODR |= BIT_4;
  }

  if(green > 0) {
    GPIO_B_ODR &= ~BIT_5;
  } else {
    GPIO_B_ODR |= BIT_5;
  }

  if(blue > 0) {
    GPIO_B_ODR &= ~BIT_6;
  } else {
    GPIO_B_ODR |= BIT_6;
  }
}
