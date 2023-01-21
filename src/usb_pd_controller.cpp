#include "usb_pd_controller.h"

#include <string.h>

#include "registers/helpers.h"
#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"
#include "tcpc.h"
#include "pd_protocol.h"


void USBPDController::init() {
  _state = PDState::unknown;

  // Configure the PHY
  rtt_printf("Setting up PD PHY");
  _phy.set_register(PHY_REG_ROLE_CTL, 0x0A);  // Sink only
  _phy.set_register(PHY_REG_RECV_DETECT, BIT_0 | BIT_5);  // SOP and hard resets
  _phy.set_register(PHY_REG_MSG_HDR_INFO, 0x04);  // Sink only
  _phy.set_register(PHY_REG_FAULT_CTL, BIT_1);  // Disable OV fault
  _phy.set_register(PHY_REG_VBUS_V_ALRM_HI_CONF, 850);  // 20V Overvolt thresh
}

void USBPDController::handle_alert() {
  rtt_printf("Handling alert from PD PHY");
  // Read the alert status off of the HPY
  uint16_t alert_status = _phy.get_register(PHY_REG_ALERT);
  while(alert_status > 0) {
    if(alert_status & BIT_0) {
      rtt_printf("CC Status");
      // CC Status Alert
      uint16_t cc_status = _phy.get_register(PHY_REG_CC_STAT);

      // Check the plug orientation
      uint8_t cc1_status = cc_status & 0x0003;
      uint8_t cc2_status = (cc_status & 0x000C) >> 2;

      if(cc2_status > 0) {
        rtt_printf("CC2 Active");
        _phy.set_register(PHY_REG_TCPC_CTL, BIT_0);
      } else {
        rtt_printf("CC1 Active");
      }

      if(cc_status & 0x0000000F > 0) {
        _state = PDState::init;
        _caps_timer = system_time();
      }

      _phy.set_register(PHY_REG_ALERT, BIT_0);
    }

    if(alert_status & BIT_1) {
      rtt_printf("Power Status");
      // Port power status
      uint16_t power_status = _phy.get_register(PHY_REG_POWER_STAT);
      _phy.set_register(PHY_REG_ALERT, BIT_1);
    }

    if(alert_status & BIT_2) {
      rtt_printf("MSG RX");
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
      rtt_printf("Fault");
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
  rtt_printf("Caps req tx");
  MessageHeader message;
  memset(&message, 0, sizeof(MessageHeader));

  message.message_type = PD_MSG_TYPE_GET_SRC_CAP;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(MessageHeader), (uint8_t*)&message);
}

void USBPDController::soft_reset() {
  rtt_printf("Soft reset tx");
  MessageHeader message;
  memset(&message, 0, sizeof(MessageHeader));

  message.message_type = PD_MSG_TYPE_SOFT_RESET;
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

void USBPDController::handle_msg_rx() {
  rtt_printf("Msg RX handle");
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
        rtt_printf("Goto Min RX");
        output_en::set_state(false);
        status_light::set_color(1, 0, 1);
        break;
      case ControlMessageType::accept:
        rtt_printf("Accept Msg RX");
        handle_src_accept_msg();
        break;
      case ControlMessageType::reject:
        rtt_printf("Reject Msg RX");
        handle_src_reject_msg();
        break;
      case ControlMessageType::ping:
        // ping
        break;
      case ControlMessageType::ps_rdy:
        rtt_printf("PS Rdy Msg RX");
        handle_src_ps_rdy_msg();
        break;
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
        rtt_printf("BIST Msg RX");
        break;
      case DataMessageType::sink_capabilities:
        rtt_printf("Sink Caps Msg RX");
        break;
      case DataMessageType::vendor_defined:
        rtt_printf("Vendor Def Msg RX");
      default:
        break;
    }
  }
}

