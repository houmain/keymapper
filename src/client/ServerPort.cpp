
#include "ServerPort.h"
#include "common/MessageType.h"

namespace {
  void write_key_sequence(Serializer& s, const KeySequence& sequence) {
    s.write(static_cast<uint32_t>(sequence.size()));
    for (const auto& event : sequence)
      s.write(event);
  }

  void write_config(Serializer& s, const Config& config) {
    s.write(static_cast<uint32_t>(config.contexts.size()));
    for (const auto& context : config.contexts) {
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
      s.write(static_cast<uint32_t>(context.device_filter.size()));
      s.write(context.device_filter.data(), context.device_filter.size());
      s.write(context.invert_device_filter);
      
      // modifier filter
      write_key_sequence(s, context.modifier_filter);
      s.write(context.invert_modifier_filter);

      // fallthrough
      s.write(context.fallthrough);
    }
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
    write_config(s, config);
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

bool ServerPort::send_request_device_names() {
  return m_connection.send_message([&](Serializer& s) {
    s.write(MessageType::device_names);
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
        case MessageType::device_names: {
          const auto count = d.read<size_t>();
          auto device_names = std::vector<std::string>();
          for (auto i = 0u; i < count; ++i)
            device_names.push_back(d.read_string());
          handler.on_device_names_message(std::move(device_names));
          break;
        }
        default: break;
      }
    });
}
