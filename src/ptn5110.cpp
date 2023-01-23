#include "ptn5110.h"

#include "utils.h"

uint16_t PTN5110::get_register(uint8_t reg) {
  uint16_t buffer = 0;
  _i2c_port.read_from(_device_addr, reg, (uint8_t*)&buffer, 2);
  return buffer;
}

bool PTN5110::set_register(uint8_t reg, uint16_t value) {
  uint16_t buffer = 0;
  _i2c_port.write_to(_device_addr, reg, (uint8_t*)&value, 2);
  _i2c_port.read_from(_device_addr, reg, (uint8_t*)&buffer, 2);
  return buffer = value;
}

bool PTN5110::rx_usb_pd_msg(uint32_t& len, uint8_t* buffer) {
  uint8_t rx_length = 0;
  uint8_t temp_buffer[64] = {0};

  _i2c_port.read_from(_device_addr, 0x30, &rx_length, 1);
  if(rx_length < 1) {
    len = 0;
    return false;
  }

  _i2c_port.read_from(_device_addr, 0x30, &temp_buffer[0], rx_length + 1);

  cpymem(buffer, temp_buffer + 2, rx_length - 1);

  len = rx_length - 1;

  return true;
}

bool PTN5110::tx_usb_pd_msg(uint32_t len, const uint8_t* buffer) {
  uint8_t tx_settings = 0x30;  // TX SOP message with 3 retries
  uint8_t tx_buff[128] = {0};
  tx_buff[0] = len;
  cpymem(&tx_buff[1], buffer, len);
  _i2c_port.write_to(_device_addr, 0x51, tx_buff, len + 1);
  _i2c_port.write_to(_device_addr, 0x50, &tx_settings, 1);
  return true;
}

void PTN5110::hard_reset() {
  uint8_t tx_settings = 0x05;
  _i2c_port.write_to(_device_addr, 0x50, &tx_settings, 1);
}

void PTN5110::handle_alert() {
  if(_alert_delegate) {
    _alert_delegate->handle_alert();
  }
}
