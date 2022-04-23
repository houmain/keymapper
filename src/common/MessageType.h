#pragma once

#include <cstdint>

enum class MessageType : uint8_t {
  configuration = 1,
  active_contexts,
  validate_state,
};
