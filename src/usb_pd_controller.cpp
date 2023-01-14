#include "usb_pd_controller.h"

#include "registers/helpers.h"

#include "status_light.h"
#include "output_en.h"
#include "rtt.h"
#include "time.h"

#include <string.h>


// PD Control Message Types
#define PD_MSG_TYPE_GOODCRC     0b0001
#define PD_MSG_TYPE_GOTOMIN     0b0010
#define PD_MSG_TYPE_ACCEPT      0b0011
#define PD_MSG_TYPE_REJECT      0b0100
#define PD_MSG_TYPE_PING        0b0101
#define PD_MSG_TYPE_PS_RDY      0b0110
#define PD_MSG_TYPE_GET_SRC_CAP 0b0111
#define PD_MSG_TYPE_GET_SNK_CAP 0b1000
#define PD_MSG_TYPE_DR_SWAP     0b1001
#define PD_MSG_TYPE_PR_SWAP     0b1010
#define PD_MSG_TYPE_VCONN_SWAP  0b1011
#define PD_MSG_TYPE_WAIT        0b1100
#define PD_MSG_TYPE_SOFT_RESET  0b1101

// PD Data Message Types
#define PD_MSG_TYPE_SRC_CAPS 0b0001
#define PD_MSG_TYPE_REQUEST  0b0010
#define PD_MSG_TYPE_BIST     0b0011
#define PD_MSG_TYPE_SNK_CAPS 0b0100
#define PD_MSG_TYPE_VND_DEF  0b1111

struct PACKED PDMsgHeader  {
  uint8_t message_type: 4;
  uint8_t : 1;
  uint8_t port_data_role: 1;
  uint8_t spec_rev: 2;
  uint8_t power_port_role: 1;
  uint8_t message_id: 3;
  uint8_t num_data_obj: 3;
  uint8_t : 1;
};


// Capability Defs
#define PD_CAP_TYPE_FIXED 0b00
#define PD_CAP_TYPE_BATT  0b01
#define PD_CAP_TYPE_VAR   0b10

struct PACKED SourceCapsFixedPDO {
  uint16_t max_current_10ma: 10;
  uint16_t voltage_50mv: 10;
  uint16_t peak_current: 2;
  uint16_t : 3;
  uint16_t dual_role_data: 1;
  uint16_t usb_comm_cap: 1;
  uint16_t uncon_power: 1;
  uint16_t suspend_sup: 1;
  uint16_t dual_role_power: 1;
  uint16_t supply_type: 2;
};

struct PACKED SourceCapsBattPDO {
  uint16_t max_power_250mw: 10;
  uint16_t min_voltage_50mv: 10;
  uint16_t max_voltage_50mv: 10;
  uint16_t  supply_type: 2;
};

struct PACKED SourceCapsVarPDO {
  uint16_t max_current_10ma: 10;
  uint16_t min_voltage_50mv: 10;
  uint16_t max_voltage_50mv: 10;
  uint16_t  supply_type: 2;
};

// Request Defs
struct PACKED RequestFixVarPDO {
  uint16_t max_current_10ma: 10;
  uint16_t op_current_10ma: 10;
  uint8_t : 4;
  uint8_t no_usb_sus: 1;
  uint8_t usb_comm_cap: 1;
  uint8_t cap_missmatch: 1;
  uint8_t give_back: 1;
  uint8_t object_pos: 3;
  uint8_t :1;
};

struct PACKED RequestBattPDO {
  uint16_t max_power_250mw: 10;
  uint16_t op_power_250mw: 10;
  uint8_t : 4;
  uint8_t no_usb_sus: 1;
  uint8_t usb_comm_cap: 1;
  uint8_t cap_missmatch: 1;
  uint8_t give_back: 1;
  uint8_t object_pos: 3;
  uint8_t :1;
};

