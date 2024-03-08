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

  std::optional<KeyState> read_virtual_key_state(std::optional<Duration> timeout);

private:
  Host m_host;
  Connection m_connection;
};
