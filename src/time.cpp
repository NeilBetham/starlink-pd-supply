#include "time.h"

#include "registers/core.h"

#define CYCLES_PER_MS 64000

static volatile uint32_t msec_clock = 0;

// Interrupt handler
void SysTick_Handler() {
  msec_clock++;
}

void systick_init() {
  STK_CSR = 0;
  STK_RVR = CYCLES_PER_MS & 0x00FFFFFF;
  STK_CVR = 0;
  STK_CSR |= BIT_1 | BIT_2;
  STK_CSR |= BIT_0;
}

void sleep(uint32_t seconds) {
  for(uint32_t index = 0; index < seconds; index++) {
    msleep(1000);
  }
}

void msleep(uint32_t milli_seconds) {
  uint32_t start_time = system_time();
  uint32_t goal_time = start_time + milli_seconds;
  while(system_time() <= goal_time);
}

uint32_t system_time() {
  return msec_clock;
}
