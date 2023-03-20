/**
 * @brief STM32G0 PD Interface
 */

#include "circular_buffer.h"
#include "pd_protocol.h"

#pragma once


enum class PDPort : uint8_t {
  unknown = 0,
  one,
  two
};

struct RXMessage {
  uint8_t buffer[32] = {0};
  uint32_t size = 0;
};

class STMPD : public IController {
public:
  STMPD(ControllerDelegate& delegate, PDPort port);
  ~STMPD() {};

  void init();
  void tick();
  void handle_interrupt();

  void send_control_msg(ControlMessageType message_type);
  void send_control_msg(ControlMessageType message_type, uint8_t index);
  void send_hard_reset();

  const SourceCapabilities& caps() { return _source_caps; };
  void request_capability(const SourceCapability& capability);
  void request_capability(const SourceCapability& capability, uint32_t power);

private:
  ControllerDelegate& _delegate;
  SourceCapabilities _source_caps;
  PDPort _port;
  uint32_t _base_addr = 0;
  uint8_t _message_id_counter = 0;
  CircularBuffer<RXMessage, 5> _message_buff;
  uint32_t _caps_rx_timer = 0;
  uint32_t _hard_reset_timer = 0;

  void send_buffer(const uint8_t* buffer, uint32_t size);
  void recv_buffer(uint8_t* buffer, uint32_t size, uint32_t* received);

  void handle_hard_reset();
  void handle_type_c_event();
  void handle_rx_buffer(const uint8_t* buffer, uint32_t size);
  void handle_src_caps_msg(const uint8_t* message, uint32_t len);

  void enable_ints();
  void disable_ints();
};
