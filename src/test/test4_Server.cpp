
#include "test.h"
#include "runtime/Timeout.h"
#include "server/ServerState.h"
#include <utility>

namespace {
  class ClientPortImpl : public IClientPort {
  private:
    std::vector<std::function<void(MessageHandler&)>> m_client_messages;
    std::vector<int> m_triggered_actions;

  public:
    Socket socket() const override { return 0; }
    Socket listen_socket() const override { return 0; }
    bool version_mismatch() const override { return false; }
    bool listen() override { return false; }
    bool accept() override { return false; }
    void disconnect() override { }
    bool send_triggered_action(int action) override { m_triggered_actions.push_back(action); return true; }
    bool send_virtual_key_state(Key key, KeyState state) override { return true; }
    bool send_next_key_info(Key key, const DeviceDesc& device_desc) override { return true; }

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

    std::vector<int> reset_triggered_actions() { return std::exchange(m_triggered_actions, std::vector<int>()); }
  };

  class State : public ServerState {
  private:
    ClientPortImpl& m_client;
    KeySequence m_output;

  public:
    State(std::unique_ptr<IClientPort> client, ClientPortImpl* client_ptr) 
      : ServerState(std::move(client)),
        m_client(*client_ptr) {
    }

    ClientPortImpl& client() { return m_client; }

    bool on_send_key(const KeyEvent& event) override {
      m_output.push_back(event);
      return true;
    }

    void on_exit_requested() override {
    }

    void set_configuration(MultiStagePtr multi_stage) {
      m_client.inject_client_message([multi_stage = multi_stage.release()](
          ClientPort::MessageHandler& handler) mutable {
        handler.on_configuration_message(MultiStagePtr{ multi_stage });
      });
      read_client_messages();
    }

    std::string set_active_contexts(std::vector<int> indices) {
      m_client.inject_client_message([indices = std::move(indices)](
          ClientPort::MessageHandler& handler) mutable {
        handler.on_active_contexts_message(std::move(indices));
      });
      read_client_messages();

      auto result = format_sequence(m_output);
      m_output.clear();
      return result;
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

    std::string flush() {
      flush_send_buffer();

      auto result = format_sequence(m_output);
      m_output.clear();
      return result;
    }

    template<size_t N>
    std::string apply_input(const char(&input)[N], int device_index = 0) {
      return apply_input(parse_sequence(input), device_index);
    }

    std::string apply_timeout(Duration timeout) {
      auto event = make_input_timeout_event(timeout);
      cancel_timeout();
      return apply_input(KeySequence{ event }, Stage::any_device_index);
    }

    std::string apply_timeout_reached() {
      assert(timeout_start_at());
      return apply_timeout(timeout());
    }

    std::string apply_timeout_not_reached() {
      assert(timeout_start_at());
      return apply_timeout(timeout() - std::chrono::milliseconds(1));
    }
  };

