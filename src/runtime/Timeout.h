#pragma once

#include <chrono>
#include <cstdint>
#include <algorithm>
#include "KeyEvent.h"
#include "config/string_iteration.h"

template<typename R, typename P>
KeyEvent::value_t duration_to_timeout(const std::chrono::duration<R, P>& duration) {
  const auto milliseconds = 
    std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  const auto max = (1 << KeyEvent::value_bits) - 1;
  return static_cast<KeyEvent::value_t>(std::clamp(
    static_cast<int>(milliseconds.count()),
    0, max));
}

inline std::chrono::milliseconds timeout_to_milliseconds(KeyEvent::value_t timeout) {
  return std::chrono::milliseconds(timeout);
}

template<typename R, typename P>
KeyEvent make_input_timeout_event(const std::chrono::duration<R, P>& duration) {
  return KeyEvent(Key::timeout, KeyState::Up, duration_to_timeout(duration));
}

inline KeyEvent::value_t sum_timeouts(KeyEvent::value_t a, KeyEvent::value_t b) {
  const auto max = (1 << KeyEvent::value_bits) - 1;
  return static_cast<KeyEvent::value_t>(std::min(
    static_cast<int>(a) + static_cast<int>(b), max));
}

template<typename ForwardIt>
std::optional<KeyEvent::value_t> try_read_timeout(ForwardIt* it, ForwardIt end) {
  const auto begin = *it;
  if (const auto number = try_read_number(it, end))
    if (skip(it, end, "ms"))
      return duration_to_timeout(std::chrono::milliseconds(*number));
  *it = begin;
  return std::nullopt;
}
