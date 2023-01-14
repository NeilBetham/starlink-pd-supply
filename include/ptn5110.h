/**
 * @brief Manages a connected PTN5110 USB C PD PHY
 */

#include "i2c.h"
#include <cstddef>

#pragma once

class AlertDelegate {
public:
  virtual ~AlertDelegate() {};

  virtual void handle_alert() = 0;
};

class PTN5110 {
public:
  PTN5110(I2C& i2c_port, uint8_t device_addr): _i2c_port(i2c_port), _device_addr(device_addr) {};

  uint16_t get_register(uint8_t reg);
  bool set_register(uint8_t reg, uint16_t value);

  bool rx_usb_pd_msg(uint32_t& len, uint8_t* buffer);
  bool tx_usb_pd_msg(uint32_t len, const uint8_t* buffer);

  void hard_reset();

  // This should be called from an ISR
  void handle_alert();

  void set_delegate(AlertDelegate* alert_delegate) { _alert_delegate = alert_delegate; }

private:
  I2C& _i2c_port;
  uint8_t _device_addr = 0;
  uint8_t _message_counter = 0;
  AlertDelegate* _alert_delegate = NULL;
};
