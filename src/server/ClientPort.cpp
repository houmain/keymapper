
#include "ClientPort.h"
#include "runtime/Stage.h"

namespace {
  KeySequence read_key_sequence(Deserializer& d, bool& has_mouse_mappings) {
    auto sequence = KeySequence();
    auto size = d.read<uint8_t>();
    for (auto i = 0; i < size; ++i) {
      auto& event = sequence.emplace_back();
      event.key = d.read<Key>();
      event.state = d.read<KeyState>();
      has_mouse_mappings |= is_mouse_button(event.key);
    }
    return sequence;
  }

  std::unique_ptr<Stage> read_config(Deserializer& d) {
    auto has_mouse_mappings = false;

    // receive contexts
    auto contexts = std::vector<Stage::Context>();
    auto count = d.read<uint32_t>();
    contexts.resize(count);
    for (auto& context : contexts) {
      // inputs
      count = d.read<uint32_t>();
      context.inputs.resize(count);
      for (auto& input : context.inputs) {
        input.input = read_key_sequence(d, has_mouse_mappings);
        input.output_index = d.read<int32_t>();
      }

      // outputs
      count = d.read<uint32_t>();
      context.outputs.resize(count);
      for (auto& output : context.outputs) {
        output = read_key_sequence(d, has_mouse_mappings);
      }

      // command outputs
      count = d.read<uint32_t>();
      context.command_outputs.resize(count);
      for (auto& command : context.command_outputs) {
        command.output = read_key_sequence(d, has_mouse_mappings);
        command.index = d.read<int32_t>();
      }
    }
    return std::make_unique<Stage>(std::move(contexts),
      has_mouse_mappings);
  }

  void read_active_contexts(Deserializer& d, std::vector<int>* indices) {
    indices->clear();
    const auto count = d.read<uint32_t>();
    for (auto i = 0u; i < count; ++i)
      indices->push_back(d.read<uint32_t>());
  }
} // namespace

ClientPort::ClientPort() = default;
ClientPort::~ClientPort() = default;

Connection::Socket ClientPort::socket() const {
  return (m_connection ? m_connection->socket() : Connection::invalid_socket);
}

Connection::Socket ClientPort::listen_socket() const {
  return (m_connection ? m_connection->listen_socket() : Connection::invalid_socket);
}

bool ClientPort::initialize() {
  auto connection = std::make_unique<Connection>();
  if (!connection->listen())
    return false;

  m_connection = std::move(connection);
  return true;
}

bool ClientPort::accept() {
  return (m_connection && m_connection->accept());
}

void ClientPort::disconnect() {
  if (m_connection)
    m_connection->disconnect();
}

std::unique_ptr<Stage> ClientPort::read_config(Deserializer& d) {
  return ::read_config(d);
}

const std::vector<int>& ClientPort::read_active_contexts(Deserializer& d) {
  ::read_active_contexts(d, &m_active_context_indices);
  return m_active_context_indices;
}

bool ClientPort::send_triggered_action(int action) {
  return m_connection && m_connection->send_message(
    [&](Serializer& s) {
      s.write(static_cast<uint32_t>(action));
    });
}
