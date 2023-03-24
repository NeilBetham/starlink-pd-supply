/**
 * @brief Handles the enabling of power to dishy
 */

#include <stdint.h>

#pragma once


enum class LoadMode {
  unknown = 0,
  disabled,
  sense_voltage,
  load_power
};


class DishyPower {
public:
  void init();
  void tick();

  void enable_power();
  void disable_power();

private:
  bool _power_output_enabled = false;
  bool _dishy_connected = true;

  uint32_t _low_side_counts = 0;
  uint32_t _low_side_thresh_counts = 0;

  uint32_t _high_side_counts = 0;

  uint32_t _current_counts = 0;
  uint32_t _current_no_load_counts = 0;
  uint32_t _current_below_thresh_counts = 0;

  LoadMode _current_mode = LoadMode::unknown;

  void set_load_mode(LoadMode mode);
  void set_boost(bool enabled);

  void set_sense_mode();
  void set_load_power_mode();

  void run_adc_conversion();

  void monitor_current();
  void monitor_sense_voltage();
};
