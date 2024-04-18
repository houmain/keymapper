#pragma once

#include "runtime/Stage.h"
#include "common/MessageType.h"
#include "common/Host.h"
#include <memory>

class IClientPort {
public:
  struct MessageHandler {
    virtual void on_configuration_message(std::unique_ptr<Stage> stage) = 0;
    virtual void on_active_contexts_message(const std::vector<int>& context_indices) = 0;
    virtual void on_set_virtual_key_state_message(Key key, KeyState state) = 0;
    virtual void on_validate_state_message() = 0;
    virtual void on_device_names_message() = 0;
  };

  virtual ~IClientPort() = default;
  virtual Socket socket() const = 0;
  virtual Socket listen_socket() const = 0;
  virtual bool listen() = 0;
  virtual bool accept() = 0;
  virtual void disconnect() = 0;
  virtual bool send_triggered_action(int action) = 0;
  virtual bool send_virtual_key_state(Key key, KeyState state) = 0;
  virtual bool send_device_names(const std::vector<std::string>& device_names) = 0;
  virtual bool read_messages(MessageHandler& handler, 
    std::optional<Duration> timeout) = 0;
};

class ClientPort : public IClientPort {
public:
  ClientPort();
  Socket socket() const override { return m_connection.socket(); }
  Socket listen_socket() const override { return m_host.listen_socket(); }
  bool listen() override;
  bool accept() override;
  void disconnect() override;
  bool send_triggered_action(int action) override;
  bool send_virtual_key_state(Key key, KeyState state) override;
  bool send_device_names(const std::vector<std::string>& device_names) override;
  bool read_messages(MessageHandler& handler, 
    std::optional<Duration> timeout) override;

private:
  std::unique_ptr<Stage> read_config(Deserializer& d);
  const std::vector<int>& read_active_contexts(Deserializer& d);

  Host m_host;
  Connection m_connection;
  std::vector<int> m_active_context_indices;
};
