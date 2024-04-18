#pragma once

#include "ClientPort.h"
#include "runtime/Stage.h"

class ServerState : public ClientPort::MessageHandler {
public:
  explicit ServerState(std::unique_ptr<IClientPort> client =
    std::make_unique<ClientPort>());
  std::optional<Socket> listen_for_client_connections();
  std::optional<Socket> accept_client_connection();
  void disconnect();
  bool read_client_messages(std::optional<Duration> timeout = { });
  void reset_configuration(std::unique_ptr<Stage> stage = { });
  bool has_configuration() const;
  bool has_mouse_mappings() const;
  bool has_device_filters() const;
  void set_device_names(std::vector<std::string> device_names);
  bool should_exit() const;
  bool translate_input(KeyEvent input, int device_index);
  bool flush_send_buffer();
  bool sending_key() const { return m_sending_key; }
  bool stage_is_clear() const { return m_stage->is_clear(); }
  void schedule_flush(Duration delay = { });
  std::optional<Clock::time_point> flush_scheduled_at() const;
  std::optional<Clock::time_point> timeout_start_at() const;
  Duration timeout() const;
  void cancel_timeout();

protected:
  void on_configuration_message(std::unique_ptr<Stage> stage) override;
  void on_active_contexts_message(
      const std::vector<int>& active_contexts) override;
  void on_set_virtual_key_state_message(Key key, KeyState state) override;
  void on_validate_state_message() override;
  void on_device_names_message() override;
  virtual bool on_send_key(const KeyEvent& event) = 0;
  virtual void on_flush_scheduled(Duration timeout) { }
  virtual void on_timeout_scheduled(Duration timeout) { }
  virtual void on_timeout_cancelled() { }
  virtual void on_exit_requested() = 0;
  virtual bool on_validate_key_is_down(Key key) { return true; }

  void release_all_keys();
  void set_active_contexts(const std::vector<int>& active_contexts);
  void send_key_sequence(const KeySequence& key_sequence);
  void schedule_timeout(Duration timeout, bool cancel_on_up);
  void set_virtual_key_state(Key key, KeyState state);
  void toggle_virtual_key(Key key);
  void evaluate_device_filters();
  void send_devices_error_message(const std::string& message);

private:
  std::unique_ptr<IClientPort> m_client;
  std::unique_ptr<Stage> m_stage;
  std::vector<KeyEvent> m_send_buffer;
  std::vector<Key> m_virtual_keys_down;
  KeyEvent m_last_key_event;
  bool m_sending_key{ };
  std::optional<Clock::time_point> m_flush_scheduled_at;
  std::optional<Clock::time_point> m_timeout_start_at;
  Duration m_timeout;
  bool m_cancel_timeout_on_up{ };
  std::vector<std::string> m_device_names;
};