void USBPDController::tick() {
  uint32_t current_time = system_time();

  if(_state == PDState::unknown) {
    soft_reset();
    _state = PDState::init;
    _caps_timer = system_time();
  }

  if(_state == PDState::init && (current_time - _caps_timer) > 30) {
    rtt_printf("Sending Caps request");
    send_caps_req();
    _state = PDState::caps_wait;
  }
}

void USBPDController::handle_src_caps_msg(const uint8_t* message, uint32_t len) {
  const uint8_t* message_contents = message + 2;
  _state = PDState::need_resp;

  uint32_t selected_object_index = 0;
  uint16_t current = 0;
  uint16_t max_voltage = 0;
  uint32_t data_objects = ((MessageHeader*)message_contents)->num_data_obj;

  const uint32_t* pdos_start = (uint32_t*)(message_contents + 2);
  // Main requirement for selecting a capability is that it is over 15V at 2A
  for(uint32_t index = 0; index < data_objects; index++) {
    uint32_t pdo = *(pdos_start + index);
    PowerDataObjectType type = (PowerDataObjectType)(pdo & 0xC0000000 >> 30);

    if(type == PowerDataObjectType::fixed) {
      // Fixed suply
      FixedPowerDataObject* fixed_pdo = (FixedPowerDataObject*)&pdo;
      uint32_t voltage = fixed_pdo->voltage_50mv;
      if((voltage >= 260) && (voltage > max_voltage)) {
        selected_object_index = index;
        current = fixed_pdo->max_current_10ma;
        max_voltage = voltage;
      }
    } else if(type == PowerDataObjectType::battery) {
      // Battery Supply
      BatteryPowerDataObject* batt_pdo = (BatteryPowerDataObject*)&pdo;
      uint32_t voltage = batt_pdo->min_voltage_50mv;

      if((voltage >= 260) && (voltage > max_voltage)) {
        selected_object_index = index;
        current = batt_pdo->max_power_250mw;
        max_voltage = voltage;
      }
    } else if(type == PowerDataObjectType::variable) {
      // Variable Supply
      VariablePowerDataObject* var_pdo = (VariablePowerDataObject*)&pdo;
      uint32_t voltage = var_pdo->min_voltage_50mv;

      if((voltage >= 260) && (voltage > max_voltage)) {
        selected_object_index = index;
        current = var_pdo->max_current_10ma;
        max_voltage = voltage;
      }
    }
  }

  if(selected_object_index > 0) {
    send_request(current, selected_object_index);
  } else {
    FixedPowerDataObject* fixed_pdo = (FixedPowerDataObject*)&pdos_start;
    send_request(fixed_pdo->max_current_10ma, 1);
    status_light::set_color(1, 0, 0);
  }
}

void USBPDController::handle_src_accept_msg() {
  _state = PDState::accepted;
}

void USBPDController::handle_src_reject_msg() {
  _state = PDState::rejected;
  status_light::set_color(1, 0, 0);
}

void USBPDController::handle_src_ps_rdy_msg() {
  _state = PDState::ps_rdy;
  status_light::set_color(0, 1, 0);
  output_en::set_state(true);
}

void USBPDController::handle_reset_msg() {

}

void USBPDController::handle_good_crc_msg() {

}

void USBPDController::send_request(uint16_t current, uint8_t index) {
  MessageHeader req_hdr;
  FixedRequestPowerDataObject request;
  memset(&req_hdr, 0, sizeof(req_hdr));
  memset(&request, 0, sizeof(request));

  req_hdr.num_data_obj = 0x1;
  req_hdr.message_type = 0x2;
  req_hdr.spec_rev = 0x1;
  req_hdr.message_id = _msg_id_counter;
  _msg_id_counter++;

  request.max_current_10ma = current;
  request.op_current_10ma = current;
  request.object_pos = index + 1;

  uint8_t buffer[6] = {0};
  memcpy(buffer, &req_hdr, sizeof(req_hdr));
  memcpy(buffer + 2, &request, sizeof(request));

  _phy.tx_usb_pd_msg(6, buffer);
}