// Phy Register Defines
#define PHY_REG_VENDOR_ID           0x00
#define PHY_REG_PROD_ID             0x02
#define PHY_REG_DEV_ID              0x04
#define PHY_REG_TC_REV              0x06
#define PHY_REG_PD_REV              0x08
#define PHY_REG_PD_INTER_REV        0x0A
#define PHY_REG_ALERT               0x10
#define PHY_REG_ALERT_MASK          0x12
#define PHY_REG_POWER_STAT_MASK     0x14
#define PHY_REG_FAULT_STAT_MASK     0x15
#define PHY_REG_EXT_STAT_MASK       0x16
#define PHY_REG_ALERT_EXT_STAT_MASK 0x17
#define PHY_REG_CONF_STD_OUTPUT     0x18
#define PHY_REG_TCPC_CTL            0x19
#define PHY_REG_ROLE_CTL            0x1A
#define PHY_REG_FAULT_CTL           0x1B
#define PHY_REG_POWER_CTL           0x1C
#define PHY_REG_CC_STAT             0x1D
#define PHY_REG_POWER_STAT          0x1E
#define PHY_REG_FAULT_STAT          0x1F
#define PHY_REG_EXTEND_STAT         0x20
#define PHY_REG_ALERT_EXT_STAT      0x21
#define PHY_REG_COMMAND             0x23
#define PHY_REG_DEVICE_CAPS_1       0x24
#define PHY_REG_DEVICE_CAPS_2       0x26
#define PHY_REG_STD_IN_CAPS         0x28
#define PHY_REG_STD_OUT_CAPS        0x29
#define PHY_REG_CONF_EXT_1          0x2A
#define PHY_REG_GEN_TIMER           0x2C
#define PHY_REG_MSG_HDR_INFO        0x2E
#define PHY_REG_RECV_DETECT         0x2F
#define PHY_REG_READ_BYTE_COUNT     0x30
#define PHY_REG_TRANSMIT            0x50
#define PHY_REG_I2C_WRITE_COUNT     0x51
#define PHY_REG_VBUS_VOLTAGE        0x70
#define PHY_REG_VBUS_SNK_DISCON_THR 0x72
#define PHY_REG_VBUS_STOP_DISCH_THR 0x74
#define PHY_REG_VBUS_V_ALRM_HI_CONF 0x76
#define PHY_REG_VBUS_V_ALRM_LO_CONF 0x78
#define PHY_REG_VBUS_HV_TARGET      0x7A
#define PHY_REG_EXT_CFG_ID          0x80
#define PHY_REG_EXT_ALERT           0x82
#define PHY_REG_EXT_ALERT_MASK      0x84
#define PHY_REG_EXT_CONF            0x86
#define PHY_REG_EXT_FAULT_CONF      0x88
#define PHY_REG_EXT_CTL             0x8E
#define PHY_REG_EXT_STAT            0x90
#define PHY_REG_EXT_GPIO_CONF       0x92
#define PHY_REG_EXT_GPIO_CTL        0x93
#define PHY_REG_EXT_GPIO_ALERT_CONF 0x94
#define PHY_REG_EXT_GPIO_STAT       0x96
#define PHY_REG_SRC_HV_MB4B_TIME    0x97
#define PHY_REG_ADC_FILT_CTL_1      0x9A
#define PHY_REG_ADC_FILT_CTL_2      0x9B
#define PHY_REG_VCONN_CONF          0x9C
#define PHY_REG_VCONN_FLT_DEBNC     0x9D
#define PHY_REG_VCONN_FLT_RECOV     0x9E
#define PHY_REG_VCONN_FLT_ATTMP     0x9F

void USBPDController::init() {
  _state = PDState::init;

  // Configure the PHY
  rtt_printf("Setting up PD PHY");
  _phy.set_register(PHY_REG_ROLE_CTL, 0x0A);  // Sink only
  _phy.set_register(PHY_REG_RECV_DETECT, BIT_0 | BIT_5);  // SOP and hard resets
  _phy.set_register(PHY_REG_MSG_HDR_INFO, 0x04);  // Sink only
  _phy.set_register(PHY_REG_FAULT_CTL, BIT_1);  // Disable OV fault
  _phy.set_register(PHY_REG_VBUS_V_ALRM_HI_CONF, 850);  // 20V Overvolt thresh

  _caps_timer = system_time();
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
  PDMsgHeader message;
  memset(&message, 0, sizeof(PDMsgHeader));

  message.message_type = PD_MSG_TYPE_GET_SRC_CAP;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(PDMsgHeader), (uint8_t*)&message);
}

