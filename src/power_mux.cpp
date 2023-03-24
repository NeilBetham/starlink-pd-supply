#include "power_mux.h"

#include "output_en.h"
#include "status_light.h"
#include "usb_pd_controller.h"
#include "rtt.h"

#define MAX_SUPPLY_CAPABILITIES 8

// Control Events
void PowerMux::go_to_min_received(IController& controller) {
  status_light::set_color(1, 1, 0);
  _dishy_power.disable_power();
}

void PowerMux::accept_received(IController& controller) {
  switch(get_controller(controller)) {
    case ControllerIndex::a:
      _port_a_accepted = true;
      break;
    case ControllerIndex::b:
      _port_b_accepted = true;
      break;
    default:
      break;
  }
}

void PowerMux::reject_received(IController& controller) {
  switch(get_controller(controller)) {
     case ControllerIndex::a:
      _port_a_accepted = false;
      _port_a_selected_cap = SourceCapability();
      break;
    case ControllerIndex::b:
      _port_b_accepted = false;
      _port_b_selected_cap = SourceCapability();
      break;
    default:
      break;
  }
}

void PowerMux::ps_ready_received(IController& controller) {
  switch(get_controller(controller)) {
     case ControllerIndex::a:
      _port_a_ps_rdy = true;
      break;
    case ControllerIndex::b:
      _port_b_ps_rdy = true;
      break;
    default:
      break;
  }

  // Check if we can enable the output
  check_if_output_is_ready();
}

void PowerMux::reset_received(IController& controller) {
  status_light::set_color(1, 0, 0);
  reset();
}

void PowerMux::controller_disconnected(IController& controller) {
  _dishy_power.disable_power();
  switch(get_controller(controller)) {
    case ControllerIndex::a:
      _port_a_accepted = false;
      _port_a_ps_rdy = false;
      _port_a_selected_cap = SourceCapability();
      _switch_a.set_enabled(false);
      break;
    case ControllerIndex::b:
      _port_b_accepted = false;
      _port_b_ps_rdy = false;
      _port_b_selected_cap = SourceCapability();
      _switch_b.set_enabled(false);
      break;
    default:
      break;
  }

  check_if_output_is_ready();
  rtt_print("Cntrl discon\r\n");
}


// Data events
void PowerMux::capabilities_received(IController& controller, const SourceCapabilities& caps) {
  // Some sources with multiple ports will renegotiate after another port is connected so check to
  // make sure we still have enough poweR
  if((_port_a_accepted || _port_a_ps_rdy) && get_controller(controller) == ControllerIndex::a) {
    rtt_print("Src reset\r\n");
    controller_disconnected(controller);
  } else if((_port_b_accepted || _port_b_ps_rdy) && get_controller(controller) == ControllerIndex::b) {
    rtt_print("Src reset\r\n");
    controller_disconnected(controller);
  }

  // Check the capabilities
  check_available_power();

  // Re check if we can sustain the output power
  check_if_output_is_ready();
}

ControllerIndex PowerMux::get_controller(IController& controller) {
  if(&_control_a == &controller) {
    return ControllerIndex::a;
  }

  if(&_control_b == &controller) {
    return ControllerIndex::b;
  }

  return ControllerIndex::unknown;
}

SourceCapability& PowerMux::get_controller_cap(IController& controller) {
  switch(get_controller(controller)) {
    case ControllerIndex::a:
      return _port_a_selected_cap;
    case ControllerIndex::b:
      return _port_b_selected_cap;
    default:
      break;
  }
  return _unknown_selected_cap;
}

