#include "i2c.h"

#include "registers/i2c.h"
#include "registers/rcc.h"

void I2C::init() {
  // First set the base address for accessing the different registers
  switch(_i2c_number) {
  case 1:
    _base_addr = I2C_1_BASE;
    break;
  case 2:
    _base_addr = I2C_2_BASE;
    break;
  case 3:
    _base_addr = I2C_3_BASE;
    break;
  }

  // Setup the clock source
  switch(_i2c_number) {
  case 1:
    RCC_CCIPR &= ~(BIT_12 | BIT_13);
    RCC_CCIPR |= BIT_13;
    break;
  case 2:
    // In this case the PLL is the source of the I2C2 clock
    break;
  case 3:
    RCC_CCIPR &= ~(BIT_16 | BIT_17);
    RCC_CCIPR |= BIT_17;
    break;
  }

  // Enable the clock to the I2C peripheral
  switch(_i2c_number) {
  case 1:
    RCC_APB1ENR |= BIT_21;
    break;
  case 2:
    RCC_APB1ENR |= BIT_22;
    break;
  case 3:
    RCC_APB1ENR |= BIT_30;
    break;
  }

  // Setup the prescaler and hold times
  // Defaulting to fast mode 400khz
  switch(_i2c_number) {
  case 1:
    // Assumes a 16 mhz clock source
    I2C_1_TIMINGR = 0;
    I2C_1_TIMINGR |= 0x9;  // SCLL
    I2C_1_TIMINGR |= 0x3 << 8; // SCLH
    I2C_1_TIMINGR |= 0x2 << 16; // SDADEL
    I2C_1_TIMINGR |= 0x3 << 20; // SCLDEL
    I2C_1_TIMINGR |= 0x1 << 28; // PRESC
    break;
  case 2:
    // Assumes a 16 mhz clock source
    I2C_2_TIMINGR = 0;
    I2C_2_TIMINGR |= 0x9;  // SCLL
    I2C_2_TIMINGR |= 0x3 << 8; // SCLH
    I2C_2_TIMINGR |= 0x2 << 16; // SDADEL
    I2C_2_TIMINGR |= 0x3 << 20; // SCLDEL
    I2C_2_TIMINGR |= 0x1 << 28; // PRESC
    break;
  case 3:
    // Assumes a 16 mhz clock source
    I2C_3_TIMINGR = 0;
    I2C_3_TIMINGR |= 0x9;  // SCLL
    I2C_3_TIMINGR |= 0x3 << 8; // SCLH
    I2C_3_TIMINGR |= 0x2 << 16; // SDADEL
    I2C_3_TIMINGR |= 0x3 << 20; // SCLDEL
    I2C_3_TIMINGR |= 0x1 << 28; // PRESC
    break;
  }

  // Set 7 bit address mode
  switch(_i2c_number) {
  case 1:
    I2C_1_CR2 &= ~(BIT_11);
    break;
  case 2:
    I2C_2_CR2 &= ~(BIT_11);
    break;
  case 3:
    I2C_3_CR2 &= ~(BIT_11);
    break;
  }

  // Enable the peripheral
  switch(_i2c_number) {
  case 1:
    I2C_1_CR1 |= BIT_0;
    break;
  case 2:
    I2C_2_CR1 |= BIT_0;
    break;
  case 3:
    I2C_3_CR1 |= BIT_0;
    break;
  }
}

int I2C::write_to(uint8_t addr, uint8_t reg, const uint8_t* data, uint32_t len) {
  // Set slave address
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~(0x000003FF);
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= (addr << 1);

  // Set transfer dir
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~BIT_10;

  // Set number of bytes to be transferred
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~(0xFF << 16);
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= (((len + 1) & 0xFF) << 16);

  // Unset auto end mode
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~BIT_25;

  // Start the transaction
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= BIT_13;

  // Start writing bytes
  // Register addr first
  while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_1));
  REGISTER(_base_addr + I2C_TXDR_OFFSET) = reg;

  // Then data
  for(uint32_t index = 0; index < len; index++) {
    while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_1));
    REGISTER(_base_addr + I2C_TXDR_OFFSET) = data[index];
  }

  // Wait for transmission complete
  while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_6));

  // Send stop
  REGISTER(_base_addr + I2C_CR2_OFFSET)  |= BIT_14;

  return len;
}

int I2C::read_from(uint8_t addr, uint8_t reg, uint8_t* data, uint32_t len) {
  // First write the register to the target device
  // Set slave address
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~(0x000003FF);
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= (addr << 1);

  // Set transfer dir
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~BIT_10;

  // Set number of bytes to be transferred
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~(0x000000FF << 16);
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= (0x1 << 16);

  // Unset auto end mode
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~BIT_25;

  // Start the transaction
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= BIT_13;

  // Write register addr
  while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_1));
  REGISTER(_base_addr + I2C_TXDR_OFFSET) = reg;

  // Wait for the transaction to complete
  while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_6));

  // Set the number of bytes to be received
  REGISTER(_base_addr + I2C_CR2_OFFSET) &= ~(0x000000FF << 16);
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= ((len & 0xFF) << 16);

  // Set transfer dir
  REGISTER(_base_addr + I2C_CR2_OFFSET) |= BIT_10;

  // Start the rx transaction
  REGISTER(_base_addr + I2C_CR2_OFFSET)  |= BIT_13;

  // Wait for the data to come in
  for(uint32_t index = 0; index < len; index++) {
    while(!(REGISTER(_base_addr + I2C_ISR_OFFSET) & BIT_2));
    data[index] = REGISTER(_base_addr + I2C_RXDR_OFFSET);
  }

  // Send stop
  REGISTER(_base_addr + I2C_CR2_OFFSET)  |= BIT_14;

  return len;
}


