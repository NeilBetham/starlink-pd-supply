#include "usb_pd_controller.h"

#include "registers/helpers.h"
#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"
#include "tcpc.h"
#include "utils.h"


void USBPDController::init() {
  _state = PDState::unknown;

  // Configure the PHY
  _phy.set_register(PHY_REG_TCPC_CTL, 0x2 << 2);  // Enable the TCPC to stretch the clock line
  _phy.set_register(PHY_REG_ALERT_MASK, 0x5FFF);
  _phy.set_register(PHY_REG_ROLE_CTL, 0x2A);  // 3A Sink only
  _phy.set_register(PHY_REG_RECV_DETECT, BIT_0 | BIT_5);  // SOP and hard resets
  _phy.set_register(PHY_REG_MSG_HDR_INFO, 0x02);  // Sink only, PD rev 2.0
  _phy.set_register(PHY_REG_FAULT_CTL, BIT_1);  // Disable OV fault
  _phy.set_register(PHY_REG_VBUS_V_ALRM_HI_CONF, 850);  // 20V Overvolt thresh

  // Setup the caps timer
  _caps_timer = system_time();

  // Check the cc state
  handle_cc_status();
}

void USBPDController::handle_alert() {
  // Read the alert status off of the HPY
  uint16_t alert_mask = _phy.get_register(PHY_REG_ALERT_MASK);
  uint16_t alert_status = _phy.get_register(PHY_REG_ALERT) & alert_mask;
  while(alert_status > 0) {
    if(alert_status & BIT_0) {
      // CC Status Alert
      rtt_printf("CC status");
      handle_cc_status();
      _phy.set_register(PHY_REG_ALERT, BIT_0);
   }

    if(alert_status & BIT_1) {
      // Port power status
      rtt_printf("Pwr status");
      uint16_t power_status = _phy.get_register(PHY_REG_POWER_STAT);
      _phy.set_register(PHY_REG_ALERT, BIT_1);
    }

    if(alert_status & BIT_2) {
      // SOP* RX
      handle_msg_rx();
    }

    if(alert_status & BIT_3) {
      // RX Hard reset
      rtt_printf("Hard reset");
      _phy.set_register(PHY_REG_ALERT, BIT_3);
      _msg_id_counter = 0;
      _delegate.controller_disconnected(*this);
    }

    if(alert_status & BIT_4) {
      // TX SOP* Failed
      rtt_printf("TX fail");
      _phy.set_register(PHY_REG_ALERT, BIT_4);
    }

    if(alert_status & BIT_5) {
      // TX SOP* Discarded
      rtt_printf("TX discard");
      _phy.set_register(PHY_REG_ALERT, BIT_5);
    }

    if(alert_status & BIT_6) {
      // TX SOP* Sucess
      rtt_printf("TX complete");
      _phy.set_register(PHY_REG_ALERT, BIT_6);
    }

    if(alert_status & BIT_7) {
      // VBUS V High
      rtt_printf("VBus v high");
      _phy.set_register(PHY_REG_ALERT, BIT_7);
    }

    if(alert_status & BIT_8) {
      // VBUS V Low
      rtt_printf("VBus v low");
      _phy.set_register(PHY_REG_ALERT, BIT_8);
    }

    if(alert_status & BIT_9) {
      // Fault, check fault reg
      rtt_printf("Fault");
      uint16_t fault_status = _phy.get_register(PHY_REG_FAULT_STAT);
      if(fault_status > 0) { _phy.set_register(PHY_REG_FAULT_STAT, 0xFFFF); }
      _phy.set_register(PHY_REG_ALERT, BIT_9);
    }

    if(alert_status & BIT_10) {
      // RX Buff Overflow
      // If we are here something has gone wrong
      // read all the contents and move on
      rtt_printf("RX buf ovfl");
      uint16_t readable_bytes = _phy.get_register(PHY_REG_READ_BYTE_COUNT);
      uint8_t msg_buffer[150] = {0};
      uint32_t msg_len = 0;
      while((readable_bytes & 0x0F) > 0) {
        _phy.rx_usb_pd_msg(msg_len, (uint8_t*)msg_buffer);
        readable_bytes = _phy.get_register(PHY_REG_READ_BYTE_COUNT);
      }
      _phy.set_register(PHY_REG_ALERT, BIT_10 | BIT_2);
    }

    if(alert_status & BIT_11) {
      // VBUS Sink Discon Detect
      _phy.set_register(PHY_REG_ALERT, BIT_11);
    }

    if(alert_status & BIT_12) {
      // Begin SOP* Message; for messages > 133 bytes
      rtt_printf("Long MSG");
      _phy.set_register(PHY_REG_ALERT, BIT_12);
    }

    if(alert_status & BIT_13) {
      // Extended Status Changed
      _phy.set_register(PHY_REG_ALERT, BIT_13);
    }

    if(alert_status & BIT_14) {
      // Alert extended changed
      rtt_printf("Ext alert");
      _phy.set_register(PHY_REG_ALERT, BIT_14);
    }

    if(alert_status & BIT_15) {
      // Vendor defined extended
      rtt_printf("Vendor RX");
      _phy.set_register(PHY_REG_ALERT, BIT_15);
    }
    alert_status = _phy.get_register(PHY_REG_ALERT) & alert_mask;
  }
}