void USBPDController::soft_reset() {
  rtt_printf("Soft reset tx");
  PDMsgHeader message;
  memset(&message, 0, sizeof(PDMsgHeader));

  message.message_type = PD_MSG_TYPE_SOFT_RESET;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _msg_id_counter;
  _msg_id_counter++;

  _phy.tx_usb_pd_msg(sizeof(PDMsgHeader), (uint8_t*)&message);
  _msg_id_counter = 0;
}

void USBPDController::hard_reset() {
  _phy.hard_reset();
}

void USBPDController::handle_msg_rx() {
  rtt_printf("Msg RX hndl");
  uint8_t msg_buffer[30] = {0};
  uint32_t msg_length = 0;
  _phy.rx_usb_pd_msg(msg_length, (uint8_t*)msg_buffer);
  _phy.set_register(PHY_REG_ALERT, BIT_2);

  PDMsgHeader* msg_header = (PDMsgHeader*)(msg_buffer + 2);
  if(msg_header->num_data_obj == 0) {
    switch(msg_header->message_type) {
      case 1:
        // Good CRC, handled by TCPC
        break;
      case 2:
        // Goto min
        break;
      case 3:
        handle_src_accept_msg();
        break;
      case 4:
        handle_src_reject_msg();
        break;
      case 5:
        // ping
        break;
      case 6:
        handle_src_ps_rdy_msg();
        break;
      default:
        break;
    }
  } else {
    switch(msg_header->message_type) {
      case 1:
        handle_src_caps_msg(msg_buffer, msg_length);
        break;
      default:
        break;
    }
  }
}

void USBPDController::tick() {
  uint32_t current_time = system_time();

  if(_state == PDState::init && (current_time - _caps_timer) > 30) {
    rtt_printf("Sending Caps request");
    send_caps_req();
  }
}

void USBPDController::handle_src_caps_msg(const uint8_t* message, uint32_t len) {
  const uint8_t* message_contents = message + 2;

  uint32_t selected_object_index = 0;
  uint16_t current = 0;
  uint16_t max_voltage = 0;
  uint32_t data_objects = ((PDMsgHeader*)message_contents)->num_data_obj;

  const uint32_t* pdos_start = (uint32_t*)(message_contents + 2);
  // Main requirement for selecting a capability is that it is over 15V at 2A
  for(uint32_t index = 0; index < data_objects; index++) {
    uint32_t pdo = *(pdos_start + index);
    uint8_t type = pdo & 0xC0000000 >> 30;

    if(type == PD_CAP_TYPE_FIXED) {
      // Fixed suply
      SourceCapsFixedPDO* fixed_pdo = (SourceCapsFixedPDO*)&pdo;
      uint32_t voltage = fixed_pdo->voltage_50mv;
      if((voltage >= 260) && (voltage > max_voltage)) {
        selected_object_index = index;
        current = fixed_pdo->max_current_10ma;
        max_voltage = voltage;
      }
    } else if(type == PD_CAP_TYPE_BATT) {
      // Battery Supply
      SourceCapsBattPDO* batt_pdo = (SourceCapsBattPDO*)&pdo;
      uint32_t voltage = batt_pdo->min_voltage_50mv;

      if((voltage >= 260) && (voltage > max_voltage)) {
        selected_object_index = index;
        current = batt_pdo->max_power_250mw;
        max_voltage = voltage;
      }
    } else if(type == PD_CAP_TYPE_VAR) {
      // Variable Supply
      SourceCapsVarPDO* var_pdo = (SourceCapsVarPDO*)&pdo;
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
    status_light::set_color(0, 1, 0);
  } else {
    SourceCapsFixedPDO* fixed_pdo = (SourceCapsFixedPDO*)&pdos_start;
    send_request(fixed_pdo->max_current_10ma, 1);
    status_light::set_color(1, 0, 0);
  }
}

void USBPDController::handle_src_accept_msg() {

}

void USBPDController::handle_src_reject_msg() {
  status_light::set_color(1, 0, 0);
}

void USBPDController::handle_src_ps_rdy_msg() {
  output_en::set_state(true);
}

void USBPDController::handle_reset_msg() {

}

void USBPDController::handle_good_crc_msg() {

}

void USBPDController::send_request(uint16_t current, uint8_t index) {
  PDMsgHeader req_hdr;
  RequestFixVarPDO request;
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
