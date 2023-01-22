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
  _phy.set_register(PHY_REG_ROLE_CTL, 0x0A);  // Sink only
  _phy.set_register(PHY_REG_RECV_DETECT, BIT_0 | BIT_5);  // SOP and hard resets
  _phy.set_register(PHY_REG_MSG_HDR_INFO, 0x04);  // Sink only
  _phy.set_register(PHY_REG_FAULT_CTL, BIT_1);  // Disable OV fault
  _phy.set_register(PHY_REG_VBUS_V_ALRM_HI_CONF, 850);  // 20V Overvolt thresh
}

void USBPDController::handle_alert() {
  // Read the alert status off of the HPY
  uint16_t alert_status = _phy.get_register(PHY_REG_ALERT);
  while(alert_status > 0) {
    if(alert_status & BIT_0) {
      // CC Status Alert
      handle_cc_status();
   }

    if(alert_status & BIT_1) {
      // Port power status
      uint16_t power_status = _phy.get_register(PHY_REG_POWER_STAT);
      _phy.set_register(PHY_REG_ALERT, BIT_1);
    }

    if(alert_status & BIT_2) {
      // SOP* RX
      handle_msg_rx();
    }

    if(alert_status & BIT_3) {
      // RX Hard reset
      _phy.set_register(PHY_REG_ALERT, BIT_3);
    }

    if(alert_status & BIT_4) {
      // TX SOP* Failed
      _phy.set_register(PHY_REG_ALERT, BIT_4);
    }

    if(alert_status & BIT_5) {
      // TX SOP* Discarded
      _phy.set_register(PHY_REG_ALERT, BIT_5);
    }

    if(alert_status & BIT_6) {
      // TX SOP* Sucess
      _phy.set_register(PHY_REG_ALERT, BIT_6);
    }

    if(alert_status & BIT_7) {
      // VBUS V High
      _phy.set_register(PHY_REG_ALERT, BIT_7);
    }

    if(alert_status & BIT_8) {
      // VBUS V Low
      _phy.set_register(PHY_REG_ALERT, BIT_8);
    }

    if(alert_status & BIT_9) {
      // Fault, check fault reg
      uint16_t fault_status = _phy.get_register(PHY_REG_FAULT_STAT);
      if(fault_status > 0) { _phy.set_register(PHY_REG_FAULT_STAT, 0xFFFF); }
      _phy.set_register(PHY_REG_ALERT, BIT_9);
    }

    if(alert_status & BIT_10) {
      // RX Buff Overflow
      _phy.set_register(PHY_REG_ALERT, BIT_10);
    }

    if(alert_status & BIT_11) {
      // VBUS Sink Discon Detect
      _phy.set_register(PHY_REG_ALERT, BIT_11);
    }

    if(alert_status & BIT_12) {
      // Begin SOP* Message; for messages > 133 bytes
      _phy.set_register(PHY_REG_ALERT, BIT_12);
    }

    if(alert_status & BIT_13) {
      // Extended Status Changed
      _phy.set_register(PHY_REG_ALERT, BIT_13);
    }

    if(alert_status & BIT_14) {
      // Alert extended changed
      _phy.set_register(PHY_REG_ALERT, BIT_14);
    }

    if(alert_status & BIT_15) {
      // Vendor defined extended
      _phy.set_register(PHY_REG_ALERT, BIT_15);
    }
    alert_status = _phy.get_register(PHY_REG_ALERT);
  }
}

void USBPDController::send_caps_req() {
  MessageHeader message;
  setmem(&message, 0, sizeof(MessageHeader));

  message.message_type = (uint8_t)ControlMessageType::get_source_cap;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(MessageHeader), (uint8_t*)&message);
}

void USBPDController::soft_reset() {
  MessageHeader message;
  setmem(&message, 0, sizeof(MessageHeader));

  message.message_type = (uint8_t)ControlMessageType::soft_reset;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(MessageHeader), (uint8_t*)&message);
  _msg_id_counter = 0;
}

void USBPDController::hard_reset() {
  _phy.hard_reset();
}

void USBPDController::request_capability(const SourceCapability& capability) {
  request_capability(capability, capability.max_power());
}

void USBPDController::request_capability(const SourceCapability& capability, uint32_t power) {
  Request request(capability, power);
  send_request(request.generate_pdo());
}


void USBPDController::handle_msg_rx() {
  uint8_t msg_buffer[30] = {0};
  uint32_t msg_length = 0;
  _phy.rx_usb_pd_msg(msg_length, (uint8_t*)msg_buffer);
  _phy.set_register(PHY_REG_ALERT, BIT_2);

  MessageHeader* msg_header = (MessageHeader*)(msg_buffer + 2);
  if(msg_header->num_data_obj == 0) {
    ControlMessageType message_type  = (ControlMessageType)(msg_header->message_type);
    switch(message_type) {
      case ControlMessageType::good_crc:
        // Good CRC, handled by TCPC
        break;
      case ControlMessageType::goto_min:
        _delegate.go_to_min_received(*this);
        break;
      case ControlMessageType::accept:
        _delegate.accept_received(*this);
        break;
      case ControlMessageType::reject:
        _delegate.reject_received(*this);
        break;
      case ControlMessageType::ping:
        break;
      case ControlMessageType::ps_rdy:
        _delegate.ps_ready_received(*this);
        break;
      case ControlMessageType::soft_reset:
        _delegate.reset_received(*this);
      default:
        break;
    }
  } else {
    DataMessageType message_type = (DataMessageType)(msg_header->message_type);
    switch(message_type) {
      case DataMessageType::source_capabilities:
        handle_src_caps_msg(msg_buffer, msg_length);
        break;
      case DataMessageType::bist:
        break;
      case DataMessageType::sink_capabilities:
        break;
      case DataMessageType::vendor_defined:
      default:
        break;
    }
  }
}

void USBPDController::handle_src_caps_msg(const uint8_t* message, uint32_t len) {
  const uint8_t* msg_start = message + 2;
  MessageHeader* msg_hdr = (MessageHeader*)msg_start;
  PowerDataObject* pdos = (PowerDataObject*)msg_start + 2;

  _source_caps = SourceCapabilities(pdos, msg_hdr->num_data_obj);

  _delegate.capabilities_received(*this, _source_caps);
}

void USBPDController::handle_cc_status() {
      uint16_t cc_status = _phy.get_register(PHY_REG_CC_STAT);

      // Check the plug orientation
      uint8_t cc1_status = cc_status & 0x0003;
      uint8_t cc2_status = (cc_status & 0x000C) >> 2;

      if(cc2_status > 0) {
        _phy.set_register(PHY_REG_TCPC_CTL, BIT_0);
      }

      if(cc_status & 0x0000000F > 0) {
        _state = PDState::init;
      }

      _phy.set_register(PHY_REG_ALERT, BIT_0);
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
