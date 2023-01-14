/**
 * @brief Manages the board status light
 */

#include <stdint.h>

#pragma once


namespace status_light {

void init();
void set_color(uint32_t red, uint32_t green, uint32_t blue);


} // namespace status_light
