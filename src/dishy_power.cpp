#include "dishy_power.h"

#include "registers/adc.h"
#include "registers/gpio.h"
#include "registers/rcc.h"
#include "rtt.h"
#include "time.h"

/**
 * GPIO Numbers
 * 48V Boost En - PA0
 * Dishy Current Sense - PA1 - ADC_IN1
 * Dishy Sense High - PA2 - ADC_IN2
 * Dishy Sense Low - PA3 - ADC_IN3
 * Dishy Power En - PA4
 * Dishy Sense En - PA5
 */

#define HIGH_SIDE_COUNTS_2V7 200
#define HIGH_SIDE_COUNTS_48V 3546
#define HIGH_SIDE_COUNTS_48V_BUFFER 300
#define CURRENT_SENSE_COUNT_5W 193
#define SENSE_VOLTAGE_MEDIAN_1V3 1613
#define SENSE_VOLTAGE_BUFFER_0V2 248


void DishyPower::init() {
  // Setup the GPIO
  RCC_IOPENR    |= BIT_0;
  GPIO_A_ODR    &= ~(BIT_0 | BIT_4 | BIT_5);
  GPIO_A_MODER  &= ~((0x3 << BIT0_POS) | (0x3 << BIT2_POS) | (0x3 << BIT4_POS) | (0x3 << BIT6_POS) | (0x3 << BIT8_POS) | (0x3 << BIT10_POS));
  GPIO_A_MODER  |=   (0x1 << BIT0_POS) | (0x3 << BIT2_POS) | (0x3 << BIT4_POS) | (0x3 << BIT6_POS) | (0x1 << BIT8_POS) | (0x1 << BIT10_POS);
  GPIO_A_OTYPER &= ~(BIT_0 | BIT_4 | BIT_5);

  // Setup ADC
  RCC_AHBENR  |= BIT_0 | BIT_1;
  RCC_APBENR2 |= BIT_20;

  // Enable the ADC vreg and wait for it to stabilize
  ADC_CR |= BIT_28;
  msleep(1);  // Only 20uS are really needed here

  // Run the ADC cal
  ADC_CR |= BIT_31;
  while(ADC_CR & BIT_31);
  ADC_ISR |= BIT_11;

  // Enable the ADC and wait for it to be ready
  ADC_CR |= BIT_0;
  while(!(ADC_ISR & BIT_0));

  // Setup the sampling config
  if(ADC_CFGR1 & BIT_21) {
    ADC_CFGR1 &= ~(BIT_21);  // Single bit channel sampling sequencing
    while(!(ADC_ISR & BIT_13));  // Wait for the CCRDY flag to set
    ADC_ISR |= BIT_13;
  }

  // Channels 1, 2 and 3 are selected
  ADC_CHSELR |= BIT_1 | BIT_2 | BIT_3;
  while(!(ADC_ISR & BIT_13));  // Wait for the CCRDY flag to set
  ADC_ISR |= BIT_13;

  // Set scan direction to low to high
  if(ADC_CFGR1 & BIT_2) {
    ADC_CFGR1 &= ~(BIT_2);
    while(!(ADC_ISR & BIT_13));  // Wait for the CCRDY flag to set
    ADC_ISR |= BIT_13;
  }

  // Setup the sampling time
  ADC_SMPR &= ~(BIT_9 | BIT_10 | BIT_11);
  ADC_SMPR |= (0x7 << BIT0_POS);

  // Capture the current no load counts
  run_adc_conversion();
  _current_no_load_counts = _current_counts;
}

void DishyPower::tick() {
  // Check if we were disabled
  if(!_power_output_enabled) {
    set_load_mode(LoadMode::disabled);
    set_boost(false);
    return;
  }

/* Disabled due to board level issues
  // Get the current state of the sense lines
  run_adc_conversion();

  // If dishy is connected then we need to monitor current
  // If dishy is not connected then we need to monitor sense voltage
  if(_dishy_connected) {
    monitor_current();
  } else {
    monitor_sense_voltage();
  }
*/

  // If dishy is connected then we can source power else go back into sense mode
  if(_dishy_connected) {
    set_load_mode(LoadMode::load_power);
  } else {
    set_load_mode(LoadMode::sense_voltage);
  }
}

