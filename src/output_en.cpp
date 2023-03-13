#include "output_en.h"

#include "registers/rcc.h"
#include "registers/gpio.h"


namespace output_en {


void init() {
  RCC_IOPENR |= BIT_0;
  GPIO_A_MODER  &= ~(0x00000300);
  GPIO_A_MODER  |=   0x00000100;
  GPIO_A_OTYPER &= ~(BIT_4);
  GPIO_A_ODR    &= ~(BIT_4);
}

void set_state(bool enabled) {
  if(enabled) {
//    GPIO_A_ODR |= BIT_4;
  } else {
//    GPIO_A_ODR &= ~(BIT_4);
  }
}



} // namespace output_en
