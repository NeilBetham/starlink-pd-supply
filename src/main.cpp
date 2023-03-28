#include "utils.h"

#include "registers/rcc.h"
#include "registers/gpio.h"
#include "registers/flash.h"
#include "registers/exti.h"
#include "registers/syscfg.h"
#include "registers/core.h"

#include "i2c.h"
#include "stm_pd.h"
#include "usb_pd_controller.h"
#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"
#include "power_mux.h"
#include "digipot.h"
#include "power_switch.h"
#include "dishy_power.h"



/**
 * Pin Mappings
 * UCPD1 -> Port A
 *   - VBUS_EN - PB11
 *   - Digipot - A0 -> L
 * UCPD2 -> Port B
 *   - VBUS_EN - PB12
 *   - Digipot - A0 -> H
 *
 * Digipot Base Addr -> 0b010111A0
 */

I2C digipot_i2c(1);
Digipot digipot_a(digipot_i2c, 0x2E);
Digipot digipot_b(digipot_i2c, 0x2F);
PowerSwitch power_switch_a(digipot_a, BIT11_POS);
PowerSwitch power_switch_b(digipot_b, BIT12_POS);
DishyPower dishy_power;
STMPD pd_one(PDPort::one);
STMPD pd_two(PDPort::two);
PowerMux power_mux(pd_one, pd_two, power_switch_a, power_switch_b, dishy_power);


void PD1_PD2_USB_ISR(void) {
  pd_one.handle_interrupt();
  pd_two.handle_interrupt();
  NVIC_ICPR |= BIT_8;
}

void HardFault_Handler(void) {
  status_light::set_color(1, 1, 1);
  asm("bkpt 1");
  while(1);
}

int main() {
  // Init status light
  // LED Hello World
  status_light::init();
  status_light::set_color(1, 0, 0);

  // Setup core clock to be 64MHz
  // Turn off PLL and wait for it to stop
  RCC_CR &= ~(BIT_24);
  while(RCC_CR & BIT_25);

  // Enable the high speed interal oscillator
  RCC_CR |= BIT_8 | BIT_9;
  while(!(RCC_CR & BIT_10));

  // Set pll source, multiplier and divisior for 64 MHz
  RCC_PLL_CFGR |=   0x00000002;         // Set PLL source to HSI16
  RCC_PLL_CFGR &= ~(0x00000007 << 4);   // Set PLL input scaler M to 1
  RCC_PLL_CFGR &= ~(0x0000007F << 8);   // Set PLL mult factor to 8
  RCC_PLL_CFGR |=   0x00001000 << 8;    // Set PLL mult factor to 8
  RCC_PLL_CFGR |=   BIT_28;             // Enable PLL R output
  RCC_PLL_CFGR &= ~(0x00000007 << 29);  // Set R output scalar to 2
  RCC_PLL_CFGR |=   0x00000001 << 29;   // Set R output scalar to 2

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

  // Enable SYSCFG clocks
  RCC_APBENR2 |= BIT_0;

  // Enable CRC clocks
  RCC_AHBENR |= BIT_12;

  // Enable A B C D GPIOs
  RCC_IOPENR |= 0xF;

  // Setup PB8/9 for I2C 1
  GPIO_B_MODER  &= ~((0x3 << BIT16_POS) | (0x3 << BIT18_POS));
  GPIO_B_MODER  |=  ((0x2 << BIT16_POS) | (0x2 << BIT18_POS));
  GPIO_B_AFRH   &= ~((0xF << BIT4_POS) | (0xF << BIT0_POS));
  GPIO_B_AFRH   |=  ((0x6 << BIT4_POS) | (0x6 << BIT0_POS));
  GPIO_B_OTYPER |= BIT_8 | BIT_9;

  // Add a small delay for things to stabilize
  msleep(10);

  // Make sure RTT gets inited
  rtt_printf("----------");
  rtt_printf("Booting...");

  status_light::set_color(0, 0, 1);

  // Init all the things
  digipot_i2c.init();
  digipot_a.init();
  digipot_b.init();
  power_switch_a.init();
  power_switch_b.init();
  dishy_power.init();
  pd_one.init();
  pd_two.init();

  // Enable UCPD Interrupt
  NVIC_ISER |= BIT_8;

  // Enable interruptS
  asm("CPSIE i");

  while(true) {
    dishy_power.tick();
    pd_one.tick();
    pd_two.tick();
  }

  return 0;
}
