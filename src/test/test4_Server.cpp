
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
    bool send_next_key_info(const std::vector<Key>& keys, const DeviceDesc& device_desc) override { return true; }

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
    
    void on_grab_device_filters_message(
        std::vector<GrabDeviceFilter> filters) override {
    }

    void set_configuration(MultiStagePtr multi_stage, DirectivesList directives) {
      m_client.inject_client_message(
        [ multi_stage = multi_stage.release(),
          directives = std::move(directives)
        ](ClientPort::MessageHandler& handler) mutable {
          handler.on_configuration_message(MultiStagePtr{ multi_stage });
          handler.on_directives_message(std::move(directives));
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
    auto [multi_stage, directives] = create_multi_stage(config);
    const auto activate_contexts = (activate_all_contexts ? 
      multi_stage->context_count() : size_t{ });

    auto client = std::make_unique<ClientPortImpl>();
    auto client_ptr = client.get();
    auto state = State(std::move(client), client_ptr);
    state.set_configuration(std::move(multi_stage), std::move(directives));

    auto indices = std::vector<int>();
    for (auto i = 0u; i < activate_contexts; ++i)
      indices.push_back(i);
    state.set_active_contexts(indices);

    state.set_device_descs({ 
      DeviceDesc{ "Device0" },
      DeviceDesc{ "Device1" },
      DeviceDesc{ "Device2" },
    });
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

TEST_CASE("Modifier filter and no might match (infinite loop bug)", "[Server]") {
  auto state = create_state(R"(
    ContextActive >> Virtual1

    [modifier = Virtual1]
    ? X >> A Virtual1
  )");
  CHECK(state.apply_input("+X") == "+A -A");
  CHECK(state.apply_input("-X") == "");
  CHECK(state.apply_input("+X") == "+X");
  CHECK(state.apply_input("-X") == "-X");
}

//--------------------------------------------------------------------

TEST_CASE("Modifier filter toggled in ContextActive", "[Server]") {
  auto state = create_state(R"(
    X >> Virtual1

    [modifier = Virtual1]
    ContextActive >> A Virtual1
  )");
  CHECK(state.apply_input("+X") == "+A -A");
  CHECK(state.apply_input("-X") == "");
  CHECK(state.apply_input("+X") == "+A -A");
  CHECK(state.apply_input("-X") == "");
}

//--------------------------------------------------------------------

TEST_CASE("Modifier filter toggled in two ContextActive (prevent infinite loop)", "[Server]") {
  auto state = create_state(R"(
    C >> Virtual1

    [modifier = Virtual1]
    ContextActive >> A Virtual1

    [modifier = "!Virtual1"]
    ContextActive >> B Virtual1
  )", false);
  CHECK(state.set_active_contexts({ 0, 1, 2 }) == "+B -B +A -A +B -B +A -A +B -B +A -A +B -B +A -A +B -B +A -A");
  CHECK(state.apply_input("+A -A") == "+A -A");
  CHECK(state.apply_input("+B -B") == "+B -B");
  CHECK(state.apply_input("+C") == "+B -B +A -A +B -B +A -A +B -B +A -A +B -B +A -A +B -B");
  CHECK(state.apply_input("+A -A") == "+A -A");
  CHECK(state.apply_input("+B -B") == "+B -B");
}

//--------------------------------------------------------------------

TEST_CASE("Modifier filter toggled by virtual modifier", "[Server]") {
  auto state = create_state(R"(
    A >> Virtual1{X}

    [modifier=Virtual1]
    ContextActive >> Y ^ Z
  )");

  CHECK(state.apply_input("+A") == "+Y -Y +X -X +Z -Z");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());
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
  CHECK(state.apply_input("-A") == "+ShiftLeft -A");
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

    [device-id = "device_id_b"]
    A >> Y

    [device = /B/]
    B >> R

    [device_id = /id_a/]   # deprecated filter name
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

TEST_CASE("Multi staging - Not Virtual", "[Server]") {
  auto state = create_state(R"(
    A >> Virtual1
    B >> !Virtual1

    [stage]
    Virtual1 >> Y
    !Virtual1 >> Z
  )");

  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("+A") == "+Z -Z -Y");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("+B") == "+Z -Z -Y");
  CHECK(state.apply_input("-B") == "");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-B") == "");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Multi staging - Virtual modifier", "[Server]") {
  auto state = create_state(R"(
    A >> Virtual1{X}

    [stage]
    Virtual1 >> Y
    !Virtual1 >> Z
  )");

  CHECK(state.apply_input("+A") == "+Y +X -X +Z -Z -Y");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());
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

//--------------------------------------------------------------------

TEST_CASE("Forwarding after timeout (#113)", "[Server]") {
  auto state = create_state(R"(
    !W Q{500ms} >> C
    (Q W) >> A
    Q     >> B
  )");

  CHECK(state.apply_input("+Q") == "");
  CHECK(state.apply_timeout_reached() == "+C");
  CHECK(state.apply_input("+Q") == "");
  CHECK(state.apply_timeout_reached() == "+C");
  CHECK(state.apply_input("+Q") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  // output is suppressed when timeout was exceeded once
  CHECK(state.apply_input("-Q") == "-C");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+Q") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-Q") == "+B -B");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+Q") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+W") == "+A");
  CHECK(state.apply_input("+W") == "+A");
  CHECK(state.apply_input("-W") == "-A");
  CHECK(state.apply_input("-Q") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+W") == "");
  CHECK(state.apply_input("+Q") == "+A");
  CHECK(state.apply_input("+Q") == "+A");
  CHECK(state.apply_input("-Q") == "");
  CHECK(state.apply_input("-W") == "-A");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Another timeout problem (#217)", "[Server]") {
  auto state = create_state(R"(
    D{!1000ms S} >> Tab
    D{K !K} >> X
  )");
  
  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-D") == "+D -D");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("-K") == "+X -X");
  CHECK(state.apply_input("-D") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("-D") == "+D +K -D");
  CHECK(state.apply_input("-K") == "-K");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+S") == "+Tab");
  CHECK(state.apply_input("+S") == "+Tab");
  CHECK(state.apply_input("-S") == "-Tab");
  CHECK(state.apply_input("-D") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_reached() == "");
  CHECK(state.apply_input("+S") == "+D +S");
  CHECK(state.apply_input("-S") == "-S");
  CHECK(state.apply_input("-D") == "-D");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+D") == "");
  CHECK(state.apply_timeout_reached() == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("+K") == "");
  CHECK(state.apply_input("-K") == "+X -X");
  CHECK(state.apply_input("-D") == "");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Wrong Keyrepeat after timeout (#216)", "[Server]") {
  auto state = create_state(R"(
    A{!50ms B} >> X
  )");
  
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "+A");
  CHECK(state.apply_input("-A") == "-A");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("-A") == "+A -A");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+C") == "+A +C");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
  CHECK(state.apply_input("-A") == "-A");
  REQUIRE(state.stage_is_clear());
  
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_not_reached() == "");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("-B") == "-X");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Suppress key repeat of all but last device", "[Server]") {
  auto state = create_state(R"(
  )");
  
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("+B", 1) == "+B");
  CHECK(state.apply_input("+A", 0) == "+A"); // <- unexpected
  CHECK(state.apply_input("+B", 1) == "+B");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+B", 1) == "+B");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+B", 1) == "+B");
  CHECK(state.apply_input("-B", 1) == "-B");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("-A", 0) == "-A");
  REQUIRE(state.stage_is_clear());

  // only stop key repeat by another key repeat
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("+ButtonLeft", 1) == "+ButtonLeft");
  CHECK(state.apply_input("-ButtonLeft", 1) == "-ButtonLeft");
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("+A", 0) == "+A");
  CHECK(state.apply_input("-A", 0) == "-A");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Inconsistent Mouse Button Behavior In Chords (#255)", "[Server]") {
  auto state = create_state(R"(
    Z{ButtonLeft} >> B
  )");
  
  // key repeat while mouse is pressed
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+ButtonLeft", 1) == "+B");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("-ButtonLeft", 1) == "-B");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("-Z", 0) == "");
  REQUIRE(state.stage_is_clear());

  // state is properly reset
  CHECK(state.apply_input("+X", 0) == "+X");
  CHECK(state.apply_input("+X", 0) == "+X");

  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+ButtonLeft", 1) == "+B");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("-ButtonLeft", 1) == "-B");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("+Z", 0) == "");
  CHECK(state.apply_input("-Z", 0) == "");

  CHECK(state.apply_input("-X", 0) == "-X");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Releasing after forwarding (#244)", "[Server]") {
  auto state = create_state(R"(
    @forward-modifiers Shift

    A{S{D}} >> Space
  )");
  
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("-A") == "+A -A");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("-ShiftLeft") == "+A -ShiftLeft");
  CHECK(state.apply_input("-A") == "-A");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("String substitution (#267)", "[Server]") {
  auto state = create_state(R"(
    ? "Abc" >> X
  )");
  
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
  REQUIRE(state.stage_is_clear());
  
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+X");
  CHECK(state.apply_input("-C") == "-X");
  REQUIRE(state.stage_is_clear());  
  
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");  
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+X");
  CHECK(state.apply_input("-C") == "-X");
  REQUIRE(state.stage_is_clear());    
}

