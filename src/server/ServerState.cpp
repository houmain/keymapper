
#include "ServerState.h"
#include "server/verbose_debug_io.h"
#include "runtime/Timeout.h"
#include "common/output.h"

ServerState::ServerState(std::unique_ptr<IClientPort> client)
  : m_client(std::move(client)),
    m_stage(std::make_unique<Stage>()) {
}

void ServerState::on_configuration_message(std::unique_ptr<Stage> stage) {
  if (!stage)
    return error("Receiving configuration failed");
  reset_configuration(std::move(stage));  
}

void ServerState::on_active_contexts_message(
    const std::vector<int>& active_contexts) {
  verbose("Active contexts received (%u)", active_contexts.size());
  set_active_contexts(active_contexts);
}

void ServerState::set_active_contexts(const std::vector<int>& active_contexts) {
  auto output = m_stage->set_active_client_contexts(active_contexts);
  send_key_sequence(output);
  m_stage->reuse_buffer(std::move(output));
  if (!m_flush_scheduled_at)
    flush_send_buffer();
}

void ServerState::on_set_virtual_key_state_message(Key key, KeyState state) {
  set_virtual_key_state(key, state);
  if (!m_flush_scheduled_at)
    flush_send_buffer();
}

void ServerState::on_validate_state_message() {
  verbose("Validating state");
  m_stage->validate_state(std::bind(&ServerState::on_validate_key_is_down, 
    this, std::placeholders::_1));
}

void ServerState::on_device_names_message() {
  m_client->send_device_names(m_device_names);
}

void ServerState::send_devices_error_message(const std::string& message) {
  m_client->send_device_names({ message });
}
  
void ServerState::release_all_keys() {
  const auto& keys_down = m_stage->get_output_keys_down();
  if (!keys_down.empty()) {
    verbose("Releasing all keys (%d)", keys_down.size());
    for (auto key : keys_down)
      m_send_buffer.push_back(KeyEvent(key, KeyState::Up));
  }
}

void ServerState::send_key_sequence(const KeySequence& key_sequence) {
  for (const auto& event : key_sequence)
    m_send_buffer.push_back(event);
}

std::optional<Socket> ServerState::listen_for_client_connections() {
  if (m_client->listen())
    return m_client->listen_socket();
  error("Initializing keymapper connection failed");
  return { };
}

std::optional<Socket> ServerState::accept_client_connection() {
  if (m_client->accept())
    return m_client->socket();
  error("Accepting keymapper connection failed");
  return { };  
}

void ServerState::disconnect() {
  m_client->disconnect();
}

bool ServerState::read_client_messages(std::optional<Duration> timeout) {
  return m_client->read_messages(*this, timeout);
}

void ServerState::reset_configuration(std::unique_ptr<Stage> stage) {
  release_all_keys();
  flush_send_buffer();
  verbose("Resetting configuration");
  m_stage = (stage ? std::move(stage) : std::make_unique<Stage>());
  m_virtual_keys_down.clear();
  m_flush_scheduled_at.reset();
  m_timeout_start_at.reset();
  evaluate_device_filters();
}

void ServerState::set_device_names(std::vector<std::string> device_names) {
  m_device_names = std::move(device_names);
  evaluate_device_filters();
}

void ServerState::evaluate_device_filters() {
  if (!m_stage->has_device_filters() ||
      m_device_names.empty())
    return;
  verbose("Evaluating device filters");
  m_stage->evaluate_device_filters(m_device_names);

  // reevaluate active contexts
  set_active_contexts(m_stage->active_client_contexts());
}

bool ServerState::has_configuration() const {
  return !m_stage->contexts().empty();
}

bool ServerState::has_mouse_mappings() const {
  return m_stage->has_mouse_mappings();
}

bool ServerState::has_device_filters() const {
  return m_stage->has_device_filters();
}

bool ServerState::should_exit() const {
  if (!m_stage->should_exit())
    return false;
  verbose("Read exit sequence");
  return true;
}

void ServerState::set_virtual_key_state(Key key, KeyState state) {
  const auto it = std::find(m_virtual_keys_down.begin(), m_virtual_keys_down.end(), key);
  if (it == m_virtual_keys_down.end() && state != KeyState::Up) {
    state = KeyState::Down;
    m_virtual_keys_down.push_back(key);
    translate_input({ key, state }, Stage::any_device_index);
  }
  else if (it != m_virtual_keys_down.end() && state != KeyState::Down) {
    state = KeyState::Up;
    m_virtual_keys_down.erase(it);
    translate_input({ key, state }, Stage::any_device_index);
  }
  else {
    return;
  }
  if (is_virtual_key(key))
    m_client->send_virtual_key_state(key, state);
}

void ServerState::toggle_virtual_key(Key key) {
  set_virtual_key_state(key, KeyState::Not);
}

