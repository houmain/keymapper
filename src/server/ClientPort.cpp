
#include "ClientPort.h"

namespace {
  KeySequence read_key_sequence(Deserializer& d) {
    auto sequence = KeySequence();
    auto size = d.read<uint32_t>();
    for (auto i = 0u; i < size; ++i) {
      auto& event = sequence.emplace_back();
      d.read(&event);
    }
    return sequence;
  }

  std::unique_ptr<Stage> read_config(Deserializer& d) {
    // receive contexts
    auto contexts = std::vector<Stage::Context>();
    auto count = d.read<uint32_t>();
    contexts.resize(count);
    for (auto& context : contexts) {
      // inputs
      count = d.read<uint32_t>();
      context.inputs.resize(count);
      for (auto& input : context.inputs) {
        input.input = read_key_sequence(d);
        input.output_index = d.read<int32_t>();
      }

      // outputs
      count = d.read<uint32_t>();
      context.outputs.resize(count);
      for (auto& output : context.outputs) {
        output = read_key_sequence(d);
      }

      // command outputs
      count = d.read<uint32_t>();
      context.command_outputs.resize(count);
      for (auto& command : context.command_outputs) {
        command.output = read_key_sequence(d);
        command.index = d.read<int32_t>();
      }

      // device filter
      context.device_filter.resize(d.read<uint32_t>(), ' ');
      d.read(context.device_filter.data(), context.device_filter.size());
      d.read(&context.invert_device_filter);
      
      // modifier filter
      context.modifier_filter = read_key_sequence(d);
      d.read(&context.invert_modifier_filter);

      // fallthrough
      d.read(&context.fallthrough);
    }
    return std::make_unique<Stage>(std::move(contexts));
  }

  void read_active_contexts(Deserializer& d, std::vector<int>* indices) {
    indices->clear();
    const auto count = d.read<uint32_t>();
    for (auto i = 0u; i < count; ++i)
      indices->push_back(static_cast<int>(d.read<uint32_t>()));
  }
} // namespace

ClientPort::ClientPort() 
  : m_host("keymapper") {
}

bool ClientPort::listen() {
  return m_host.listen();
}

bool ClientPort::accept() {
  m_connection = m_host.accept();
  return static_cast<bool>(m_connection);
}

void ClientPort::disconnect() {
  m_connection.disconnect();
}

std::unique_ptr<Stage> ClientPort::read_config(Deserializer& d) {
  return ::read_config(d);
}

const std::vector<int>& ClientPort::read_active_contexts(Deserializer& d) {
  ::read_active_contexts(d, &m_active_context_indices);
  return m_active_context_indices;
}

bool ClientPort::send_triggered_action(int action) {
  return m_connection.send_message(
    [&](Serializer& s) {
      s.write(MessageType::execute_action);
      s.write(static_cast<uint32_t>(action));
    });
}

bool ClientPort::send_virtual_key_state(Key key, KeyState state) {
  return m_connection.send_message(
    [&](Serializer& s) {
      s.write(MessageType::virtual_key_state);
      s.write(key);
      s.write(state);
    });
}

bool ClientPort::send_device_names(const std::vector<std::string>& device_names) {
  return m_connection.send_message(
    [&](Serializer& s) {
      s.write(MessageType::device_names);
      s.write(device_names.size());
      for (const auto& device_name : device_names)
        s.write(device_name);
    });
}

bool ClientPort::read_messages(MessageHandler& handler,
    std::optional<Duration> timeout) {
  return m_connection.read_messages(timeout,
    [&](Deserializer& d) {
      switch (d.read<MessageType>()) {
        case MessageType::configuration: {
          handler.on_configuration_message(read_config(d));
          break;
        }
        case MessageType::active_contexts: {
          handler.on_active_contexts_message(read_active_contexts(d));
          break;
        }
        case MessageType::set_virtual_key_state: {
          const auto key = d.read<Key>();
          const auto state = d.read<KeyState>();
          handler.on_set_virtual_key_state_message(key, state);
          break;
        }
        case MessageType::validate_state: {
          handler.on_validate_state_message();
          break;
        }
        case MessageType::device_names: {
          handler.on_device_names_message();
          break;
        }
        default: break;
      }
    });
}
