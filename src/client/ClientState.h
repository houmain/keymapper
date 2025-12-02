#pragma once

#include "client/FocusedWindow.h"
#include "client/ConfigFile.h"
#include "client/ServerPort.h"
#include "client/ControlPort.h"

class ClientState : public ServerPort::MessageHandler,
                    public ControlPort::MessageHandler {
public:
  const std::filesystem::path& config_filename() const;
  bool is_focused_window_inaccessible() const;
  bool is_active() const { return m_active; }

  bool load_config(std::filesystem::path filename);
  bool update_config(bool check_modified);
  const Config& config() const;
  std::optional<Socket> connect_server();
  bool read_server_messages(std::optional<Duration> timeout = { });
  void on_server_disconnected();
  bool initialize_contexts();
  bool send_config();
  bool send_validate_state();
  void toggle_active();
  void clear_active_contexts();
  bool update_active_contexts(bool force = false);
  bool send_active_contexts();
  std::optional<Socket> listen_for_control_connections();
  std::optional<Socket> accept_control_connection();
  void read_control_messages();
  void request_next_key_info();

protected:
  // server messages
  void on_execute_action_message(int triggered_action) override;
  void on_virtual_key_state_message(Key key, KeyState state) override;
  void on_next_key_info_message(const std::vector<Key>& keys, DeviceDesc device) override;

  // control messages
  void on_set_virtual_key_state_message(Key key, KeyState state) override;
  bool on_set_config_file_message(std::string filename) override;
  void on_next_key_info_requested_message() override;
  bool on_inject_input_message(const std::string& string) override;
  bool on_inject_output_message(const std::string& string) override;
  bool on_notify_message(const std::string& string) override;

  virtual void show_next_key_info(const std::string& next_key_info);

private:
  void execute_action(const Config::Action& action);

  ConfigFile m_config_file;
  std::vector<ConfigFile> m_recent_config_files;
  ServerPort m_server;
  ControlPort m_control;
  FocusedWindow m_focused_window;
  std::vector<int> m_active_contexts;
  std::vector<int> m_new_active_contexts;
  bool m_active{ true };
};

bool execute_terminal_command(const std::string& command);
