/**
 * @brief Re-implementation of system utils to keep code size small
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

void* setmem(void* dest, int value, size_t count);
void* cpymem(void* dest, const void* src, size_t count);

