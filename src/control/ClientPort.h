#pragma once

#include "runtime/KeyEvent.h"
#include "common/Host.h"
#include "common/MessageType.h"
#include <memory>

class ClientPort {
public:
  ClientPort();
  bool connect(std::optional<Duration> timeout);
  bool send_get_virtual_key_state(std::string_view name);
  bool send_set_virtual_key_state(std::string_view name, KeyState state);
  bool send_request_virtual_key_toggle_notification(std::string_view name);
  bool send_set_instance_id(std::string_view id);
  bool send_set_config_file(const std::string& filename);
  bool read_virtual_key_state(std::optional<Duration> timeout, 
    std::optional<KeyState>* result);

private:
  Host m_host;
  Connection m_connection;
};
