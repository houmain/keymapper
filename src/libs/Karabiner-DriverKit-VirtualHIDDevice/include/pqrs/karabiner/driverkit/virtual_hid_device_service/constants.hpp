#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include <string_view>
#include <filesystem>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
namespace constants {
inline std::filesystem::path get_rootonly_directory(void) {
  return "/Library/Application Support/org.pqrs/tmp/rootonly";
}

inline std::filesystem::path get_server_socket_directory_path(void) {
  // Note:
  // The socket file path length must be <= 103 because sizeof(sockaddr_un.sun_path) == 104.
  // So we use the shorten name virtual_hid_device_service_server => vhidd_server.

  return get_rootonly_directory() / "vhidd_server";
}

inline std::filesystem::path get_server_response_socket_directory_path(void) {
  // Note:
  // The socket file path length must be <= 103 because sizeof(sockaddr_un.sun_path) == 104.
  // So we use the shorten name virtual_hid_device_service_server_response => vhidd_response.

  return get_rootonly_directory() / "vhidd_response";
}

inline std::filesystem::path get_client_socket_directory_path(void) {
  // Note:
  // The socket file path length must be <= 103 because sizeof(sockaddr_un.sun_path) == 104.
  // So we use the shorten name virtual_hid_device_service_client => vhidd_client.

  return get_rootonly_directory() / "vhidd_client";
}

constexpr std::size_t local_datagram_buffer_size = 1024;
} // namespace constants
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
