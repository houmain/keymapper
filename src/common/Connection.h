#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <optional>
#include <type_traits>
#include "Duration.h"

#if defined(_WIN32)
using Socket = uintptr_t;
#else
using Socket = int;
#endif

constexpr Socket invalid_socket = ~Socket{ };

bool block_until_readable(Socket socket, std::optional<Duration> timeout);

class Serializer {
public:
  void write(const void* data, size_t size) {
    const auto offset = buffer.size();
    buffer.resize(buffer.size() + size);
    std::memcpy(buffer.data() + offset, data, size);
  }

  template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  void write(const T& value) {
    write(&value, sizeof(T));
  }

  void write(std::string_view string) {
    write(static_cast<uint32_t>(string.size()));
    write(string.data(), string.size());
  }

private:
  friend class Connection;
  std::vector<char> buffer;
};

class Deserializer {
public:
  void read(void* data, size_t size) {
    if (size && can_read(size)) {
      std::memcpy(data, &*it, size);
      it += size;
    }
  }

  template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  T read() {
    auto result = T{ };
    read(&result, sizeof(T));
    return result;
  }

  template<typename T, typename = std::enable_if_t<std::is_trivially_copyable_v<T>>>
  void read(T* data) {
    read(data, sizeof(T));
  }

  std::string read_string() {
    const auto size = read<uint32_t>();
    auto result = std::string(size, ' ');
    read(result.data(), size);
    return result;
  }

  bool can_read(size_t length) const { 
    return (it - buffer.begin() + length <= buffer.size()); 
  }

private:
  friend class Connection;
  std::vector<char> buffer;
  std::vector<char>::iterator it;
};

class Connection {
  using Size = uint32_t;
public:
  Connection() = default;
  explicit Connection(Socket socket);
  Connection(Connection&& rhs);
  Connection& operator=(Connection&& rhs);
  ~Connection();

  Socket socket() const { return m_socket_fd; }
  explicit operator bool() const { return m_socket_fd != invalid_socket; }
  void disconnect();
  bool send(const char* buffer, size_t length);
  int recv(char* buffer, size_t length);

  template<typename F> // void(Serializer&)
  bool send_message(F&& write_message) {
    // serialize messages to buffer
    auto& buffer = m_serializer.buffer;
    buffer.clear();
    write_message(m_serializer);

    // send message size and buffer
    auto size = static_cast<Size>(buffer.size());
    return send(reinterpret_cast<char*>(&size), sizeof(size)) &&
           send(buffer.data(), buffer.size());
  }

  template<typename F> // void(Deserializer&)
  bool read_messages(std::optional<Duration> timeout, F&& deserialize) {
    // block until message can be read or timeout
    if (timeout != Duration::zero() &&
        !wait_for_message(timeout))
      return false;

    // read into buffer until it would block
    auto& buffer = m_deserializer.buffer;
    if (!recv(buffer))
      return false;

    // deserialize complete messages
    m_deserializer.it = buffer.begin();
    while (m_deserializer.can_read(sizeof(Size))) {
      const auto size = m_deserializer.read<Size>();
      if (!m_deserializer.can_read(size)) {
        m_deserializer.it -= sizeof(Size);
        break;
      }
      const auto end = m_deserializer.it + size;
      deserialize(m_deserializer);
      if (m_deserializer.it != end)
        return false;
    }
    buffer.erase(buffer.begin(), m_deserializer.it);
    return true;
  }

private:
  bool wait_for_message(std::optional<Duration> timeout);
  bool recv(std::vector<char>& buffer);

  Socket m_socket_fd{ invalid_socket };
  Serializer m_serializer;
  Deserializer m_deserializer;
};
