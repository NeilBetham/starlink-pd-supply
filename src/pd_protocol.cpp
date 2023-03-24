#include "pd_protocol.h"

#include "rtt.h"
#include "utils.h"

SourceCapability::SourceCapability(const PowerDataObject& object, uint8_t index) {
  _index = index;
  PowerDataObjectType pdo_type = (PowerDataObjectType)object.supply_type;

  switch(pdo_type) {
    case PowerDataObjectType::fixed: {
      FixedPowerDataObject* fixed_pdo = (FixedPowerDataObject*)(&object);
      _pdo_type = pdo_type;
      _voltage = fixed_pdo->voltage_50mv * 50;
      _current = fixed_pdo->max_current_10ma * 10;
      _power = (_voltage * fixed_pdo->max_current_10ma * 10) / 1000;
      break;
    }
    case PowerDataObjectType::battery: {
      BatteryPowerDataObject* batt_pdo = (BatteryPowerDataObject*)(&object);
      _pdo_type = pdo_type;
      uint32_t median_voltage = ((batt_pdo->max_voltage_50mv - batt_pdo->min_voltage_50mv) >> 1) + batt_pdo->min_voltage_50mv;
      _voltage = median_voltage * 50;
      _power = batt_pdo->max_power_250mw * 250;
      _current = (_power * 1000) / _voltage;
      break;
    }
    case PowerDataObjectType::variable: {
      VariablePowerDataObject* var_pdo = (VariablePowerDataObject*)(&object);
      _pdo_type = pdo_type;
      uint32_t median_voltage = ((var_pdo->max_voltage_50mv - var_pdo->min_voltage_50mv) >> 1) + var_pdo->min_voltage_50mv;
      _voltage = median_voltage * 50;
      _power = var_pdo->max_current_10ma * 10 * median_voltage;
      _current = (_power * 1000) / _voltage;
      break;
    }
    default:
      break;
  }
}

bool SourceCapability::is_battery() const {
  return _pdo_type == PowerDataObjectType::battery;
}


SourceCapabilities::SourceCapabilities(const PowerDataObject* objects, uint8_t object_count) {
  for(uint8_t index = 0; index < object_count; index++) {
    _capabilities[index] = SourceCapability(objects[index], index);
  }
  _capability_count = object_count;
}


Request::Request(const SourceCapability& capability, uint32_t power) {
  _pdo_type = capability.type();
  _voltage = capability.voltage();
  _power = power;
  _pdo_index = capability.index() + 1;
}

uint32_t Request::generate_pdo() {
  uint32_t ret_pdo = 0;

  switch(_pdo_type) {
    case PowerDataObjectType::fixed:
    case PowerDataObjectType::variable: {
      FixedRequestPowerDataObject* f_pdo_req = (FixedRequestPowerDataObject*)(&ret_pdo);
      uint16_t current = (_power / _voltage) / 10;
      f_pdo_req->max_current_10ma = current;
      f_pdo_req->op_current_10ma = current;
      f_pdo_req->object_pos = _pdo_index;
      break;
    }
    case PowerDataObjectType::battery: {
      BatteryRequestPowerDataObject* b_pdo_req = (BatteryRequestPowerDataObject*)(&ret_pdo);
      b_pdo_req->max_power_250mw = _power / 250;
      b_pdo_req->op_power_250mw = b_pdo_req->max_power_250mw;
      b_pdo_req->object_pos = _pdo_index;
      break;
    }
    default:
      break;
  }

  return ret_pdo;
}
