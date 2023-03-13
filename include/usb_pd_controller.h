/**
 * @brief Handle the implementation and management of the USB PD protocol
 */

#include "ptn5110.h"
#include "pd_protocol.h"


#pragma once

enum class PDState {
  unknown = 0,
  init,         // State to be entered while waiting for a src caps message
  caps_wait,    // A caps request has been sent we're waiting for a repsonse
  need_resp,    // We've sent a pd request and are waiting for a respose
  accepted,     // Src has accepted the request waiting for ps_rdy
  rejected,     // Src has reject our request indicate error
  ps_rdy,       // Src supply is ready to source current
  fault         // A fault has occured indicate error
};


class USBPDController : public AlertDelegate, public IController {
public:
  USBPDController(PTN5110& phy, ControllerDelegate& delegate) : _phy(phy), _delegate(delegate) {
    _phy.set_delegate(this);
  };
  ~USBPDController() {
    _phy.set_delegate(NULL);
  }


  void init();

  // AlertDelegate
  void handle_alert();

  void send_control_msg(ControlMessageType message_type);
  void send_hard_reset();
  void set_vbus_sink(bool enabled);

  const SourceCapabilities& caps() { return _source_caps; };
  void request_capability(const SourceCapability& capability);
  void request_capability(const SourceCapability& capability, uint32_t power);

  // Peridodic interface for handling timer based events
  void tick();

private:
  PTN5110& _phy;
  ControllerDelegate& _delegate;
  SourceCapabilities _source_caps;

  void handle_msg_rx();
  void handle_src_caps_msg(const uint8_t* message, uint32_t len);
  void handle_cc_status();

  void send_request(const uint32_t& request_data);

  uint8_t _msg_id_counter = 0;
  PDState _state = PDState::unknown;
  uint32_t _caps_timer = 0;
  uint32_t _caps_reset_timer = 0;
  bool _cc_partner = false;
  uint32_t _caps_rx_attempts = 0;
};
