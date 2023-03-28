#include "stm_pd.h"

#include "circular_buffer.h"
#include "registers/dma.h"
#include "registers/pd.h"
#include "registers/rcc.h"
#include "registers/syscfg.h"
#include "rtt.h"
#include "status_light.h"
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

STMPD::STMPD(PDPort port) : _port(port) {}

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

  // Enable DMA Clocks
  RCC_AHBENR |= BIT_0 | BIT_1;

  // Setup the config for the port and enable it
  REGISTER(_base_addr + PD_CFG1_OFFSET) &= ~((0x1F << 6) | (0x1F << 11) | (0x7 << 17) | (0x1FF << 20));
  REGISTER(_base_addr + PD_CFG1_OFFSET) |=  (0x0D | (0x10 << 6) | (0x08 << 11) | (0x1 << 17) | ((BIT_0 | BIT_3) << 20) | BIT_29 | BIT_30);
  REGISTER(_base_addr + PD_CFG1_OFFSET) |=  BIT_31;

  // Setup DMA for RX first
  // Port 1 -> DMA Chan 1
  // Port 2 -> DMA Chan 2
  switch(_port) {
    case PDPort::one:
      DMA_1_CCR1   &= ~(0x00007FFF);
      DMA_1_CCR1   |=  (0x3 << BIT12_POS) | BIT_7;
      DMA_1_CNDTR1 &= ~(0x0000FFFF);
      DMA_1_CNDTR1 |=  PD_BUFFER_SIZE;
      DMA_1_CPAR1  = _base_addr + PD_RXDR_OFFSET;
      DMA_1_CMAR1  = (uint32_t)_rx_buff_a.buffer;

      // Setup DMAMUX Channel 1- UCPD1_RX -> MUX Input 58
      DMA_MUX_C0CR &= ~((0xF << 24) | (0x3 << 17) | BIT_16 | BIT_9 | BIT_8 | (0x7F));
      DMA_MUX_C0CR |=  58;

      // Enabled the DMA Channel
      DMA_1_CCR1 |= BIT_0;
      break;

    case PDPort::two:
      DMA_1_CCR2   &= ~(0x00007FFF);
      DMA_1_CCR2   |=  (0x3 << BIT12_POS) | BIT_7;
      DMA_1_CNDTR2 &= ~(0x0000FFFF);
      DMA_1_CNDTR2 |=  PD_BUFFER_SIZE;
      DMA_1_CPAR2  = _base_addr + PD_RXDR_OFFSET;
      DMA_1_CMAR2  = (uint32_t)_rx_buff_a.buffer;

      // Setup DMAMUX Channel 2- UCPD2_RX -> MUX Input 60
      DMA_MUX_C1CR &= ~((0xF << 24) | (0x3 << 17) | BIT_16 | BIT_9 | BIT_8 | (0x7F));
      DMA_MUX_C1CR |=  60;

      // Enabled the DMA Channel
      DMA_1_CCR2 |= BIT_0;
      break;

    default:
     break;
  }

  // Setup DMA for TX
  // Port 1 -> DMA Chan 3 -> Mux Input 59
  // Port 2 -> DMA Chan 4 -> Mux Input 61
  switch(_port) {
    case PDPort::one:
      // Configure the DMA mux
      DMA_MUX_C2CR &= ~((0xF << 24) | (0x3 << 17) | BIT_16 | BIT_9 | BIT_8 | (0x7F));
      DMA_MUX_C2CR |= 59;
      break;
    case PDPort::two:
      // Configure the DMA mux
      DMA_MUX_C3CR &= ~((0xF << 24) | (0x3 << 17) | BIT_16 | BIT_9 | BIT_8 | (0x7F));
      DMA_MUX_C3CR |= 61;
      break;
    default:
      break;
  }

  // Enable the PD detectors set to sink mode
  REGISTER(_base_addr + PD_CR_OFFSET) |=  (BIT_9 | (0x3 << 10));

  // Enable interrupts for Type C events
  enable_ints();

  // Stobe SYSCFG to update resistors on CC lines
  switch(_port) {
    case PDPort::one:
      SYSCFG_CFGR1 |= BIT_9;
      break;
    case PDPort::two:
      SYSCFG_CFGR1 |= BIT_10;
      break;
    default:
      break;
  }

  // Sleep for a bit to let the PHYs detect the CC line state
  msleep(1);

  // Do an initial pass checking the type c state
  handle_type_c_event();

  switch(_port) {
    case PDPort::one:
      rtt_printf("Port 1 Init Done");
      break;
    case PDPort::two:
      rtt_printf("Port 2 Init Done");
  }
}