void USBPDController::send_control_msg(ControlMessageType message_type) {
  MessageHeader message;
  setmem(&message, 0, sizeof(MessageHeader));

  message.message_type = (uint8_t)message_type;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(MessageHeader), (uint8_t*)&message);
}

void USBPDController::send_hard_reset() {
  _phy.hard_reset();
}

void USBPDController::set_vbus_sink(bool enabled) {
  if(enabled) {
    _phy.set_register(PHY_REG_COMMAND, 0x55);
  } else {
    _phy.set_register(PHY_REG_COMMAND, 0x44);
  }
}

void USBPDController::request_capability(const SourceCapability& capability) {
  request_capability(capability, capability.max_power());
}

void USBPDController::request_capability(const SourceCapability& capability, uint32_t power) {
  Request request(capability, power);
  send_request(request.generate_pdo());
}

void USBPDController::tick() {
  if(_caps_rx_attempts < 10 && _cc_partner && system_time() > (_caps_timer + 100) && _state == PDState::unknown) {
    // We know there is someone to talk to so ask for capabilities
    send_control_msg(ControlMessageType::get_source_cap);
    _caps_rx_attempts++;
    _state = PDState::init;
    _caps_reset_timer = system_time();
  }

  if(_caps_rx_attempts < 10 && _cc_partner && system_time() > (_caps_reset_timer + 100) && _state == PDState::init) {
    // We have sent a caps request but received no response, trigger a hard reset
    send_control_msg(ControlMessageType::soft_reset);
    _msg_id_counter = 0;
    _state = PDState::unknown;
    _caps_timer = system_time();
  }
}

void USBPDController::handle_msg_rx() {
  uint8_t msg_buffer[64] = {0};
  uint32_t msg_length = 0;
  _phy.rx_usb_pd_msg(msg_length, (uint8_t*)msg_buffer);
  _phy.set_register(PHY_REG_ALERT, BIT_2);

  MessageHeader* msg_header = (MessageHeader*)(msg_buffer);
  if(msg_header->num_data_obj == 0) {
    ControlMessageType message_type  = (ControlMessageType)(msg_header->message_type);
    switch(message_type) {
      case ControlMessageType::good_crc:
        // Good CRC, handled by TCPC
        rtt_printf("Good CRC");
        break;
      case ControlMessageType::goto_min:
        rtt_printf("Goto min");
        _delegate.go_to_min_received(*this);
        break;
      case ControlMessageType::accept:
        rtt_printf("Accept");
        _delegate.accept_received(*this);
        break;
      case ControlMessageType::reject:
        rtt_printf("Reject");
        _delegate.reject_received(*this);
        break;
      case ControlMessageType::ping:
        rtt_printf("Ping");
        break;
      case ControlMessageType::ps_rdy:
        rtt_printf("Ps rdy");
        _delegate.ps_ready_received(*this);
        break;
      case ControlMessageType::soft_reset:
        rtt_printf("Soft reset");
        _msg_id_counter = 0;
        send_control_msg(ControlMessageType::accept);
        _delegate.reset_received(*this);
      default:
        break;
    }
  } else {
    DataMessageType message_type = (DataMessageType)(msg_header->message_type);
    switch(message_type) {
      case DataMessageType::source_capabilities:
        rtt_printf("Caps RX");
        _state = PDState::caps_wait;
        handle_src_caps_msg(msg_buffer, msg_length);
        break;
      case DataMessageType::bist:
        break;
      case DataMessageType::sink_capabilities:
        break;
      case DataMessageType::vendor_defined:
        send_control_msg(ControlMessageType::reject);
        break;
      default:
        break;
    }
  }
}

void USBPDController::handle_src_caps_msg(const uint8_t* message, uint32_t len) {
  MessageHeader* msg_hdr = (MessageHeader*)message;
  PowerDataObject* pdos = (PowerDataObject*)(message + 2);

  _source_caps = SourceCapabilities(pdos, msg_hdr->num_data_obj);

  _delegate.capabilities_received(*this, _source_caps);
}

void USBPDController::handle_cc_status() {
  uint16_t cc_status = _phy.get_register(PHY_REG_CC_STAT);

  // Check the plug orientation
  uint8_t cc1_status = cc_status & 0x0003;
  uint8_t cc2_status = (cc_status & 0x000C) >> 2;

  // If cc2 is active we need to change the phy plug orientation
  if(cc2_status > 0) {
    _phy.set_register(PHY_REG_TCPC_CTL, BIT_0);
  }

  if(_cc_partner && (cc1_status | cc2_status) == 0) {
    _delegate.controller_disconnected(*this);
  }
  _cc_partner = cc1_status || cc2_status;
}

void USBPDController::send_request(const uint32_t& request_data) {
  MessageHeader req_hdr;
  setmem(&req_hdr, 0, sizeof(req_hdr));

  req_hdr.num_data_obj = 1;
  req_hdr.message_type = (uint8_t)DataMessageType::request;
  req_hdr.spec_rev = (uint8_t)SpecificationRev::two_v_zero;
  req_hdr.message_id = _msg_id_counter;
  _msg_id_counter++;

  uint8_t buffer[6] = {0};
  cpymem(buffer, &req_hdr, sizeof(req_hdr));
  cpymem(buffer + 2, &request_data, sizeof(request_data));

  _phy.tx_usb_pd_msg(sizeof(buffer), buffer);
}
