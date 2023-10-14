#pragma once

// (C) Copyright Takayama Fumihiko 2020.
// Distributed under the Boost Software License, Version 1.0.
// (See https://www.boost.org/LICENSE_1_0.txt)

#include "constants.hpp"
#include "request.hpp"
#include "response.hpp"
#include <glob/glob.hpp>
#include <pqrs/dispatcher.hpp>
#include <pqrs/hid.hpp>
#include <pqrs/local_datagram.hpp>
#include <sstream>

namespace pqrs {
namespace karabiner {
namespace driverkit {
namespace virtual_hid_device_service {
class client final : public dispatcher::extra::dispatcher_client {
public:
  // Signals (invoked from the dispatcher thread)

  nod::signal<void(const std::string&)> warning_reported;
  nod::signal<void(void)> connected;
  nod::signal<void(const asio::error_code&)> connect_failed;
  nod::signal<void(void)> closed;
  nod::signal<void(const asio::error_code&)> error_occurred;
  nod::signal<void(bool)> driver_activated;
  nod::signal<void(bool)> driver_connected;
  nod::signal<void(bool)> driver_version_mismatched;
  nod::signal<void(bool)> virtual_hid_keyboard_ready;
  nod::signal<void(bool)> virtual_hid_pointing_ready;

  // Methods

  client(void)
      : dispatcher_client() {
  }

  virtual ~client(void) {
    detach_from_dispatcher([this] {
      client_ = nullptr;
    });
  }

  void async_start(void) {
    enqueue_to_dispatcher([this] {
      if (client_) {
        return;
      }

      create_client();

      if (client_) {
        client_->async_start();
      }
    });
  }

  void async_stop(void) {
    enqueue_to_dispatcher([this] {
      client_ = nullptr;

      clear_state();
    });
  }

  void async_virtual_hid_keyboard_initialize(hid::country_code::value_t country_code,
                                             bool force = false) {
    if (!force) {
      if (last_virtual_hid_keyboard_ready_ == true &&
          last_virtual_hid_keyboard_initialize_country_code_ == country_code) {
        return;
      }
    }

    last_virtual_hid_keyboard_initialize_country_code_ = country_code;

    async_send(request::virtual_hid_keyboard_initialize,
               country_code);
  }

  void async_virtual_hid_keyboard_terminate(void) {
    async_send(request::virtual_hid_keyboard_terminate);
  }

  void async_virtual_hid_keyboard_reset(void) {
    async_send(request::virtual_hid_keyboard_reset);
  }

  void async_virtual_hid_pointing_initialize(bool force = false) {
    if (!force) {
      if (last_virtual_hid_pointing_ready_ == true) {
        return;
      }
    }

    async_send(request::virtual_hid_pointing_initialize);
  }

  void async_virtual_hid_pointing_terminate(void) {
    async_send(request::virtual_hid_pointing_terminate);
  }

  void async_virtual_hid_pointing_reset(void) {
    async_send(request::virtual_hid_pointing_reset);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::keyboard_input& report) {
    async_send(request::post_keyboard_input_report, report);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::consumer_input& report) {
    async_send(request::post_consumer_input_report, report);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::apple_vendor_keyboard_input& report) {
    async_send(request::post_apple_vendor_keyboard_input_report, report);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::apple_vendor_top_case_input& report) {
    async_send(request::post_apple_vendor_top_case_input_report, report);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::generic_desktop_input& report) {
    async_send(request::post_generic_desktop_input_report, report);
  }

  void async_post_report(const virtual_hid_device_driver::hid_report::pointing_input& report) {
    async_send(request::post_pointing_input_report, report);
  }

private:
  std::string client_socket_file_path(void) const {
    while (true) {
      auto now = std::chrono::system_clock::now();
      auto duration = now.time_since_epoch();

      std::stringstream ss;
      ss << pqrs::karabiner::driverkit::virtual_hid_device_service::constants::get_client_socket_directory_path().string()
         << "/"
         << std::hex
         << std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count()
         << ".sock";

      auto path = ss.str();
      std::error_code ec;
      if (!std::filesystem::exists(path, ec)) {
        return path;
      }
    }
  }

  std::filesystem::path find_server_socket_file_path(void) const {
    auto pattern = (pqrs::karabiner::driverkit::virtual_hid_device_service::constants::get_server_socket_directory_path() / "*.sock").string();
    auto paths = glob::glob(pattern);
    std::sort(std::begin(paths), std::end(paths));

    if (!paths.empty()) {
      return paths.back();
    }

    return pqrs::karabiner::driverkit::virtual_hid_device_service::constants::get_server_socket_directory_path() / "not_found.sock";
  }

