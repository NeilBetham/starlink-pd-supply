#include "rtt.h"

#include <string.h>
#include <stdio.h>

#include "time.h"


namespace {


uint32_t next_index(uint32_t offset, uint32_t count, uint32_t size) {
  return (offset + count) % size;
}

uint32_t space_available(uint32_t write_offset, uint32_t read_offset, uint32_t size) {
  if(write_offset < read_offset) {
    return read_offset - write_offset - 1u;
  } else {
    return size - (write_offset - read_offset + 1u);
  }
}

inline uint32_t min(uint32_t a, uint32_t b) {
  if(a > b) {
    return b;
  } else {
    return a;
  }
}


} // namespace


RTT::RTT() {
  memset(&_control_buffer, 0, sizeof(_control_buffer));

  _control_buffer.max_number_up_buffers = MAX_UP_BUFFERS;
  _control_buffer.max_number_down_buffers = MAX_DOWN_BUFFERS;

  RTT_BUFFER_UP* up_buffer = &_control_buffer.up_buffers[0];
  memset(up_buffer, 0, sizeof(RTT_BUFFER_UP));
  up_buffer->name = "Terminal";
  up_buffer->buffer = &_main_buffer[0];
  up_buffer->buffer_size = CHANNEL_BUFFER_SIZE;

  RTT_BUFFER_DOWN* down_buffer = &_control_buffer.down_buffers[0];
  memset(down_buffer, 0, sizeof(RTT_BUFFER_UP));
  down_buffer->name = "Terminal";
  down_buffer->buffer = &_main_buffer[CHANNEL_BUFFER_SIZE];
  down_buffer->buffer_size = CHANNEL_BUFFER_SIZE;

  static const char sentinel[] = "\0\0\0\0\0\0\0\0EREH TTR";
  for(uint32_t index = 0; index < 16; index++){
    _control_buffer.buffer_id[index] = sentinel[15 - index];
  }
}

RTT::~RTT() {
}


int RTT::printf(const char* format, ...) {
  va_list args;
  int retval = 0;
  va_start(args, format);
  retval = vprintf(format, args);
  va_end(args);
  return retval;
}

int RTT::vprintf(const char* format, va_list args) {
  char buffer[258] = {0};
  int buffer_pos = 0;

  buffer_pos += snprintf(&buffer[buffer_pos], sizeof(buffer) - buffer_pos - 2, "[%u] ", system_time());
  buffer_pos += vsnprintf(&buffer[buffer_pos], sizeof(buffer) - buffer_pos - 2, format, args);

  buffer[buffer_pos] = '\r';
  buffer[buffer_pos + 1] = '\n';

  write(&buffer[0], buffer_pos + 2);
  return buffer_pos;
}

void RTT::write(const void* buffer, uint32_t count) {
  write(0, buffer, count);
}

uint32_t RTT::write_space() {
  return write_space(0);
}

void RTT::read(void* buffer, uint32_t count) {
  read(0, buffer, count);
}

uint32_t RTT::read_available() {
  return read_available(0);
}

void RTT::write(uint32_t out_buffer, const void* buffer, uint32_t count) {
  RTT_BUFFER_UP* write_cb = &_control_buffer.up_buffers[out_buffer];
  uint32_t read_offset = write_cb->read_offset;
  uint32_t write_offset = write_cb->write_offset;
  uint32_t bytes_written = 0;

  if(space_available(write_offset, read_offset, write_cb->buffer_size) < 1) {
    // The buffer is full so the debugger is likely not attached
    return;
  }

  while(bytes_written < count) {
    // First check to see if the debugger has read any bytes
    read_offset =- write_cb->read_offset;

    // Figure out how many bytes we can write
    uint32_t bytes_till_wrap = write_cb->buffer_size - write_offset;
    uint32_t space_to_write = space_available(write_offset, read_offset, write_cb->buffer_size);
    uint32_t bytes_to_write = min(bytes_till_wrap, space_to_write);
    uint32_t bytes_to_copy = min(bytes_to_write, count);

    // Next copy the bytes to the buffer
    uint8_t* buffer_dest = &write_cb->buffer[write_offset];
    const uint8_t* buffer_src = &((uint8_t*)buffer)[bytes_written];
    memcpy(buffer_dest, buffer_src, bytes_to_copy);
    bytes_written += bytes_to_copy;
    write_offset += bytes_to_copy;

    // Check if we need to wrap around
    if(write_offset == write_cb->buffer_size) {
      write_offset = 0;
    }

    // Update the write offset in the control buffer
    write_cb->write_offset = write_offset;
  }
}

uint32_t RTT::write_space(uint32_t out_buffer) {
  RTT_BUFFER_UP* write_cb = &_control_buffer.up_buffers[out_buffer];
  return space_available(write_cb->write_offset, write_cb->read_offset, write_cb->buffer_size);
}

uint32_t RTT::read(uint32_t in_buffer, void* buffer, uint32_t count) {
  // I only need output so skipping this for now
  return 0;
}

uint32_t RTT::read_available(uint32_t in_buffer) {
  // I only need output so skipping this for now
  return 0;
}


RTT& rtt() {
  static RTT rtt_comms;
  return rtt_comms;
}

int rtt_printf(const char* format, ...) {
  va_list args;
  int retval = 0;
  va_start(args, format);
  retval = rtt().vprintf(format, args);
  va_end(args);
  return retval;
}
