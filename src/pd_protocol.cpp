#include "pd_protocol.h"

SourceCapability::SourceCapability(const PDObject object, uint8_t index) {
  _index = index;
  PowerDataObjectType pdo_type = (PowerDataObjectType)object.supply_type;

  switch(pdo_type) {
    case PowerDataObjectType::fixed:
      FixedPowerDataObject* fixed_pdo = (FixedPowerDataObject*)(&object);
      _pdo_type = pdo_type;
      _voltage = fixed_pdo->voltage_50mv * 50;
      _power = _voltage * fixed_pdo->max_current_10ma * 10;
      break;
    case PowerDataObjectType::battery:
      BatteryPowerDataObject* batt_pdo = (BatteryRequestPowerDataObject*)(&object);
      _pdo_type = pdo_type;
      uint32_t median_voltage = ((batt_pdo->max_voltage_50mv - batt_pdo->min_voltage_50mv) / 2) + batt_pdo->min_voltage_50mv;
      _voltage = median_voltage * 50;
      _power = batt_pdo->max_power_250mw * 250;
      break;
    case PowerDataObjectType::variable:
      VariablePowerDataObject* var_pdo = (VariablePowerDataObject*)(&object);
      _pdo_type = pdo_type;
      uint32_t median_voltage = ((var_pdo->max_voltage_50mv - var_pdo->min_voltage_50mv) / 2) + var_pdo->min_voltage_50mv;
      _voltage = median_voltage * 50;
      _power = var_pdo->max_power_250mw * 250;
      break;
    default:
      break;
  }
}

bool SourceCapability::is_battery() {
  return _pdo_type == battery;
}


SourceCapabilities::SourceCapabilities(const PDObject* objects, uint8_t object_count) {
  for(uint8_t index = 0; index < object_count; index++) {
    _capabilities[index] = SourceCapability(objects[index], index);
  }
}


Request::Request(const SourceCapability& capability, uint32_t power) {
  _pdo_type = capability.type();
  _voltage = capability.voltage();
  _power = power;
  _pdo_index = capability._index;
}

uint32_t Request::generate_pdo() {
  uint32_t ret_pdo = 0;
  switch(_pdo_type) {
    case PowerDataObjectType::fixed:
    case PowerDataObjectType::variable:
      FixedRequestPowerDataObject* pdo_req = (FixedRequestPowerDataObject*)(&ret_pdo);
      uint16_t current = (_power / _voltage) / 10;
      pdo_req->max_current_10ma = current;
      pdo_req->op_current_10ma = current;
      pdo_reg->object_pos = _pdo_index;
      break;
    case PowerDataObjectType::battery:
      BatteryRequestPowerDataObject* pdo_req = (BatteryRequestPowerDataObject*)(&ret_pod);
      pdo_req->max_power_250mw = _power / 250;
      pdo_req->op_power_250mw = pdo_req->max_power_250mw;
      pdo_reg->object_pos = _pdo_index;
      break;
    default:
      break;
  }

  return ret_pdo;
}
