/**
 * @brief sleep and time keeping
 */

#pragma once

#include <stdint.h>

// Init the systick registers
void systick_init();

// Sleep
void sleep(uint32_t seconds);
void msleep(uint32_t milli_seconds);

// System time
uint32_t system_time();
