
#include "test.h"
#include "runtime/Timeout.h"
#include "server/ServerState.h"

namespace {
  class ClientPort : public IClientPort {
  private:
    std::vector<std::function<void(MessageHandler&)>> m_client_messages;
  public:
    Socket socket() const override { return 0; }
    Socket listen_socket() const override { return 0; }
    bool listen() override { return false; }
    bool accept() override { return false; }
    void disconnect() override { }
    bool send_triggered_action(int action) override { return true; }
    bool send_virtual_key_state(Key key, KeyState state) override { return true; }
    bool send_device_names(const std::vector<std::string>& device_names) override { return true; }

    bool read_messages(MessageHandler& handler, 
        std::optional<Duration> timeout) override {
      for (auto i = 0u; i < m_client_messages.size(); ++i) 
        m_client_messages[i](handler);
      m_client_messages.clear();
      return true; 
    }

    void inject_client_message(std::function<void(MessageHandler&)> send) {
      m_client_messages.push_back(send);
    }
  };

  class State : public ServerState {
  private:
    ClientPort& m_client;
    KeySequence m_output;

  public:
    State(std::unique_ptr<IClientPort> client, ClientPort* client_ptr) 
      : ServerState(std::move(client)),
        m_client(*client_ptr) {
    }

    bool on_send_key(const KeyEvent& event) override {
      m_output.push_back(event);
      return true;
    }

    void on_exit_requested() override {
    }

    void set_configuration(Stage&& stage) {
      m_client.inject_client_message([stage = std::move(stage)](
          ClientPort::MessageHandler& handler) mutable {
        handler.on_configuration_message(
          std::make_unique<Stage>(std::move(stage)));
      });
      read_client_messages();
    }

    void set_active_contexts(std::vector<int> indices) {
      m_client.inject_client_message([indices = std::move(indices)](
          ClientPort::MessageHandler& handler) mutable {
        handler.on_active_contexts_message(std::move(indices));
      });
      read_client_messages();
    }

    std::string apply_input(const KeySequence& sequence, int device_index = 0) {
      for (auto event : sequence)
        if (!translate_input(event, device_index))
          m_output.push_back(event);

      if (!flush_scheduled_at())
        flush_send_buffer();
      auto result = format_sequence(m_output);
      m_output.clear();
      return result;
    }

    template<size_t N>
    std::string apply_input(const char(&input)[N], int device_index = 0) {
      return apply_input(parse_sequence(input), device_index);
    }

    std::string apply_timeout_reached() {
      auto event = make_input_timeout_event(timeout());
      cancel_timeout();
      return apply_input(KeySequence{ event }, Stage::any_device_index);
    }

    std::string apply_timeout_not_reached() {
      auto event = make_input_timeout_event(
        timeout() - std::chrono::milliseconds(1));
      cancel_timeout();
      return apply_input(KeySequence{ event }, Stage::any_device_index);
    }
  };

  State create_state(const char* config, 
      bool activate_all_contexts = true) {
    auto stage = create_stage(config, false);
    const auto activate_contexts = (activate_all_contexts ? 
      stage.contexts().size() : size_t{ });
    auto indices = std::vector<int>();
    for (auto i = 0u; i < activate_contexts; ++i)
      indices.push_back(i);

    auto client = std::make_unique<ClientPort>();
    auto client_ptr = client.get();
    auto state = State(std::move(client), client_ptr);
    state.set_configuration(std::move(stage));
    state.set_active_contexts(indices);
    state.set_device_names({ "Device0 "});
    return state;
  }
} // namespace

TEST_CASE("Minimal configuration", "[Server]") {
  auto state = create_state(R"(
    A >> B
  )");
  CHECK(state.apply_input("+X") == "+X");
  CHECK(state.apply_input("-X") == "-X");
  CHECK(state.apply_input("+A") == "+B");
  CHECK(state.apply_input("-A") == "-B");
}

TEST_CASE("Trigger Not Timeout", "[Server]") {
  auto state = create_state(R"(
    ShiftLeft{!200ms} >> B
    Shift >> Shift
    J >> N
    K >> E
  )");
  // shift released before timeout
  CHECK(state.apply_input("+ShiftLeft") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-ShiftLeft") == "+B -B");
  REQUIRE(state.stage_is_clear());

  // shift hold until timeout
  CHECK(state.apply_input("+ShiftLeft") == "");
  CHECK(state.apply_timeout_reached() == "+ShiftLeft");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());

  // other key pressed while waiting for timeout
  CHECK(state.apply_input("+ShiftLeft") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+J") == "+ShiftLeft +N");
  CHECK(state.apply_input("-J") == "-N");
  CHECK(state.apply_input("+K") == "+E");
  CHECK(state.apply_input("-K") == "-E");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());

  // releasing key after timeout reached
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("+ShiftLeft") == "");
  CHECK(state.apply_timeout_reached() == "+ShiftLeft");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());

  // releasing key while waiting for timeout
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("+ShiftLeft") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-A") == "-A +ShiftLeft");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());
}

TEST_CASE("ContextActive with fallthrough contexts", "[Server]") {
  auto state = create_state(R"(
    [modifier = B]
    [modifier = A]
    ContextActive >> X ^ Y
  )");

  CHECK(state.apply_input("+A") == "+X -X +A");
  CHECK(state.apply_input("-A") == "-A +Y -Y");
  
  CHECK(state.apply_input("+B") == "+X -X +B");
  CHECK(state.apply_input("-B") == "-B +Y -Y");
  
  CHECK(state.apply_input("+A") == "+X -X +A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("-A") == "-A +Y -Y");

  CHECK(state.apply_input("+A") == "+X -X +A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("-B") == "-B +Y -Y");
  
  CHECK(state.apply_input("+B") == "+X -X +B");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("-B") == "-B +Y -Y");

  CHECK(state.apply_input("+B") == "+X -X +B");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("-A") == "-A +Y -Y");  
}