  State create_state(const char* config, bool activate_all_contexts = true) {
    auto multi_stage = create_multi_stage(config);
    const auto activate_contexts = (activate_all_contexts ? 
      multi_stage->context_count() : size_t{ });

    auto client = std::make_unique<ClientPortImpl>();
    auto client_ptr = client.get();
    auto state = State(std::move(client), client_ptr);
    state.set_configuration(std::move(multi_stage));

    auto indices = std::vector<int>();
    for (auto i = 0u; i < activate_contexts; ++i)
      indices.push_back(i);
    state.set_active_contexts(indices);

    state.set_device_descs({ DeviceDesc{ "Device0" } });
    return state;
  }
} // namespace

//--------------------------------------------------------------------

TEST_CASE("Minimal configuration", "[Server]") {
  auto state = create_state(R"(
    A >> B
  )");
  CHECK(state.apply_input("+X") == "+X");
  CHECK(state.apply_input("-X") == "-X");
  CHECK(state.apply_input("+A") == "+B");
  CHECK(state.apply_input("-A") == "-B");
}

//--------------------------------------------------------------------

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

//--------------------------------------------------------------------

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

//--------------------------------------------------------------------

TEST_CASE("Not Any with action in ContextActive", "[Server]") {
  auto state = create_state(R"(
    ContextActive >> $(action1) ^ $(action2)
    A             >> S
    B             >> !Any T
  )");

  CHECK(state.client().reset_triggered_actions().size() == 1);
  CHECK(state.apply_input("+A") == "+S");
  CHECK(state.apply_input("-A") == "-S");
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+B") == "-ShiftLeft +T");
  CHECK(state.apply_input("-B") == "-T");
  CHECK(state.apply_input("-ShiftLeft") == "");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
  CHECK(state.client().reset_triggered_actions().size() == 0);
}

//--------------------------------------------------------------------

TEST_CASE("Device context filter", "[Server]") {
  auto state = create_state(R"(
    [device = "DeviceA"]
    A >> X

    [device_id = "device_id_b"]
    A >> Y

    [device = /B/]
    B >> R

    [device_id = /id_a/]
    B >> S
  )");

  state.set_device_descs({
    { "DeviceA", "device_id_a" }, // 0
    { "DeviceB", "device_id_b" }, // 1
  });
  CHECK(state.apply_input("+A", 0) == "+X");
  CHECK(state.apply_input("-A", 0) == "-X");
  CHECK(state.apply_input("+A", 1) == "+Y");
  CHECK(state.apply_input("-A", 1) == "-Y");
  CHECK(state.apply_input("+B", 0) == "+S");
  CHECK(state.apply_input("-B", 0) == "-S");
  CHECK(state.apply_input("+B", 1) == "+R");
  CHECK(state.apply_input("-B", 1) == "-R");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging", "[Server]") {
  auto state = create_state(R"(
    # colemak layout
    S >> R
    D >> S
    F >> T

    [stage]
    BS = Backspace

    [default]
    ? R A T >> BS BS "cat"
  )");

  CHECK(state.apply_input("+S") == "+R");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("+F") == "+Backspace -Backspace +Backspace -Backspace -R -A +C -C +A -A +T -T");
  CHECK(state.apply_input("-F") == "");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("-S") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - timeout", "[Server]") {
  auto state = create_state(R"(
    A{500ms} >> B

    [stage]
    B{500ms} >> C
  )");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_not_reached() == "+A");
  CHECK(state.apply_input("-A") == "-A");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "");
  CHECK(state.apply_input("-A") == "+B -B");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "");
  CHECK(state.apply_timeout_not_reached() == "+B");
  CHECK(state.apply_input("-A") == "-B");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "");
  CHECK(state.apply_timeout_reached() == "+C");
  CHECK(state.apply_input("-A") == "-C");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - not timeout", "[Server]") {
  auto state = create_state(R"(
    A{!500ms} >> B

    [stage]
    B{!500ms} >> C
  )");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "+A");
  CHECK(state.apply_input("-A") == "-A");

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.flush() == "+B -B");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - virtual key", "[Server]") {
  auto state = create_state(R"(
    X >> Virtual1
    Virtual1{A} >> R

    [stage]
    Y >> Virtual2
    Virtual2{B} >> S
    Virtual1{C} >> T
  )");

  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");

  CHECK(state.apply_input("+X") == "");
  CHECK(state.apply_input("-X") == "");

  CHECK(state.apply_input("+A") == "+R");
  CHECK(state.apply_input("-A") == "-R");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+T");
  CHECK(state.apply_input("-C") == "-T");

  CHECK(state.apply_input("+Y") == "");
  CHECK(state.apply_input("-Y") == "");

  CHECK(state.apply_input("+A") == "+R");
  CHECK(state.apply_input("-A") == "-R");
  CHECK(state.apply_input("+B") == "+S");
  CHECK(state.apply_input("-B") == "-S");
  CHECK(state.apply_input("+C") == "+T");
  CHECK(state.apply_input("-C") == "-T");

  CHECK(state.apply_input("+X") == "");
  CHECK(state.apply_input("-X") == "");

  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+S");
  CHECK(state.apply_input("-B") == "-S");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");

  CHECK(state.apply_input("+Y") == "");
  CHECK(state.apply_input("-Y") == "");

  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - actions", "[Server]") {
  auto state = create_state(R"(
    F1 >> $(action0)

    [stage]
    F2 >> $(action1)
  )");

  CHECK(state.apply_input("+F1") == "");
  CHECK(state.apply_input("-F1") == "");
  CHECK(state.client().reset_triggered_actions().size() == 1);

  CHECK(state.apply_input("+F2") == "");
  CHECK(state.apply_input("-F2") == "");
  CHECK(state.client().reset_triggered_actions().size() == 1);
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - output on release", "[Server]") {
  auto state = create_state(R"(
    F1 >> A ^ B

    [stage]
    F2 >> C ^ D
  )");

  CHECK(state.apply_input("+F1") == "+A -A");
  CHECK(state.apply_input("-F1") == "+B -B");
  CHECK(state.apply_input("+F2") == "+C -C");
  CHECK(state.apply_input("-F2") == "+D -D");

  CHECK(state.apply_input("+F1") == "+A -A");
  CHECK(state.apply_input("+F2") == "+C -C");
  CHECK(state.apply_input("-F1") == "+B -B");
  CHECK(state.apply_input("-F2") == "+D -D");

  CHECK(state.apply_input("+F1") == "+A -A");
  CHECK(state.apply_input("+F2") == "+C -C");
  CHECK(state.apply_input("-F2") == "+D -D");
  CHECK(state.apply_input("-F1") == "+B -B");

  CHECK(state.apply_input("+F2") == "+C -C");
  CHECK(state.apply_input("+F1") == "+A -A");
  CHECK(state.apply_input("-F2") == "+D -D");
  CHECK(state.apply_input("-F1") == "+B -B");
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - ContextActive", "[Server]") {
  auto state = create_state(R"(
    [title="App1"]    # 0
    ContextActive >> A ^ B

    [stage]           # 1

    [title="App2"]    # 2
    ContextActive >> C ^ D
  )", false);

  CHECK(state.set_active_contexts({ 1 }) == "");

  CHECK(state.set_active_contexts({ 0, 1 }) == "+A -A");
  CHECK(state.set_active_contexts({ 1 }) == "+B -B");
  CHECK(state.set_active_contexts({ 1, 2 }) == "+C -C");
  CHECK(state.set_active_contexts({ 0, 1 }) == "+A -A +D -D");
  CHECK(state.set_active_contexts({ 1 }) == "+B -B");
}
