/**
 * @brief Circular buffer implementation
 */
#pragma once

#include <array>
#include <stdint.h>

// TODO: Make concurrency safe

template <typename T, uint8_t BUFFER_SIZE>
class CircularBuffer {
public:
  uint8_t size() {
    if(_head >= _tail) {
      return _head - _tail;
    } else {
      return ((BUFFER_SIZE - _tail) + _head);
    }
  }

  uint8_t capacity() { return BUFFER_SIZE - 1; }

  bool can_push() { return size() < capacity(); }
  bool can_pop() { return size() > 0; }

  T pop() {
    T element = _buffer[_tail];
    if(_tail + 1 >= BUFFER_SIZE){
      _tail = 0;
    } else {
      _tail++;
    }
    return element;
  }

  void push(const T& element) {
    _buffer[_head] = element;
    if(_head + 1 >= BUFFER_SIZE) {
      _head = 0;
    } else {
      _head++;
    }
  }

private:
  uint8_t _head = 0;
  uint8_t _tail = 0;
  std::array<T, BUFFER_SIZE> _buffer;
};