void PowerMux::check_available_power() {
  // Check the available capabilities for each supply
  uint32_t port_a_max_powers[MAX_SUPPLY_CAPABILITIES] = {0};
  uint32_t port_b_max_powers[MAX_SUPPLY_CAPABILITIES] = {0};
  uint8_t port_a_cap_count = _control_a.caps().count();
  uint8_t port_b_cap_count = _control_b.caps().count();

  for(uint8_t index = 0; index < port_a_cap_count; index++) {
    port_a_max_powers[index] = _control_a.caps().caps()[index].max_power();
  }

  for(uint8_t index = 0; index < port_b_cap_count; index++) {
    port_b_max_powers[index] = _control_b.caps().caps()[index].max_power();
  }

  // Next find a power level that meets or exceeds our power reuqirement
  if(active_supplies() == 1) {
    rtt_print("One active sup\r\n");
    // Check Port A caps
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= REQUIRED_OUTPUT_POWER_MW) {
        // This cap will work for what we need request it
        _control_a.request_capability(_control_a.caps().caps()[index]);
        _port_a_selected_cap = _control_a.caps().caps()[index];
        return;
      }
    }

    // Check if we selected a capability, if not request the min power level
    if(_port_a_selected_cap.max_power() < 1) {
      _port_a_selected_cap = _control_a.caps().caps()[0];
      _control_a.request_capability(_port_a_selected_cap);
      rtt_print("A: 1 Sup, not enough power\r\n");
    }

    // Check Port B caps
    for(uint8_t index = 0; index < port_b_cap_count; index++) {
      if(port_b_max_powers[index] >= REQUIRED_OUTPUT_POWER_MW) {
        // This cap will work for what we need request it
        _control_b.request_capability(_control_b.caps().caps()[index]);
        _port_b_selected_cap = _control_b.caps().caps()[index];
        return;
      }
    }

    // Check if we selected a capability, if not request the min power level
    if(_port_b_selected_cap.max_power() < 1) {
      _port_b_selected_cap = _control_b.caps().caps()[0];
      _control_b.request_capability(_port_b_selected_cap);
      rtt_print("B: 1 Sup, not enough power\r\n");
    }
  } else if(active_supplies() == 2) {
    rtt_print("Two active sup\r\n");
    // We have two supplies so try to load balance between the two
    uint32_t target_power = REQUIRED_OUTPUT_POWER_MW / 2;
    uint8_t port_a_max_power_cap = 0;
    uint32_t port_a_max_power = 0;
    uint8_t port_b_max_power_cap = 0;
    uint32_t port_b_max_power = 0;

    // Check Port A for the target power level
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= port_a_max_power) {
        port_a_max_power = port_a_max_powers[index];
        port_a_max_power_cap = index;
      }
    }

    // Check Port B for the targt power level
    for(uint8_t index = 0; index < port_b_cap_count; index++) {
      if(port_b_max_powers[index] >= port_b_max_power) {
        port_b_max_power = port_b_max_powers[index];
        port_b_max_power_cap = index;
      }
    }

    // Check if the potential caps have the same voltage level
    const SourceCapability& port_a_cap = _control_a.caps().caps()[port_a_max_power_cap];
    const SourceCapability& port_b_cap = _control_b.caps().caps()[port_b_max_power_cap];
    if(port_a_cap.voltage() != port_b_cap.voltage()) {
      rtt_print("Potents V!=\r\n");
      return;
    }

    // Next up request the voltage level from both supplies
    rtt_print("Req load balance caps\r\n");
    _port_a_selected_cap = port_a_cap;
    _port_b_selected_cap = port_b_cap;

    _control_a.request_capability(_port_a_selected_cap, target_power);
    _control_b.request_capability(_port_b_selected_cap, target_power);
    _port_a_accepted = false;
    _port_b_accepted = false;
    return;
  }

  rtt_print("No active sup\r\n");
}

uint32_t PowerMux::total_available_power() {
  uint32_t power = 0;
  power += _port_a_selected_cap.max_power();
  power += _port_b_selected_cap.max_power();
  return power;
}

void PowerMux::check_if_output_is_ready() {
  // Check that we have enough power and the supplies have said we can draw power
  if(active_supplies() == 1) {
    rtt_print("Check output, one active sup\r\n");
    if(_port_a_accepted && _port_a_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      rtt_print("Port A accept and ready\r\n");
      _switch_a.set_current(_port_a_selected_cap.current());
      _switch_a.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    }
    if(_port_b_accepted && _port_b_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      rtt_print("Port b accept and ready\r\n");
      _switch_b.set_current(_port_b_selected_cap.current());
      _switch_b.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    }
  } else if(active_supplies() == 2) {
    rtt_print("Check output, two active sup\r\n");
    if(_port_a_accepted && _port_b_accepted && _port_a_ps_rdy && _port_b_ps_rdy && total_available_power() > REQUIRED_OUTPUT_POWER_MW) {
      _switch_a.set_current(_port_a_selected_cap.current());
      _switch_a.set_enabled(true);
      _switch_b.set_current(_port_b_selected_cap.current());
      _switch_b.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    }
  }
  _switch_a.set_enabled(false);
  _switch_b.set_enabled(false);
  _dishy_power.disable_power();
  status_light::set_color(1, 1, 0);
}

void PowerMux::reset() {
  _switch_a.set_enabled(false);
  _switch_b.set_enabled(false);
  _dishy_power.disable_power();
  _port_a_selected_cap = SourceCapability();
  _port_b_selected_cap = SourceCapability();
  _port_a_accepted = false;
  _port_b_accepted = false;
  _port_a_ps_rdy = false;
  _port_b_ps_rdy = false;
}

uint8_t PowerMux::active_supplies() {
  uint8_t has_caps = 0;
  if(_control_a.caps().count() > 0) { has_caps++; }
  if(_control_b.caps().count() > 0) { has_caps++; }
  return has_caps;
}

