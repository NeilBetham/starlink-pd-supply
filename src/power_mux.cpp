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
      _port_a_requested = false;
      _port_a_selected_cap = SourceCapability();
      break;
    case ControllerIndex::b:
      _port_b_accepted = false;
      _port_b_requested = false;
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
      _port_a_requested = false;
      break;
    case ControllerIndex::b:
      _port_b_ps_rdy = true;
      _port_b_requested = false;
      break;
    default:
      break;
  }

  // Check power caps
  check_available_power();

  // Check if we can enable the output
  check_if_output_is_ready();
}

void PowerMux::reset_received(IController& controller) {
  status_light::set_color(1, 0, 0);
  reset(controller);
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
  rtt_printf("Cntrl discon");
}


// Data events
void PowerMux::capabilities_received(IController& controller, const SourceCapabilities& caps) {
  // Some sources with multiple ports will renegotiate after another port is connected so check to
  // make sure we still have enough poweR
  if((_port_a_accepted || _port_a_ps_rdy) && get_controller(controller) == ControllerIndex::a) {
    rtt_printf("Src reset");
    controller_disconnected(controller);
  } else if((_port_b_accepted || _port_b_ps_rdy) && get_controller(controller) == ControllerIndex::b) {
    rtt_printf("Src reset");
    controller_disconnected(controller);
  }

  // Check the capabilities
  check_available_power();
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
    const auto& cap  = _control_a.caps().caps()[index];
    rtt_printf("Port A - %d - %dmV - %dmA", cap.index(), cap.voltage(), cap.current());
  }

  for(uint8_t index = 0; index < port_b_cap_count; index++) {
    port_b_max_powers[index] = _control_b.caps().caps()[index].max_power();
    const auto& cap = _control_b.caps().caps()[index];
    rtt_printf("Port B - %d - %dmV - %dmA", cap.index(), cap.voltage(), cap.current());
  }

  // General strategy here is to negotiate the highest power available on port A
  // then negotiate the highest power available on port B. This needs to be done
  // in series so that the caps on port B can be updated after port A has
  // negotiated its contract.

  uint8_t port_a_max_power_cap = 0;
  uint32_t port_a_max_power = 0;
  uint8_t port_b_max_power_cap = 0;
  uint32_t port_b_max_power = 0;

  // Port A Inflight check
  if(_port_a_requested) {
    return;
  }

  // Request the highest power on port A
  if(!_port_a_ps_rdy && _control_a.caps().count() > 0) {
    for(uint8_t index = 0; index < port_a_cap_count; index++) {
      if(port_a_max_powers[index] >= port_a_max_power) {
        port_a_max_power = port_a_max_powers[index];
        port_a_max_power_cap = index;
      }
    }

    _port_a_selected_cap = _control_a.caps().caps()[port_a_max_power_cap];
    _port_a_accepted = false;
    _port_a_ps_rdy = false;
    _port_a_requested = true;
    rtt_printf("Port A Request - Cap %d -> %dmV @ %dmA", _port_a_selected_cap.index(), _port_a_selected_cap.voltage(), _port_a_selected_cap.current());
    _control_a.request_capability(_port_a_selected_cap);
    return;
  }

  // Port B Inflight check
  if(_port_b_requested) {
    return;
  }

  // Request the highest power on port B
  if(!_port_b_ps_rdy && _control_b.caps().count() > 0) {
    for(uint8_t index = 0; index < port_b_cap_count; index++) {
      if(port_b_max_powers[index] >= port_b_max_power) {
        port_b_max_power = port_b_max_powers[index];
        port_b_max_power_cap = index;
      }
    }
    _port_b_selected_cap = _control_b.caps().caps()[port_b_max_power_cap];
    _port_b_accepted = false;
    _port_b_ps_rdy = false;
    _port_b_requested = true;
    rtt_printf("Port B Request - Cap %d -> %dmV @ %dmA", _port_b_selected_cap.index(), _port_b_selected_cap.voltage(), _port_b_selected_cap.current());
    _control_b.request_capability(_port_b_selected_cap);
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
  if(active_supplies() == 1) {
    rtt_printf("1 active sup");
    if(_port_a_accepted && _port_a_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      rtt_printf("A acc & rdy");
      _switch_a.set_current(_port_a_selected_cap.current());
      _switch_a.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    }
    if(_port_b_accepted && _port_b_ps_rdy && total_available_power() >= REQUIRED_OUTPUT_POWER_MW) {
      rtt_printf("B acc & rdy");
      _switch_b.set_current(_port_b_selected_cap.current());
      _switch_b.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    }
  } else if(active_supplies() == 2) {
    rtt_printf("2 active sup");
    if(_port_a_accepted &&
       _port_b_accepted &&
       _port_a_ps_rdy &&
       _port_b_ps_rdy &&
       total_available_power() > REQUIRED_OUTPUT_POWER_MW &&
       _port_a_selected_cap.voltage() == _port_b_selected_cap.voltage()) {
      _switch_a.set_current(_port_a_selected_cap.current());
      _switch_a.set_enabled(true);
      _switch_b.set_current(_port_b_selected_cap.current());
      _switch_b.set_enabled(true);
      _dishy_power.enable_power();
      status_light::set_color(0, 1, 0);
      return;
    } else  {
      rtt_printf("Two incompat sups");
    }
  }
  _switch_a.set_enabled(false);
  _switch_b.set_enabled(false);
  _dishy_power.disable_power();
  status_light::set_color(1, 1, 0);
}

void PowerMux::reset(IController& controller) {
  switch(get_controller(controller)) {
    case ControllerIndex::a:
      _switch_a.set_enabled(false);
      _port_a_selected_cap = SourceCapability();
      _port_a_accepted = false;
      _port_a_ps_rdy = false;
      _port_a_requested = false;
      break;

    case ControllerIndex::b:
      _switch_b.set_enabled(false);
      _dishy_power.disable_power();
      _port_b_selected_cap = SourceCapability();
      _port_b_accepted = false;
      _port_b_ps_rdy = false;
      _port_b_requested = false;
      break;

    default:
      break;
  }
}

uint8_t PowerMux::active_supplies() {
  uint8_t has_caps = 0;
  if(_control_a.caps().count() > 0) { has_caps++; }
  if(_control_b.caps().count() > 0) { has_caps++; }
  return has_caps;
}