//--------------------------------------------------------------------

TEST_CASE("String substitution B (#267)", "[Server]") {
  auto state = create_state(R"(
    ? "abc" >> X
  )");
  
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+X");
  CHECK(state.apply_input("-C") == "-X");
  REQUIRE(state.stage_is_clear());
  
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  CHECK(state.apply_input("-A") == "-A");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
  REQUIRE(state.stage_is_clear());  
  
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "+A");
  CHECK(state.apply_input("-A") == "-A");  
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  CHECK(state.apply_input("+B") == "+B");
  CHECK(state.apply_input("-B") == "-B");
  CHECK(state.apply_input("+C") == "+C");
  CHECK(state.apply_input("-C") == "-C");
  REQUIRE(state.stage_is_clear());    
}

//--------------------------------------------------------------------

TEST_CASE("Shortcuts with Shift", "[Server]") {
  auto state = create_state(R"(
    A >> 'X'
  )");
  
  CHECK(state.apply_input("+A") == "+ShiftLeft +X -X -ShiftLeft");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("+A") == "+X -X");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  REQUIRE(state.stage_is_clear());
  
  CHECK(state.apply_input("+ControlLeft") == "+ControlLeft");
  CHECK(state.apply_input("+A") == "+ShiftLeft +X -X -ShiftLeft"); // do not release Control
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("-ControlLeft") == "-ControlLeft");
  REQUIRE(state.stage_is_clear());  
}

