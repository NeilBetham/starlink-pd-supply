#include "utils.h"

void* setmem(void* dest, int value, size_t count) {
  uint8_t* dest_bytes = (uint8_t*)dest;

  for(size_t index = 0; index < count; index++) {
    dest_bytes[index] = value;
  }

  return dest;
}
void* cpymem(void* dest, const void* src, size_t count) {
  uint8_t* dest_bytes = (uint8_t*)dest;
  const uint8_t* src_bytes = (const uint8_t*)src;

  for(size_t index = 0; index < count; index++) {
    dest_bytes[index] = src_bytes[index];
  }

  return dest;
}

