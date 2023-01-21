#include "registers/rcc.h"
#include "registers/gpio.h"
#include "registers/flash.h"
#include "registers/exti.h"
#include "registers/syscfg.h"
#include "registers/core.h"

#include "i2c.h"
#include "ptn5110.h"
#include "usb_pd_controller.h"
#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"

I2C ptn5110_i2c(1);
PTN5110 ptn5110_a(ptn5110_i2c, 0x52);
PTN5110 ptn5110_b(ptn5110_i2c, 0x50);
USBPDController controller_a(ptn5110_a);
USBPDController controller_b(ptn5110_b);
bool read_pending = false;

void ExternInterrupt_15_4_ISR(void) {
  read_pending = true;
  NVIC_ICPR |= BIT_7;
  EXTI_PR |= BIT_7;
}


int main() {
  // Up main clock frequency
  // Turn off PLL and wait for it be ready
  RCC_CR &= ~(BIT_24);
  while(RCC_CR & BIT_25);

  // Enable the high speed interal oscillator
  RCC_CR |= BIT_0;
  while(RCC_CR & BIT_2);

  // Set pll source, multiplier and divisior for 32 MHz
  RCC_CFGR &= ~BIT_16;
  RCC_CFGR &= ~(0xF << 18);
  RCC_CFGR |= (0x1 << 18);
  RCC_CFGR &= ~(0x3 << 22);
  RCC_CFGR |= (0x1 << 22);

  // Set flash wait states
  FLASH_ACR |= 0x1;

  // Enable PLL
  RCC_CR |= BIT_24;
  while(RCC_CR & BIT_25);

  // Switch system clock to PLL
  RCC_CFGR |= (0x3 << 0);

  // Init systick
  systick_init();

  rtt_printf("----");
  rtt_printf("Booting...");

  // Setup the I2C peripheral
  ptn5110_i2c.init();

  // Enable IO Ports
  RCC_IOPENR |= 0x0000009F;

  // Setup GPIO B 6/7 for I2C 1
  GPIO_B_AFRL    &= ~(0x000000FF << 24);
  GPIO_B_AFRL     |= (0x00000011 << 24);

  GPIO_B_MODER   &= ~(0x0000000F << 12);
  GPIO_B_MODER    |= (0x0000000A << 12);

  GPIO_B_OSPEEDR &= ~(0x0000000F << 12);
  GPIO_B_OSPEEDR  |= (0x0000000F << 12);

  GPIO_B_PUPDR   &= ~(0x0000000F << 12);
  GPIO_B_PUPDR    |= (0x00000005 << 12);

  GPIO_B_OTYPER   |= (0x00000003 << 6);

  // Init status light
  status_light::init();
  status_light::set_color(1, 0, 0);

  // Init the output enable line
  output_en::init();

  // Setup alert GPIO interrupt
  RCC_APB2ENR |= BIT_0;
  GPIO_A_MODER &= ~(0x0000C000); // PA7 Input mode
  EXTI_IMR |= BIT_7;  // Unmask EXTI line 7
  EXTI_FTSR |= BIT_7;  // Falling edge detection
  SYSCFG_EXTICR2 &= ~(0x0000F000);  // Interrupt 7 reads from PA7
  NVIC_ISER |= BIT_7;

  // Init the PD Controllers
  controller_a.init();
  controller_b.init();

  rtt_printf("Starlink PD Supply Booted");

  // Enable interruptS
  asm("CPSIE i");

  // We likely have an alert that was missed on power up so handle it now
  controller_a.handle_alert();
  controller_b.handle_alert();

  while(true) {
    if(read_pending)  {
      controller_a.handle_alert();
      controller_b.handle_alert();
      read_pending = false;
    }
    controller_a.tick();
    controller_b.tick();
  }

  return 0;
}
