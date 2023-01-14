#include "status_light.h"

#include "registers/rcc.h"
#include "registers/gpio.h"

void status_light::init() {
  RCC_IOPENR |= BIT_0;
  GPIO_A_MODER  &= ~(0x0030000F);
  GPIO_A_MODER  |=   0x00100005;
  GPIO_A_OTYPER |=   0x00000403;
  GPIO_A_ODR    |=   0x00000403;
}

void status_light::set_color(uint32_t red, uint32_t green, uint32_t blue) {
  if(red > 0) {
    GPIO_A_ODR &= ~BIT_0;
  } else {
    GPIO_A_ODR |= BIT_0;
  }

  if(green > 0) {
    GPIO_A_ODR &= ~BIT_1;
  } else {
    GPIO_A_ODR |= BIT_1;
  }

  if(blue > 0) {
    GPIO_A_ODR &= ~BIT_10;
  } else {
    GPIO_A_ODR |= BIT_10;
  }
}
