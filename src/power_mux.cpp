#include "power_mux.h"

#include "output_en.h"
#include "status_light.h"
#include "usb_pd_controller.h"
#include "rtt.h"

#define MAX_SUPPLY_CAPABILITIES 8

// Control Events
void PowerMux::go_to_min_received(USBPDController& controller) {
  set_controller(controller);
  status_light::set_color(1, 1, 0);
  reset();
}

void PowerMux::accept_received(USBPDController& controller) {
  set_controller(controller);
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

void PowerMux::reject_received(USBPDController& controller) {
  set_controller(controller);
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

void PowerMux::ps_ready_received(USBPDController& controller) {
  set_controller(controller);
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

void PowerMux::reset_received(USBPDController& controller) {
  set_controller(controller);
  status_light::set_color(1, 0, 1);
  reset();
}


// Data events
void PowerMux::capabilities_received(USBPDController& controller, const SourceCapabilities& caps) {
  set_controller(controller);

  // First request the first PDO with it's max power to keep the supply from shutting us down
  get_controller_cap(controller) = caps.caps()[0];
  controller.request_capability(get_controller_cap(controller));

  // Check the capabilities
  check_available_power();

  rtt_print("Caps rx");
}

void PowerMux::set_controller(USBPDController& controller) {
  if(_control_a && _control_b) {
    return;
  }

  // This is bad, but i'm lazy and know that the controllers will not go out of scope
  if(_control_a == 0) {
    _control_a = &controller;
    _supply_count = 1;
  } else if(_control_b == 0) {
    _control_b = &controller;
    _supply_count = 2;
  }
}

ControllerIndex PowerMux::get_controller(USBPDController& controller) {
  if(_control_a == &controller) {
    return ControllerIndex::a;
  }

  if(_control_b == &controller) {
    return ControllerIndex::b;
  }

  return ControllerIndex::unknown;
}

SourceCapability& PowerMux::get_controller_cap(USBPDController& controller) {
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
  uint8_t port_a_cap_count = 0;
  uint8_t port_b_cap_count = 0;

  if(_control_a) {
    port_a_cap_count = _control_a->caps().count();
  }

  if(_control_b) {
    port_b_cap_count = _control_b->caps().count();
  }

  for(uint8_t index = 0; index < port_a_cap_count; index++) {
    port_a_max_powers[index] = _control_a->caps().caps()[index].max_power();
  }

  for(uint8_t index = 0; index < port_b_cap_count; index++) {
    port_b_max_powers[index] = _control_b->caps().caps()[index].max_power();
  }

  // Next find a power level that meets or exceeds our power reuqirement
  if(_supply_count == 1) {
    // Check Port A caps
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= REQUIRED_OUTPUT_POWER_MW) {
        // This cap will work for what we need request it
        _control_a->request_capability(_control_a->caps().caps()[index]);
        return;
      }
    }

    // Check Port B caps
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= REQUIRED_OUTPUT_POWER_MW) {
        // This cap will work for what we need request it
        _control_a->request_capability(_control_a->caps().caps()[index]);
        return;
      }
    }
  } else if(_supply_count == 2) {
    // We have two supplies so try to load balance between the two
    uint32_t target_power = REQUIRED_OUTPUT_POWER_MW / 2;
    uint8_t port_a_potential_cap = 0;
    uint8_t port_b_potential_cap = 0;

    // Check Port A for the target power level
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= target_power) {
        port_a_potential_cap = index;
      }
    }

    // Check Port B for the targt power level
    for(uint8_t index = 0; index < port_b_cap_count; index++) {
      if(port_b_max_powers[index] >= target_power) {
        port_b_potential_cap = index;
      }
    }

    // Check if the potential caps have the same voltage level
    const SourceCapability& port_a_cap = _control_a->caps().caps()[port_a_potential_cap];
    const SourceCapability& port_b_cap = _control_b->caps().caps()[port_b_potential_cap];
    if(port_a_cap.voltage() != port_b_cap.voltage()) {
      return;
    }

    // Next up request the voltage level from both supplies
    _port_a_selected_cap = port_a_cap;
    _port_b_selected_cap = port_b_cap;

    _control_a->request_capability(_port_a_selected_cap, target_power);
    _control_b->request_capability(_port_b_selected_cap, target_power);
    _port_a_accepted = false;
    _port_b_accepted = false;
  }
}

uint32_t PowerMux::total_available_power() {
  uint32_t power = 0;
  power += _port_a_selected_cap.max_power();
  power += _port_b_selected_cap.max_power();
  return power;
}

void PowerMux::check_if_output_is_ready() {
  // Check that we have enough power and the supplies have said we can draw power
  if(_supply_count == 1) {
    if(_port_a_accepted && _port_a_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      output_en::set_state(true);
      status_light::set_color(0, 1, 0);
    }
    if(_port_b_accepted && _port_b_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      output_en::set_state(true);
      status_light::set_color(0, 1, 0);
    }
  } else if(_supply_count == 2) {
    if(_port_a_accepted && _port_b_accepted && _port_a_ps_rdy && _port_b_ps_rdy && total_available_power() > REQUIRED_OUTPUT_POWER_MW) {
      output_en::set_state(true);
      status_light::set_color(0, 1, 0);
    }
  }
}

void PowerMux::reset() {
  output_en::set_state(false);
  _port_a_selected_cap = SourceCapability();
  _port_b_selected_cap = SourceCapability();
  _port_a_accepted = false;
  _port_b_accepted = false;
  _port_a_ps_rdy = false;
  _port_b_ps_rdy = false;
  _supply_count = 0;
}
