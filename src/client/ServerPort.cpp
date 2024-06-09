
#include "ServerPort.h"
#include "common/MessageType.h"

namespace {
  void write_key_sequence(Serializer& s, const KeySequence& sequence) {
    s.write(static_cast<uint32_t>(sequence.size()));
    for (const auto& event : sequence)
      s.write(event);
  }
  
  void write_filter(Serializer& s, const Filter& filter) {
    s.write(filter.string);
    s.write(filter.invert);
  }

  void write_contexts(Serializer& s, 
      const std::vector<Config::Context>& contexts) {
    s.write(static_cast<uint32_t>(contexts.size()));
    for (const auto& context : contexts) {
      // begin stage
      s.write(context.begin_stage);

      // inputs
      s.write(static_cast<uint32_t>(context.inputs.size()));
      for (const auto& input : context.inputs) {
        write_key_sequence(s, input.input);
        s.write(static_cast<int32_t>(input.output_index));
      }

      // outputs
      s.write(static_cast<uint32_t>(context.outputs.size()));
      for (const auto& output : context.outputs)
        write_key_sequence(s, output);

      // command outputs
      s.write(static_cast<uint32_t>(context.command_outputs.size()));
      for (const auto& command : context.command_outputs) {
        write_key_sequence(s, command.output);
        s.write(static_cast<int32_t>(command.index));
      }

      // device filter
      write_filter(s, context.device_filter);
      
      // device-id filter
      write_filter(s, context.device_id_filter);
      
      // modifier filter
      write_key_sequence(s, context.modifier_filter);
      s.write(context.invert_modifier_filter);

      // fallthrough
      s.write(context.fallthrough);
    }
  }

  void write_grab_device_filters(Serializer& s, 
      const std::vector<GrabDeviceFilter>& device_filters) {
    s.write(static_cast<uint32_t>(device_filters.size()));
    for (const auto& device_filter : device_filters) {
      write_filter(s, device_filter);
      s.write(device_filter.by_id);
    }
  }

  void write_directives(Serializer& s,
      const std::vector<std::string>& directives) {
    s.write(static_cast<uint32_t>(directives.size()));
    for (const auto& directive : directives)
      s.write(directive);
  }

  void write_active_contexts(Serializer& s, const std::vector<int>& indices) {
    s.write(static_cast<uint32_t>(indices.size()));
    for (const auto& index : indices)
      s.write(static_cast<uint32_t>(index));
  }
} // namespace

ServerPort::ServerPort() 
  : m_host("keymapper") {
}

bool ServerPort::connect() {
  m_connection = m_host.connect();
  return static_cast<bool>(m_connection);
}

void ServerPort::disconnect() {
  m_connection.disconnect();
}

bool ServerPort::send_config(const Config& config) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::configuration);
    write_grab_device_filters(s, config.grab_device_filters);    
    write_contexts(s, config.contexts);
    write_directives(s, config.server_directives);
  });
}

bool ServerPort::send_active_contexts(const std::vector<int>& indices) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::active_contexts);
    write_active_contexts(s, indices);
  });
}

bool ServerPort::send_validate_state() {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::validate_state);
  });
}

bool ServerPort::send_set_virtual_key_state(Key key, KeyState state) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::set_virtual_key_state);
    s.write(key);
    s.write(state);
  });
}

bool ServerPort::send_request_next_key_info() {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::next_key_info);
  });
}

bool ServerPort::send_inject_input(const KeySequence& sequence) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::inject_input);
    write_key_sequence(s, sequence);
  });
}

bool ServerPort::send_inject_output(const KeySequence& sequence) {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::inject_output);
    write_key_sequence(s, sequence);
  });
}

bool ServerPort::read_messages(MessageHandler& handler,
    std::optional<Duration> timeout) {
  return m_connection.read_messages(timeout,
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::execute_action: {
          handler.on_execute_action_message(
            static_cast<int>(d.read<uint32_t>()));
          break;
        }
        case MessageType::virtual_key_state: {
          const auto key = d.read<Key>();
          const auto state = d.read<KeyState>();
          handler.on_virtual_key_state_message(key, state);
          break;
        }
        case MessageType::next_key_info: {
          const auto key = d.read<Key>();
          auto device_desc = DeviceDesc{ d.read_string(), d.read_string() };
          handler.on_next_key_info_message(key, std::move(device_desc));
          break;
        }
        default: break;
      }
    });
}