//--------------------------------------------------------------------

TEST_CASE("Keyrepeat triggers last matching mapping (#275)", "[Server]") {
  auto state = create_state(R"(
    A{B} >> X
    B{A} >> Y
    C{A} >> Z
    C{B} >> W
  )");
  
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("-B") == "-X");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("-B") == "-X");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("-A") == "-Y");
  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("+A") == "+Y");
  CHECK(state.apply_input("-A") == "-Y");
  CHECK(state.apply_input("-B") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("+A") == "+Z");
  CHECK(state.apply_input("+A") == "+Z");
  CHECK(state.apply_input("-A") == "-Z");
  CHECK(state.apply_input("+B") == "+W");
  CHECK(state.apply_input("+B") == "+W");
  CHECK(state.apply_input("-B") == "-W");
  CHECK(state.apply_input("+A") == "+Z");
  CHECK(state.apply_input("+A") == "+Z");
  CHECK(state.apply_input("-A") == "-Z");
  CHECK(state.apply_input("+B") == "+W");
  CHECK(state.apply_input("+B") == "+W");
  CHECK(state.apply_input("-B") == "-W");
  CHECK(state.apply_input("-C") == "");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Matched are optional regression (#275)", "[Server]") {
  auto state = create_state(R"(
    Boss = Virtual1
    ScrollLock >> Virtual1
    Boss{Any} >> Any
    A >> X
  )");

  CHECK(state.apply_input("+A -A") == "+X -X");
  CHECK(state.apply_input("+ScrollLock -ScrollLock") == "");
  CHECK(state.apply_input("+A -A") == "+A -A");
  CHECK(state.apply_input("+ScrollLock -ScrollLock") == "");
  CHECK(state.apply_input("+A -A") == "+X -X");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Context with device filter fallthrough", "[Server]") {
  auto state = create_state(R"(
    [device = "Device0"]
    [device = "Device1"]
    A >> X

    [default]
    A >> Y
  )");

  CHECK(state.apply_input("+A", 0) == "+X");
  CHECK(state.apply_input("-A", 0) == "-X");
  CHECK(state.apply_input("+A", 1) == "+X");
  CHECK(state.apply_input("-A", 1) == "-X");
  CHECK(state.apply_input("+A", 2) == "+Y");
  CHECK(state.apply_input("-A", 2) == "-Y");
}

