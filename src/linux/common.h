#pragma once

#include <cstdarg>
#include <cstddef>
#include <type_traits>

struct timeval;

extern bool g_verbose_output;

void error(const char* format, ...);
void verbose(const char* format, ...);

bool write_all(int fd, const char* buffer, size_t length);

template<typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
bool send(int fd, const T& value) {
  return write_all(fd, reinterpret_cast<const char*>(&value), sizeof(T));
}

bool select(int fd, int timeout_ms);
bool read_all(int fd, char* buffer, size_t length);

template<typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
bool read(int fd, T* value) {
  return read_all(fd, reinterpret_cast<char*>(value), sizeof(T));
}
