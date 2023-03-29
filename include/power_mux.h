/**
 * @brief Handles the muxing of power from both USB C ports
 */

#pragma once

#include "dishy_power.h"
#include "pd_protocol.h"
#include "power_switch.h"

#define REQUIRED_OUTPUT_POWER_MW 93000


enum class ControllerIndex : uint8_t {
  unknown = 0,
  a,
  b
};


class PowerMux : public ControllerDelegate {
public:
  PowerMux(IController& controller_a, IController& controller_b, PowerSwitch& power_switch_a, PowerSwitch& power_switch_b, DishyPower& dishy_power) :
      _control_a(controller_a), _control_b(controller_b), _switch_a(power_switch_a), _switch_b(power_switch_b), _dishy_power(dishy_power) {
    _control_a.set_delegate(this);
    _control_b.set_delegate(this);
  };
  ~PowerMux() {};

  // ControllerDelegate Interface
  // Control Events
  void go_to_min_received(IController& controller);
  void accept_received(IController& controller);
  void reject_received(IController& controller);
  void ps_ready_received(IController& controller);
  void reset_received(IController& controller);
  void controller_disconnected(IController& controller);

  // Data events
  void capabilities_received(IController& controller, const SourceCapabilities& caps);

private:
  ControllerIndex get_controller(IController& controller);
  SourceCapability& get_controller_cap(IController& controller);

  // Check if we have enough power now to enable the output
  void check_available_power();

  // Based on selected power modes what is the current available power
  uint32_t total_available_power();

  // Check if the output can be enabled
  void check_if_output_is_ready();

  // Should be called when we need to renegotiate power
  void reset(IController& controller);

  // Called to check the number of supplies available
  uint8_t active_supplies();

	IController& _control_a;
	IController& _control_b;
  PowerSwitch& _switch_a;
  PowerSwitch& _switch_b;
  DishyPower& _dishy_power;

  SourceCapability _port_a_selected_cap;
  SourceCapability _port_b_selected_cap;
  SourceCapability _unknown_selected_cap;

  bool _port_a_requested = false;
  bool _port_b_requested = false;

  bool _port_a_accepted = false;
  bool _port_b_accepted = false;

  bool _port_a_ps_rdy = false;
  bool _port_b_ps_rdy = false;
};
