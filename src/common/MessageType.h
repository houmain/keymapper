#pragma once

#include <cstdint>

enum class MessageType : uint8_t {
  configuration = 1,
  active_contexts,
  validate_state,
  execute_action,
  virtual_key_state,
  get_virtual_key_state,
  set_virtual_key_state,
  request_virtual_key_toggle_notification,
  set_instance_id,
  set_config_file,
  device_names,
};
