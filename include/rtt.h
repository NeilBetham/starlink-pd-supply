/**
 * @brief Minimal implementation of RTT
 */

#pragma once

#include <stdint.h>

#include "registers/helpers.h"

#define MAX_UP_BUFFERS 1
#define MAX_DOWN_BUFFERS 1
#define CHANNEL_BUFFER_SIZE 1024

struct PACKED RTT_BUFFER_UP {
  const char* name;
  uint8_t* buffer;
  uint32_t buffer_size;
  uint32_t write_offset;
  volatile uint32_t read_offset;
  uint32_t flags;
};


struct PACKED RTT_BUFFER_DOWN {
  const char* name;
  uint8_t* buffer;
  uint32_t buffer_size;
  volatile uint32_t write_offset;
  uint32_t read_offset;
  uint32_t flags;
};


struct PACKED RTT_CONTROL_BUFFER {
  char buffer_id[16];
  int32_t max_number_up_buffers;
  int32_t max_number_down_buffers;
  RTT_BUFFER_UP up_buffers[MAX_UP_BUFFERS];
  RTT_BUFFER_DOWN down_buffers[MAX_DOWN_BUFFERS];
};


class RTT {
public:
  RTT();
  ~RTT();

  // Default 0 out / in buffer
  int print(const char* message);
  void write(const void* buffer, uint32_t count);
  uint32_t write_space();
  void read(void* buffer, uint32_t count);
  uint32_t read_available();

  // Individual buffer writers / readers
  void write(uint32_t out_buffer, const void* buffer, uint32_t count);
  uint32_t write_space(uint32_t out_buffer);

  uint32_t read(uint32_t in_buffer, void* buffer, uint32_t count);
  uint32_t read_available(uint32_t in_buffer);

private:
  ALIGNED RTT_CONTROL_BUFFER _control_buffer;
  ALIGNED uint8_t _main_buffer[(MAX_UP_BUFFERS + MAX_DOWN_BUFFERS) * CHANNEL_BUFFER_SIZE] = {};

  uint32_t _buffer_full_count = 0;

};


// Simple singleton to get access to a single RTT comm manager
RTT& rtt();
int rtt_print(const char* message);
int rtt_printf(const char* format, ...);
