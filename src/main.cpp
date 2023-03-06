#include "utils.h"

#include "registers/rcc.h"
#include "registers/gpio.h"
#include "registers/flash.h"
#include "registers/exti.h"
#include "registers/syscfg.h"
#include "registers/core.h"

#include "i2c.h"
#include "usb_pd_controller.h"
#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"
#include "power_mux.h"


int main() {
  // Init status light
  // LED Hello World
  status_light::init();
  status_light::set_color(1, 0, 0);
  for(uint32_t index = 0; index < 200000; index++);
  status_light::set_color(0, 1, 0);
  for(uint32_t index = 0; index < 200000; index++);
  status_light::set_color(0, 0, 1);
  for(uint32_t index = 0; index < 200000; index++);
  status_light::set_color(1, 1, 1);


  // Up main clock frequency
  // Turn off PLL and wait for it be ready
  RCC_CR &= ~(BIT_24);
  while(RCC_CR & BIT_25);

  // Enable the high speed interal oscillator
  RCC_CR |= BIT_8;
  while(RCC_CR & BIT_2);

  // Set pll source, multiplier and divisior for 64 MHz
  RCC_PLL_CFGR |=   0x00000002;  // Set PLL source to HSI16
  RCC_PLL_CFGR &= ~(0x00000007 << 4);  // Set PLL input scaler M to 1
  RCC_PLL_CFGR &= ~(0x0000007F << 8);  // Set PLL mult factor to 8
  RCC_PLL_CFGR |=   0x00001000 << 8;  // Set PLL mult factor to 8
  RCC_PLL_CFGR |=   BIT_28;  // Enable PLL R output
  RCC_PLL_CFGR &= ~(0x00000007 << 29);  // Set R output scalar to 2
  RCC_PLL_CFGR |=   0x00000001 << 29;  // Set R output scalar to 2

  // Set flash wait states
  FLASH_ACR &= ~(0x7);
  FLASH_ACR |= 0x2;  // 2 wait states for 64 MHz

  // Enable PLL
  RCC_CR |= BIT_24;
  while(RCC_CR & BIT_25);

  // Switch system clock to PLL
  RCC_CFGR &= ~(0x7);
  RCC_CFGR |= 0x2;

  // Init systick
  systick_init();

  // Add a small delay for things to stabilize
  msleep(10);

  // Make sure RTT gets inited
  rtt_print("----\r\n");
  rtt_print("Booting...\r\n");

  status_light::set_color(0, 0, 1);

  // Enable interruptS
  asm("CPSIE i");

  while(true) {}

  return 0;
}