void DishyPower::enable_power() {
  rtt_printf("Dishy power enable");
  _power_output_enabled = true;
}

void DishyPower::disable_power() {
  rtt_printf("Dishy power disable");
  _power_output_enabled = false;
}


void DishyPower::set_load_mode(LoadMode mode) {
  if(_current_mode == mode) {
    return;
  }
  switch(mode) {
    case LoadMode::disabled:
      rtt_printf("DishyPower Mode: Disabled");
      GPIO_A_ODR &= ~(BIT_4 | BIT_5);  // Disable both the load switch and the sense switch break; case LoadMode::
      break;
    case LoadMode::sense_voltage:
      rtt_printf("DishyPower Mode: Sense");
      set_sense_mode();
      break;
    case LoadMode::load_power:
      rtt_printf("DishyPower Mode: Load");
      set_load_power_mode();
      break;
    default:
      break;
  }

  _current_mode = mode;
}

void DishyPower::set_boost(bool enabled) {
  if(enabled) {
    GPIO_A_ODR |= BIT_0;
  } else {
    GPIO_A_ODR &= ~(BIT_0);
  }
}

void DishyPower::set_sense_mode() {
  // First disable load switch
  GPIO_A_ODR &= ~(BIT_4);

  // Disable the boost convertor
  set_boost(false);

  // Next we need to make sure that the high voltage has dropped
/*
  while(_high_side_counts > HIGH_SIDE_COUNTS_2V7) {
    run_adc_conversion();
  }
*/
  // Enable the sense side switch
  GPIO_A_ODR |= BIT_5;
}

void DishyPower::set_load_power_mode() {
  // First disable the sense side switch
  GPIO_A_ODR &= ~(BIT_5);

  // Wait some time for the switch to disable
  msleep(1);

  // Enable the boost convertor
  set_boost(true);

  // Wait for the output to reach regulation
/*
  while(_high_side_counts < HIGH_SIDE_COUNTS_48V - HIGH_SIDE_COUNTS_48V_BUFFER) {
    run_adc_conversion();
  }
*/
  // Enable the load switch
  GPIO_A_ODR |= BIT_4;
}

void DishyPower::run_adc_conversion() {
  // First up trigger the ADC and run a round of conversions
  ADC_CR |= BIT_2;
  while(!(ADC_ISR & BIT_2));
  _current_counts = ADC_DR;
  ADC_ISR |= BIT_2;

  while(!(ADC_ISR & BIT_2));
  _high_side_counts = ADC_DR;
  ADC_ISR |= BIT_2;

  while(!(ADC_ISR & BIT_2));
  _low_side_counts = ADC_DR;
  ADC_ISR |= BIT_2;
}

void DishyPower::monitor_current() {
  uint32_t current_counts = 0;
  // CYA with regards to subtracting negative unsigned ints
  if (_current_counts > _current_no_load_counts) {
    current_counts = _current_counts - _current_no_load_counts;
  }

  // Check if we are below the threshold of current
  if(current_counts < CURRENT_SENSE_COUNT_5W) {
    _current_below_thresh_counts++;
  } else {
    _current_below_thresh_counts = 0;
  }

  // Debounce check on being below the threshold
  if(_current_below_thresh_counts > 10) {
    _dishy_connected = false;
    _current_below_thresh_counts = 0;
  }
}

void DishyPower::monitor_sense_voltage() {
  // The voltage should be around 1.3 volts if dishy is connected
  if(_low_side_counts > (SENSE_VOLTAGE_MEDIAN_1V3 - SENSE_VOLTAGE_BUFFER_0V2) &&
      _low_side_counts < (SENSE_VOLTAGE_MEDIAN_1V3 + SENSE_VOLTAGE_BUFFER_0V2)) {
    _low_side_thresh_counts++;
  } else {
    _low_side_thresh_counts = 0;
  }

  // Debounce check on being in the threshold range
  if(_low_side_thresh_counts > 10) {
    _dishy_connected = true;
    _low_side_thresh_counts = 0;
  }
}