void STMPD::tick() {
	if(_rx_message_buff.can_pop()) {
    auto message = _rx_message_buff.pop();
    if(message.hard_reset) {
      handle_hard_reset();
    } else {
      handle_rx_buffer(message.buffer, message.size);
    }
	}
  if(_caps_rx_timer != 0 && (system_time() - _caps_rx_timer) > 100) {
    send_control_msg(ControlMessageType::get_source_cap);
    _caps_rx_timer = 0;
    _hard_reset_timer = system_time();
  }
  if(_hard_reset_timer != 0 && (system_time() - _hard_reset_timer) > 200) {
    send_hard_reset();
    _hard_reset_timer = 0;
  }

  if(_tx_message_buff.can_pop()) {
    start_tx_dma();
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
    RXMessage rx_hard_reset;
    rx_hard_reset.hard_reset = true;
    if(_rx_message_buff.can_push()) {
      _rx_message_buff.push(rx_hard_reset);
    }
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_10;
  }

  if(ifs & BIT_12) {
    // RX DMA Complete
    handle_rx_dma();
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_12;
  }

  if(ifs & BIT_2) {
    // TX DMA Complete
    _tx_dma_inflight = false;
    _tx_message_buff.pop();
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_2;
  }
}

void STMPD::send_control_msg(ControlMessageType message_type) {
  send_control_msg(message_type, _message_id_counter++);
}

void STMPD::send_control_msg(ControlMessageType message_type, uint8_t index) {
  MessageHeader message;
  setmem(&message, 0, sizeof(MessageHeader));

  message.message_type = (uint8_t)message_type;
  message.num_data_obj = 0;
  message.spec_rev = 1;
  message.message_id = index & 0x07;

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

  request_header.num_data_obj = 1;
  request_header.message_type = (uint8_t)DataMessageType::request;
  request_header.spec_rev = (uint8_t)SpecificationRev::two_v_zero;
  request_header.message_id = _message_id_counter++ & 0x07;

  uint8_t buffer[6] = {0};
  cpymem(buffer, &request_header, sizeof(MessageHeader));
  cpymem(buffer + 2, &pdo, sizeof(uint32_t));

  send_buffer(buffer, 6);
}

void STMPD::send_buffer(const uint8_t* buffer, uint32_t size) {
  TXMessage msg;
  msg.size = size;
  cpymem(msg.buffer, buffer, size);

  if(_tx_message_buff.can_push()) {
    _tx_message_buff.push(msg);
  }

  start_tx_dma();
}

