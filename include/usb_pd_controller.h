/**
 * @brief Handle the implementation and management of the USB PD protocol
 */

#include "ptn5110.h"

#pragma once

enum class PDState {
  unknown = 0,
  init,         // Inital state to be entered while waiting for a src caps message
  need_resp,    // We've sent a pd request and are waiting for a respose
  accepted,     // Src has accepted the request waiting for ps_rdy
  rejected,     // Src has reject our request indicate error
  ps_rdy,       // Src supply is ready to source current
  fault         // A fault has occured indicate error
};


class USBPDController : public AlertDelegate {
public:
  USBPDController(PTN5110& phy) : _phy(phy) {
    _phy.set_delegate(this);
  };
  ~USBPDController() {
    _phy.set_delegate(NULL);
  }


  void init();

  // AlertDelegate
  void handle_alert();

  // Send caps request
  void send_caps_req();
  void soft_reset();
  void hard_reset();

  // Handle tasks at periodic intervals
  void tick();

private:
  PTN5110& _phy;

  void handle_msg_rx();
  void handle_src_caps_msg(const uint8_t* message, uint32_t len);
  void handle_src_accept_msg();
  void handle_src_reject_msg();
  void handle_src_ps_rdy_msg();
  void handle_reset_msg();
  void handle_good_crc_msg();

  void send_request(uint16_t current, uint8_t index);

  uint8_t _msg_id_counter = 0;
  PDState _state = PDState::unknown;

  uint8_t _caps_timer = 0;
};