  void clear_state(void) {
    last_virtual_hid_keyboard_ready_ = std::nullopt;
    virtual_hid_keyboard_ready(false);

    last_virtual_hid_pointing_ready_ = std::nullopt;
    virtual_hid_pointing_ready(false);

    last_virtual_hid_keyboard_initialize_country_code_ = std::nullopt;
  }

  void create_client(void) {
    client_ = std::make_unique<local_datagram::client>(weak_dispatcher_,
                                                       find_server_socket_file_path(),
                                                       client_socket_file_path(),
                                                       constants::local_datagram_buffer_size);
    client_->set_server_check_interval(std::chrono::milliseconds(3000));
    client_->set_reconnect_interval(std::chrono::milliseconds(1000));
    client_->set_server_socket_file_path_resolver([this] {
      return find_server_socket_file_path();
    });

    client_->warning_reported.connect([this](auto&& message) {
      enqueue_to_dispatcher([this, message] {
        warning_reported(message);
      });
    });

    client_->connected.connect([this] {
      enqueue_to_dispatcher([this] {
        connected();
      });
    });

    client_->connect_failed.connect([this](auto&& error_code) {
      enqueue_to_dispatcher([this, error_code] {
        connect_failed(error_code);
      });
    });

    client_->closed.connect([this] {
      enqueue_to_dispatcher([this] {
        closed();

        clear_state();
      });
    });

    client_->error_occurred.connect([this](auto&& error_code) {
      enqueue_to_dispatcher([this, error_code] {
        error_occurred(error_code);
      });
    });

    client_->next_heartbeat_deadline_exceeded.connect([this](auto&& sender_endpoint) {
      enqueue_to_dispatcher([this] {
        if (client_) {
          async_stop();
          async_start();
        }
      });
    });

    client_->received.connect([this](auto&& buffer, auto&& sender_endpoint) {
      if (buffer) {
        if (buffer->empty()) {
          return;
        }

        auto p = &((*buffer)[0]);
        auto size = buffer->size();

        auto r = response(*p);
        ++p;
        --size;

        switch (r) {
          case response::none:
            break;

          case response::driver_activated:
            if (size == 1) {
              driver_activated(*p);
            }
            break;

          case response::driver_connected:
            if (size == 1) {
              driver_connected(*p);
            }
            break;

          case response::driver_version_mismatched:
            if (size == 1) {
              driver_version_mismatched(*p);
            }
            break;

          case response::virtual_hid_keyboard_ready:
            if (size == 1) {
              last_virtual_hid_keyboard_ready_ = *p;
              virtual_hid_keyboard_ready(*p);
            }
            break;

          case response::virtual_hid_pointing_ready:
            if (size == 1) {
              last_virtual_hid_pointing_ready_ = *p;
              virtual_hid_pointing_ready(*p);
            }
            break;
        }
      }
    });
  }

  void async_send(request r) {
    enqueue_to_dispatcher([this, r] {
      if (client_) {
        std::vector<uint8_t> buffer;
        append_data(buffer, 'c');
        append_data(buffer, 'p');
        append_data(buffer, client_protocol_version::embedded_client_protocol_version);
        append_data(buffer, r);
        client_->async_send(buffer);
      }
    });
  }

  template <typename T>
  void async_send(request r, const T& data) {
    enqueue_to_dispatcher([this, r, data] {
      if (client_) {
        std::vector<uint8_t> buffer;
        append_data(buffer, 'c');
        append_data(buffer, 'p');
        append_data(buffer, client_protocol_version::embedded_client_protocol_version);
        append_data(buffer, r);
        append_data(buffer, data);
        client_->async_send(buffer);
      }
    });
  }

  template <typename T>
  void append_data(std::vector<uint8_t>& buffer, const T& data) const {
    auto size = buffer.size();
    buffer.resize(size + sizeof(data));
    *(reinterpret_cast<T*>(&(buffer[size]))) = data;
  }

  std::unique_ptr<local_datagram::client> client_;

  std::optional<bool> last_virtual_hid_keyboard_ready_;
  std::optional<bool> last_virtual_hid_pointing_ready_;

  std::optional<hid::country_code::value_t> last_virtual_hid_keyboard_initialize_country_code_;
};
} // namespace virtual_hid_device_service
} // namespace driverkit
} // namespace karabiner
} // namespace pqrs
