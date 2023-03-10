#include "stm_pd.h"

#include "circular_buffer.h"
#include "registers/rcc.h"
#include "registers/pd.h"
#include "registers/syscfg.h"
#include "rtt.h"
#include "time.h"
#include "utils.h"


#define K_CODE_SYNC1 0x00000018
#define K_CODE_SYNC2 0x00000011
#define K_CODE_SYNC3 0x00000006
#define K_CODE_RST1  0x00000007
#define K_CODE_RST2  0x00000019
#define K_CODE_EOP   0x0000000D

#define ORDSET_SOP            (K_CODE_SYNC1 | (K_CODE_SYNC1 << 5) | (K_CODE_SYNC1 << 10) | (K_CODE_SYNC2 << 15))
#define ORDSET_SOP_PRIME      (K_CODE_SYNC1 | (K_CODE_SYNC1 << 5) | (K_CODE_SYNC3 << 10) | (K_CODE_SYNC3 << 15))
#define ORDSET_SOP_PRIMEPRIME (K_CODE_SYNC1 | (K_CODE_SYNC3 << 5) | (K_CODE_SYNC1 << 10) | (K_CODE_SYNC3 << 15))
#define ORDSET_HARD_RESET     (K_CODE_RST1  | (K_CODE_RST1  << 5) | (K_CODE_RST1  << 10) | (K_CODE_RST2  << 15))

STMPD::STMPD(ControllerDelegate& delegate, PDPort port) : _delegate(delegate), _port(port) {

}

void STMPD::init() {
  switch(_port) {
    case PDPort::one:
      // Enable clock to the peripheral
      RCC_APBENR1 |= BIT_25;
      _base_addr = PD1_BASE;
      break;
    case PDPort::two:
      // Enable clock to the peripheral
      RCC_APBENR1 |= BIT_26;
      _base_addr = PD2_BASE;
      break;
    default:
      break;
  }

  // Setup the config for the port and enable it
  REGISTER(_base_addr + PD_CFG1_OFFSET) &= ~((0x1F << 6) | (0x1F << 11) | (0x7 << 17) | (0x1FF << 20));
  REGISTER(_base_addr + PD_CFG1_OFFSET) |=  ((0x0E << 6) | (0x09 << 11) | (0x0 << 17) | ((BIT_0 | BIT_3) << 20) | BIT_31);

  // Enable the PD detectors set to sink mode
  REGISTER(_base_addr + PD_CR_OFFSET) |=  (BIT_9 | (0x3 << 10));

  // Enable interrupts for Type C events
  REGISTER(_base_addr + PD_IMR_OFFSET) |= (BIT_15 | BIT_14 | BIT_10 | BIT_9);

  // Stobe SYSCFG to get control of CC lines
  SYSCFG_CFGR1 |= BIT_9 | BIT_10;

  // Do an initial pass checking the type c state
  handle_type_c_event();

  switch(_port) {
    case PDPort::one:
      rtt_print("Port 1 Init Done\r\n");
      break;
    case PDPort::two:
      rtt_print("Port 2 Init Done\r\n");
  }
}

void STMPD::tick() {
	if(_message_buff.can_pop()) {
    auto message = _message_buff.pop();
    handle_rx_buffer(message.buffer, message.size);
	}
  if(_caps_rx_timer != 0 && (system_time() - _caps_rx_timer) > 100) {
    send_control_msg(ControlMessageType::get_source_cap);
    _caps_rx_timer = 0;
  }
}

void STMPD::handle_interrupt() {
  uint32_t ifs = REGISTER(_base_addr + PD_SR_OFFSET);

  if(ifs & (BIT_15 | BIT_14)) {
    // Handle type c event
    handle_type_c_event();
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_15 | BIT_14;
  }

  if(ifs & BIT_10) {
    // Hard reset detected
    handle_hard_reset();
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_10;
  }

  if(ifs & BIT_9) {
    // RX detected
    RXMessage rx_message;
    recv_buffer(rx_message.buffer, 32, &rx_message.size);
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_9;

    if(rx_message.size > 0 && _message_buff.can_push()) {
			_message_buff.push(rx_message);
		}
  }
}

void STMPD::send_control_msg(ControlMessageType message_type) {
  MessageHeader message;
  setmem(&message, 0, sizeof(MessageHeader));

  message.message_type = (uint8_t)message_type;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = _message_id_counter++;

  send_buffer((uint8_t*)&message, sizeof(message));
}

void STMPD::send_hard_reset() {
  _message_id_counter = 0;
  REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_3;
}


void STMPD::request_capability(const SourceCapability& capability) {
  request_capability(capability, capability.max_power());
}