void STMPD::handle_rx_dma() {
  bool buffer_a = true;
  uint32_t dma_payload_size = 0;

  // Handle the DMA specific shit first
  switch(_port) {
    case PDPort::one:
      // Setup the DMA on the other buffer while we process this one
      dma_payload_size = PD_BUFFER_SIZE - DMA_1_CNDTR1;
      DMA_1_CCR1 &= ~(BIT_0);

      if(DMA_1_CMAR1 == (uint32_t)_rx_buff_a.buffer) {
        DMA_1_CMAR1 = (uint32_t)_rx_buff_b.buffer;
      } else {
        buffer_a = false;
        DMA_1_CMAR1 = (uint32_t)_rx_buff_a.buffer;
      }

      // Reset the buffer size and enable the channel
      DMA_1_CNDTR1 = PD_BUFFER_SIZE;
      DMA_1_CCR1 |= BIT_0;
      break;

    case PDPort::two:
      // Setup the DMA on the other buffer while we process this one
      dma_payload_size = PD_BUFFER_SIZE - DMA_1_CNDTR2;
      DMA_1_CCR2 &= ~(BIT_0);

      if(DMA_1_CMAR2 == (uint32_t)_rx_buff_a.buffer) {
        DMA_1_CMAR2 = (uint32_t)_rx_buff_b.buffer;
      } else {
        buffer_a = false;
        DMA_1_CMAR2 = (uint32_t)_rx_buff_a.buffer;
      }

      // Reset the buffer size and enable the channel
      DMA_1_CNDTR2 = PD_BUFFER_SIZE;
      DMA_1_CCR2 |= BIT_0;
      break;

    default:
      break;
  }

  // Get the payload size and check it against the DMA
  uint32_t payload_size = REGISTER(_base_addr + PD_RX_PAYSZ_OFFSET);
  if(payload_size == dma_payload_size) {
    if(buffer_a) {
      _rx_buff_a.size = payload_size;
      if(_rx_message_buff.can_push()) {
        _rx_message_buff.push(_rx_buff_a);
      }
    } else {
      _rx_buff_b.size = payload_size;
      if(_rx_message_buff.can_push()) {
        _rx_message_buff.push(_rx_buff_b);
      }
    }
    return;
  }

  rtt_printf("PD Pyld Sz Err - %d != %d", payload_size, dma_payload_size);
}

void STMPD::handle_hard_reset() {
  _message_id_counter = 0;
  if(_delegate) {
    _delegate->reset_received(*this);
  }
  _source_caps = SourceCapabilities();
  rtt_printf("HRST RX");
}

void STMPD::handle_type_c_event() {
  // Check if one of the phys has a voltage on it
  uint32_t pd_status = REGISTER(_base_addr + PD_SR_OFFSET);
  if(((pd_status & 0x00030000) >> 16) > 0) {
    // CC1 active, set the phy to use CC1
    REGISTER(_base_addr + PD_CR_OFFSET) &= ~(BIT_6);

    // Enable RX
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_5;

    _caps_rx_timer = system_time();
  } else if (((pd_status & 0x000C0000) >> 18) > 0) {
    // CC2 active, set the phy to use CC2
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_6;

    // Enable RX
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_5;

    _caps_rx_timer = system_time();
  } else {
    // No CC Active, disable RX
    REGISTER(_base_addr + PD_CR_OFFSET) &= ~(BIT_5);
  }

  REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_14 | BIT_15;

  // If neither CC is active clear out any capabilies
  _source_caps = SourceCapabilities();
}