//--------------------------------------------------------------------

TEST_CASE("Hanging wheel events bug #280", "[Server]") {
  auto state = create_state(R"(
    A{WheelUp WheelUp WheelUp} >> 3
    A{WheelUp WheelUp} >> 2
    A{WheelUp} >> 1
  )");

  const auto wheel_up = KeySequence{ KeyEvent(Key::WheelUp, KeyState::Up, 120) };

  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("-A", 0) == "+A -A");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == ""); // Down is automatically inserted
  CHECK(state.apply_input("-A", 0) == "+1 -1");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("-A", 0) == "+2 -2");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "+3 -3");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input(wheel_up, 1) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("+A", 0) == "");
  CHECK(state.apply_input("-A", 0) == "+2 -2");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Toggle Virtual immediately", "[Server]") {
  auto state = create_state(R"(
    Virtual1 >> X
    A >> Virtual1 !Virtual1
    B >> Virtual1 ^ !Virtual1
    C >> ^ Virtual1 !Virtual1
  )");

  CHECK(state.apply_input("+A") == "+X -X");
  CHECK(state.apply_input("+A") == "+X -X");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+B") == "+X");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-B") == "-X");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("-C") == "+X -X");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Toggle Virtual chain", "[Server]") {
  auto state = create_state(R"(
    Virtual1 >> Virtual2 ^ Virtual2
    Virtual2 >> X ^ Y
    A >> Virtual1 !Virtual1
    B >> Virtual1 ^ !Virtual1
    C >> ^ Virtual1 !Virtual1
  )");

  CHECK(state.apply_input("+A") == "+X -X +Y -Y");
  CHECK(state.apply_input("+A") == "+X -X +Y -Y");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+B") == "+X -X");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-B") == "+Y -Y");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("-C") == "+X -X +Y -Y");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Toggle Virtual recursion", "[Server]") {
  auto state = create_state(R"(
    Virtual1 >> Virtual2 X ^ Virtual2
    Virtual2 >> Virtual1 Y ^ Virtual1
    A >> Virtual1 !Virtual1
    B >> Virtual1 ^ !Virtual1
    C >> ^ Virtual1 !Virtual1
  )");

  // output does not really matter, it just should not deadlock
  CHECK(state.apply_input("+A") == "+X -X +Y -Y +X -X +Y -Y +X -X");
  CHECK(state.apply_input("+A") == "+X -X +Y -Y +X -X +Y -Y");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+B") == "+X -X +Y -Y +X -X +Y -Y +X -X");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-B") == "+X -X +Y -Y +X -X +Y -Y");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("+C") == "");
  CHECK(state.apply_input("-C") == "+X -X +Y -Y +X -X +Y -Y +X -X");
  //REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Suppressed modifiers getting reapplied #291", "[Server]") {
  auto state = create_state(R"(
    @forward-modifiers Control Shift
    (Control Alt Shift) >> Meta
  )");

  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
  CHECK(state.apply_input("-ShiftLeft") == "-ShiftLeft");
  
  CHECK(state.apply_input("+AltLeft") == "");
  CHECK(state.apply_input("-AltLeft") == "+AltLeft -AltLeft");
  
  CHECK(state.apply_input("+ControlLeft") == "+ControlLeft");
  CHECK(state.apply_input("+ShiftLeft") == "+ShiftLeft");