void STMPD::request_capability(const SourceCapability& capability, uint32_t power) {
  Request request(capability, power);
  MessageHeader request_header;
  setmem(&request_header, 0, sizeof(MessageHeader));

  uint32_t pdo = request.generate_pdo();

  request_header.num_data_obj = 0;
  request_header.message_type = (uint8_t)DataMessageType::request;
  request_header.spec_rev = (uint8_t)SpecificationRev::two_v_zero;
  request_header.message_id = _message_id_counter++;

  uint8_t buffer[6] = {0};
  cpymem(buffer, &request_header, sizeof(MessageHeader));
  cpymem(buffer + 2, &pdo, sizeof(uint32_t));

  send_buffer(buffer, 6);
}

void STMPD::send_buffer(const uint8_t* buffer, uint32_t size) {
  REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_2;

  // Setup ordered set and payload size
  REGISTER(_base_addr + PD_TX_ORDSET_OFFSET) = ORDSET_SOP;
  REGISTER(_base_addr + PD_TX_PAYSZ_OFFSET) = size;

  // Start the transmit process
  REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_2;

  // Start writing data
  for(uint32_t index = 0; index < size; index++) {
    while(!(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_0));
    REGISTER(_base_addr + PD_TXDR_OFFSET) = buffer[index];
  }

  // Check if the message was sent
  while(!(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_2));
  rtt_print("MSG Sent\r\n");
}

void STMPD::recv_buffer(uint8_t* buffer, uint32_t size, uint32_t* received) {
  *received = 0;
  // Ordered set has been recevied, assume it's SOP since this driver is sink only
  // Start reading bytes as they become available
  while(!(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_12)) {
    // Wait for a byte to be readable
    while(!(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_8));

    // Check if we are going to overflow the output buffer
    if(*received + 1 == size) {
      return;
    }

    // Read the byte
    buffer[*received] = REGISTER(_base_addr + PD_RXDR_OFFSET) & 0xFF;
    *received++;

  }

  // Check payload size
  if(REGISTER(_base_addr + PD_RX_PAYSZ_OFFSET) != *received) {
    *received = 0;
  }

  // Check for rx error
  if(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_13) {
    *received = 0;
  }

  // Clear the rx end flag
  REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_12;
}

void STMPD::handle_hard_reset() {
  _message_id_counter = 0;
  _delegate.reset_received(*this);
}

void STMPD::handle_type_c_event() {
  // Check if one of the phys has a voltage on it
  uint32_t pd_status = REGISTER(_base_addr + PD_SR_OFFSET);
  if(((pd_status & 0x00030000) >> 16) > 0) {
    rtt_print("CC1 Active\r\n");
    // CC1 active, set the phy to use CC1
    REGISTER(_base_addr + PD_CR_OFFSET) &= ~(BIT_6);

    // Enable RX
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_5;

    _caps_rx_timer = system_time();
  } else if (((pd_status & 0x000C0000) >> 18) > 0) {
    rtt_print("CC2 Active\r\n");
    // CC2 active, set the phy to use CC2
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_6;

    // Enable RX
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_5;

    _caps_rx_timer = system_time();
  } else {
    rtt_print("No CC Active\r\n");
    // No CC Active, disable RX
    REGISTER(_base_addr + PD_CR_OFFSET) &= ~(BIT_5);
  }

  REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_14 | BIT_15;
}

void STMPD::handle_rx_buffer(const uint8_t* buffer, uint32_t size) {
  MessageHeader* msg_header = (MessageHeader*)buffer;
  if(msg_header->num_data_obj == 0) {
    // Control message received
    ControlMessageType message_type = (ControlMessageType)(msg_header->message_type);
    switch(message_type) {
      case ControlMessageType::good_crc:
				rtt_print("Good CRC\r\n");
        break;
      case ControlMessageType::goto_min:
				rtt_print("Goto min\r\n");
        _delegate.go_to_min_received(*this);
        break;
      case ControlMessageType::accept:
				rtt_print("Accept\r\n");
        _delegate.accept_received(*this);
        break;
      case ControlMessageType::reject:
				rtt_print("Reject\r\n");
        _delegate.reject_received(*this);
        break;
      case ControlMessageType::ping:
				rtt_print("Ping\r\n");
        break;
      case ControlMessageType::ps_rdy:
				rtt_print("Ps rdy\r\n");
        _delegate.ps_ready_received(*this);
        break;
      case ControlMessageType::soft_reset:
				rtt_print("Soft reset\r\n");
        _message_id_counter = 0;
        send_control_msg(ControlMessageType::accept);
        _delegate.reset_received(*this);
        break;
      default:
        break;
    }
  } else {
    // Data message received
    DataMessageType message_type = (DataMessageType)(msg_header->message_type);
    switch(message_type) {
      case DataMessageType::source_capabilities:
        rtt_print("Caps RX\r\n");
        handle_src_caps_msg(buffer, size);
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

void STMPD::handle_src_caps_msg(const uint8_t* message, uint32_t len) {
  MessageHeader* msg_hdr = (MessageHeader*)message;
  PowerDataObject* pdos = (PowerDataObject*)(message + 2);

  _source_caps = SourceCapabilities(pdos, msg_hdr->num_data_obj);

  _delegate.capabilities_received(*this, _source_caps);
}