bool ServerState::translate_input(KeyEvent input, int device_index) {
  // ignore key repeat while a flush or a timeout is pending
  if (input == m_last_key_event && 
        (m_flush_scheduled_at || m_timeout_start_at)) {
    verbose_debug_io(input, { }, true);
    return true;
  }

  [[maybe_unused]] auto cancelled_timeout = false;
  if (m_timeout_start_at &&
      (input.state == KeyState::Down || m_cancel_timeout_on_up)) {
    // cancel current time out, inject event with elapsed time
    const auto time_since_timeout_start = 
      (Clock::now() - *m_timeout_start_at);
    cancel_timeout();
    translate_input(make_input_timeout_event(time_since_timeout_start), device_index);
    cancelled_timeout = true;
  }

#if defined(_WIN32)
  // turn NumLock succeeding Pause into another Pause
  auto translated_numlock_to_pause = false;
  if (input.state == m_last_key_event.state && 
      input.key == Key::NumLock && 
      m_last_key_event.key == Key::Pause) {
    input.key = Key::Pause;
    translated_numlock_to_pause = true;
  }
#endif

  if (input.key != Key::timeout)
    m_last_key_event = input;

  auto output = m_stage->update(input, device_index);

  if (m_stage->should_exit()) {
    verbose("Read exit sequence");
    reset_configuration();
    on_exit_requested();
    return true;
  }

  // waiting for input timeout
  if (!output.empty() && output.back().key == Key::timeout) {
    const auto& request = output.back();
    schedule_timeout(
      timeout_to_milliseconds(request.timeout), 
      cancel_timeout_on_up(request.state));
    output.pop_back();
  }

#if defined(_WIN32)
  const auto translated =
      output.size() != 1 ||
      output.front().key != input.key ||
      (output.front().state == KeyState::Up) != (input.state == KeyState::Up) ||
      translated_numlock_to_pause;

  const auto intercept_and_send =
      m_flush_scheduled_at ||
      cancelled_timeout ||
      translated ||
      // always intercept and send AltGr
      input.key == Key::AltRight;
#else
  const auto intercept_and_send = true;
#endif

  verbose_debug_io(input, output, intercept_and_send);

  if (intercept_and_send)
    send_key_sequence(output);

  m_stage->reuse_buffer(std::move(output));
  return intercept_and_send;
}

bool is_control_up(const KeyEvent& event) {
  return (event.state == KeyState::Up &&
        (event.key == Key::ControlLeft ||
          event.key == Key::ControlRight));
}

bool ServerState::flush_send_buffer() {
  if (m_sending_key)
    return true;
  m_sending_key = true;
  m_flush_scheduled_at.reset();

  auto succeeded = true;
  auto i = size_t{ };
  for (; i < m_send_buffer.size(); ++i) {
    const auto& event = m_send_buffer[i];

    if (is_action_key(event.key)) {
      if (event.state == KeyState::Down)
        m_client->send_triggered_action(
          static_cast<int>(*event.key - *Key::first_action));
      continue;
    }

    if (is_virtual_key(event.key)) {
      if (event.state == KeyState::Down)
        toggle_virtual_key(event.key);
      continue;
    }

    if (event.key == Key::timeout) {
      schedule_flush(timeout_to_milliseconds(event.timeout));
      ++i;
      break;
    }

#if defined(_WIN32)
    // do not release Control too quickly
    // otherwise copy/paste does not work in some input fields
    const auto is_first = (i == 0);
    if (!is_first && is_control_up(event)) {
      schedule_flush(std::chrono::milliseconds(10));
      break;
    }
#endif

    if (!on_send_key(event)) {
      succeeded = false;
      break;
    }
  }
  m_send_buffer.erase(m_send_buffer.begin(), m_send_buffer.begin() + i);
  m_sending_key = false;
  return succeeded;
}

void ServerState::schedule_flush(Duration delay) {
  if (m_flush_scheduled_at)
    return;
  m_flush_scheduled_at = Clock::now() + 
    std::chrono::duration_cast<Clock::duration>(delay);
  on_flush_scheduled(delay);
}

std::optional<Clock::time_point> ServerState::flush_scheduled_at() const {
  return m_flush_scheduled_at;
}

void ServerState::schedule_timeout(Duration timeout, bool cancel_on_up) {
  m_timeout = timeout;
  m_timeout_start_at = Clock::now();
  m_cancel_timeout_on_up = cancel_on_up;
  on_timeout_scheduled(timeout);
}

std::optional<Clock::time_point> ServerState::timeout_start_at() const {
  return m_timeout_start_at;
}

Duration ServerState::timeout() const {
  return m_timeout;
}

void ServerState::cancel_timeout() {
  m_timeout_start_at.reset();
  m_timeout = { };
  on_timeout_cancelled();
}
