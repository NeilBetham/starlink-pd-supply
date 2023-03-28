/**
 * @brief Structs representing current state of pd system
 */

#pragma once


#include <stdint.h>

#include "registers/helpers.h"

#define MAX_CAPABILITIES 7


// Enum for different power data object types in a source caps message
enum class PowerDataObjectType : uint8_t {
  fixed = 0,
  battery = 1,
  variable = 2,
  reserved = 3
};

enum class ControlMessageType : uint8_t {
  reserved = 0,
  good_crc,
  goto_min,
  accept,
  reject,
  ping,
  ps_rdy,
  get_source_cap,
  get_sink_cap,
  dr_swap,
  pr_swap,
  vconn_swap,
  wait,
  soft_reset
};

enum class DataMessageType : uint8_t {
  reserved = 0,
  source_capabilities,
  request,
  bist,
  sink_capabilities,
  vendor_defined = 0xF
};

enum class SpecificationRev : uint8_t {
  one_v_zero = 0,
  two_v_zero,
  reserved
};

// PD Message Header
struct PACKED MessageHeader {
  uint8_t message_type: 4;
  uint8_t : 1;
  uint8_t port_data_role: 1;
  uint8_t spec_rev: 2;
  uint8_t power_port_role: 1;
  uint8_t message_id: 3;
  uint8_t num_data_obj: 3;
  uint8_t : 1;
};

// Source capability PDOs
struct PACKED FixedPowerDataObject {
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

struct PACKED BatteryPowerDataObject {
  uint16_t max_power_250mw: 10;
  uint16_t min_voltage_50mv: 10;
  uint16_t max_voltage_50mv: 10;
  uint16_t  supply_type: 2;
};

struct PACKED VariablePowerDataObject {
  uint16_t max_current_10ma: 10;
  uint16_t min_voltage_50mv: 10;
  uint16_t max_voltage_50mv: 10;
  uint16_t  supply_type: 2;
};

// Source request PDOs
struct PACKED FixedRequestPowerDataObject {
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

struct PACKED BatteryRequestPowerDataObject {
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

// Each PDO is 32 bits
struct PACKED PowerDataObject {
  uint32_t : 30;
  uint8_t supply_type : 2;
};

// Forward declarations
class Request;
class USBPDController;

// Classes representing different data messages from the source
class SourceCapability {
public:
  SourceCapability() {};
  SourceCapability(const PowerDataObject& object, uint8_t index);

  PowerDataObjectType type() const { return _pdo_type; };
  uint32_t voltage() const { return _voltage; };
  uint32_t max_power() const { return _power; };
  uint32_t current() const { return _current; }
  bool is_battery() const;
  uint8_t index() const { return _index; };

private:
  PowerDataObjectType _pdo_type = PowerDataObjectType::reserved;
  uint32_t _voltage = 0;
  uint32_t _current = 0;
  uint32_t _power = 0;
  uint8_t _index = 0;
};


class SourceCapabilities {
public:
  SourceCapabilities() {};
  SourceCapabilities(const PowerDataObject* objects, uint8_t object_count);
  const SourceCapability* caps() const { return _capabilities; };
  uint8_t count() const { return _capability_count; };

private:
  SourceCapability _capabilities[MAX_CAPABILITIES] = {};
  uint8_t _capability_count = 0;
};

// Classes representing data messages to the source
class Request {
public:
  Request(const SourceCapability& capability, uint32_t power);

  uint32_t generate_pdo();

private:
  PowerDataObjectType _pdo_type = PowerDataObjectType::reserved;
  uint32_t _voltage = 0;
  uint32_t _power = 0;
  uint32_t _pdo_index = 0;
};

class ControllerDelegate;

// Controller interface description
class IController {
public:
  virtual ~IController() {};

  virtual const SourceCapabilities& caps() = 0;

  virtual void send_control_msg(ControlMessageType message_type) = 0;
  virtual void send_hard_reset() = 0;

  virtual void request_capability(const SourceCapability& capability) = 0;
  virtual void request_capability(const SourceCapability& capability, uint32_t power) = 0;

  virtual void set_delegate(ControllerDelegate* delegate) = 0;
};

// Delegate ABC to model events received from a USB PD source
class ControllerDelegate {
public:
  virtual ~ControllerDelegate() {};

  // Control Events
  virtual void go_to_min_received(IController& controller) = 0;
  virtual void accept_received(IController& controller) = 0;
  virtual void reject_received(IController& controller) = 0;
  virtual void ps_ready_received(IController& controller) = 0;
  virtual void reset_received(IController& controller) = 0;
  virtual void controller_disconnected(IController& controller) = 0;

  // Data events
  virtual void capabilities_received(IController& controller, const SourceCapabilities& caps) = 0;

};