void STMPD::handle_rx_buffer(const uint8_t* buffer, uint32_t size) {
  MessageHeader* msg_header = (MessageHeader*)buffer;

  if(msg_header->num_data_obj == 0) {
    // Control message received
    ControlMessageType message_type = (ControlMessageType)(msg_header->message_type);
    switch(message_type) {
      case ControlMessageType::good_crc:
        _caps_rx_timer = 0;
        _hard_reset_timer = 0;
        break;
      case ControlMessageType::goto_min:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        if(_delegate) {
          _delegate->go_to_min_received(*this);
        }
        break;
      case ControlMessageType::accept:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        if(_delegate) {
          _delegate->accept_received(*this);
        }
        break;
      case ControlMessageType::reject:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        if(_delegate) {
          _delegate->reject_received(*this);
        }
        break;
      case ControlMessageType::ping:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        break;
      case ControlMessageType::ps_rdy:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        if(_delegate) {
          _delegate->ps_ready_received(*this);
        }
        break;
      case ControlMessageType::soft_reset:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        _message_id_counter = 0;
        send_control_msg(ControlMessageType::accept);
        _source_caps = SourceCapabilities();
        if(_delegate) {
          _delegate->reset_received(*this);
        }
        break;
      case ControlMessageType::get_sink_cap:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        send_control_msg(ControlMessageType::reject);
      default:
        rtt_printf("Unhan ctl msg: %d", msg_header->message_type);
        break;
    }
  } else {
    // Data message received
    DataMessageType message_type = (DataMessageType)(msg_header->message_type);
    switch(message_type) {
      case DataMessageType::source_capabilities:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        _caps_rx_timer = 0;
        _hard_reset_timer = 0;
        handle_src_caps_msg(buffer, size);
        break;
      case DataMessageType::bist:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        break;
      case DataMessageType::sink_capabilities:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
        break;
      case DataMessageType::vendor_defined:
        send_control_msg(ControlMessageType::good_crc, msg_header->message_id);
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

  if(_delegate) {
    _delegate->capabilities_received(*this, _source_caps);
  }
}

void STMPD::enable_ints() {
  // Enable interrupts for Type C Events on CC1 and 2, RX Message End and RX hard reset
  REGISTER(_base_addr + PD_IMR_OFFSET) |= (BIT_15 | BIT_14 | BIT_12 | BIT_10 | BIT_2);
}

void STMPD::disable_ints() {
  REGISTER(_base_addr + PD_IMR_OFFSET) &= ~(BIT_15 | BIT_14 | BIT_12 | BIT_10 | BIT_2);
}

void STMPD::start_tx_dma() {
  // Check if there are any messages to send
  if(!_tx_message_buff.can_pop() || _tx_dma_inflight) {
    return;
  }

  _tx_dma_inflight = true;

  // Get the next message to send but don't pop it until it gets sent
  auto& message = _tx_message_buff.peek();

  // Depend on port setup the DMA for TX
  // Port 1 -> Chan 3
  // Port 2 -> Chan 4
  switch(_port) {
    case PDPort::one:
      // Configure the DMA channel
      DMA_1_CCR3 &= ~(0x00007FFF);
      DMA_1_CCR3 |=  (0x3 << 12) | BIT_7 | BIT_4;
      DMA_1_CNDTR3 &= ~(0x0000FFFF);
      DMA_1_CNDTR3 |=   message.size;
      DMA_1_CPAR3 = _base_addr + PD_TXDR_OFFSET;
      DMA_1_CMAR3 = (uint32_t)message.buffer;
      DMA_1_CCR3 |= BIT_0;
      break;

    case PDPort::two:
      // Configure the DMA channel
      DMA_1_CCR4 &= ~(0x00007FFF);
      DMA_1_CCR4 |=  (0x3 << 12) | BIT_7 | BIT_4;
      DMA_1_CNDTR4 &= ~(0x0000FFFF);
      DMA_1_CNDTR4 |=   message.size;
      DMA_1_CPAR4 = _base_addr + PD_TXDR_OFFSET;
      DMA_1_CMAR4 = (uint32_t)message.buffer;
      DMA_1_CCR4 |= BIT_0;
      break;

    default:
      break;
  }

  // Setup the PD Side
  REGISTER(_base_addr + PD_TX_ORDSET_OFFSET) |= ORDSET_SOP;
  REGISTER(_base_addr + PD_TX_PAYSZ_OFFSET) |= message.size;

  // Trigger the send on the PD side
  REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_2;

  // Check if the TX was discarded
  while(REGISTER(_base_addr + PD_SR_OFFSET) & BIT_1) {
    rtt_printf("TX Discarded");
    REGISTER(_base_addr + PD_ICR_OFFSET) |= BIT_2;
    REGISTER(_base_addr + PD_CR_OFFSET) |= BIT_2;
  }
}