#if defined(_WIN32)
  // handle "wait before releasing control too quickly" hack on Windows
  CHECK(state.apply_input("+AltLeft") == "-ShiftLeft");
  CHECK(state.flush() == "-ControlLeft +MetaLeft");
#else
  CHECK(state.apply_input("+AltLeft") == "-ShiftLeft -ControlLeft +MetaLeft");
#endif
  CHECK(state.apply_input("+E") == "+E");
  CHECK(state.apply_input("-E") == "-E");
  CHECK(state.apply_input("-AltLeft") == "");
  CHECK(state.apply_input("-ShiftLeft") == "-MetaLeft");
  CHECK(state.apply_input("-ControlLeft") == "");
  
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Ignore key repeat also when mouse was clicked #294", "[Server]") {
  auto state = create_state(R"(
    A{ButtonLeft 200ms} >> Z
    A{ButtonLeft} >> W
  )");
  
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+ButtonLeft") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_timeout_reached() == "+Z");
  CHECK(state.apply_input("-ButtonLeft") == "-Z");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+ButtonLeft") == "");
  CHECK(state.apply_timeout_not_reached() == "+W");
  CHECK(state.apply_input("-ButtonLeft") == "-W");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("Unresponsive key with together group and output on release #306", "[Server]") {
  auto state = create_state(R"(
    (A B) >> X ^
  )");
  
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("+A") == "+X -X");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("-B") == "");
  CHECK(state.apply_input("-A") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+B") == "+X -X");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-A") == "");
  CHECK(state.apply_input("-B") == "");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("+B") == "");
  CHECK(state.apply_input("-B") == "+B -B");
  REQUIRE(state.stage_is_clear());

  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("+A") == "");
  CHECK(state.apply_input("-A") == "+A -A");
  REQUIRE(state.stage_is_clear());
}

//--------------------------------------------------------------------

TEST_CASE("virtual-keys-toggle directive", "[Server]") {
  auto state = create_state(R"(
    @virtual-keys-toggle true
    F1 >> Virtual1
    Virtual1{X} >> Y
  )");

  CHECK(state.apply_input("+X -X") == "+X -X");
  CHECK(state.apply_input("+F1 -F1") == "");
  CHECK(state.apply_input("+X -X") == "+Y -Y");
  CHECK(state.apply_input("+F1 -F1") == "");
  CHECK(state.apply_input("+X -X") == "+X -X");
  CHECK(state.apply_input("+F1 -F1") == "");
  CHECK(state.apply_input("+F1 -F1") == "");
  CHECK(state.apply_input("+X -X") == "+X -X");
  REQUIRE(state.stage_is_clear());
  
  auto state2 = create_state(R"(
    @virtual-keys-toggle false
    F1 >> Virtual1
    F2 >> !Virtual1
    Virtual1{X} >> Y
  )");

  CHECK(state2.apply_input("+X -X") == "+X -X");
  CHECK(state2.apply_input("+F1 -F1") == "");
  CHECK(state2.apply_input("+X -X") == "+Y -Y");
  CHECK(state2.apply_input("+F1 -F1") == "");
  CHECK(state2.apply_input("+X -X") == "+Y -Y");
  CHECK(state2.apply_input("+F1 -F1") == "");
  CHECK(state2.apply_input("+F1 -F1") == "");
  CHECK(state2.apply_input("+X -X") == "+Y -Y");
  CHECK(state2.apply_input("+F2 -F2") == "");
  CHECK(state2.apply_input("+X -X") == "+X -X");
  CHECK(state2.apply_input("+F2 -F2") == "");
  CHECK(state2.apply_input("+F2 -F2") == "");
  CHECK(state2.apply_input("+X -X") == "+X -X");
  REQUIRE(state2.stage_is_clear());
}
