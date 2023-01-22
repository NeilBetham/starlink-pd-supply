#include <stddef.h>

#include "utils.h"

// Hacks to get code size down
extern "C" {

void __cxa_pure_virtual(void) {
  while(1);
}

void __register_exitproc(void) {
  while(1);
}

int __wrap_atexit(void __attribute__((unused)) (*function)(void)) {
  return -1;
}

void __wrap_free(void* __attribute__((unused)) ptr) {
  while(1);
}

void* __wrap_memcpy(void* dst, const void* src, size_t count) {
  return cpymem(dst, src, count);
}

void* __wrap_memset(void* dst, int value, size_t count) {
  return setmem(dst, value, count);
}

void __wrap_exit(int __attribute__((unused)) status) {
  while(1);
}


}
