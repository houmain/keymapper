
#include "VirtualDevice.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <atomic>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>

using namespace pqrs::karabiner::driverkit;

namespace {
  void static_init_pqrs_dispatcher() {
    static struct static_init {
      static_init() {
        pqrs::dispatcher::extra::initialize_shared_dispatcher();
      }
      ~static_init() {
        pqrs::dispatcher::extra::terminate_shared_dispatcher();
      }
    } s_static_init;
  }
} // namespace

class VirtualDeviceImpl {
private:
  enum class State : int { initializing, connected, disconnected };
  std::atomic<State> m_state{ };
  virtual_hid_device_service::client m_client;
  virtual_hid_device_driver::hid_report::keyboard_input m_keyboard;

public:
  ~VirtualDeviceImpl() {
    m_client.async_stop();
  }

  bool create([[maybe_unused]] const char* name) {
    m_client.connected.connect([this] {
      verbose("karabiner: connected");
      m_client.async_virtual_hid_keyboard_initialize(pqrs::hid::country_code::us);
    });
    m_client.warning_reported.connect([](const std::string& message) {
      verbose("karabiner: warning %s", message.c_str());
    });
    m_client.connect_failed.connect([this](const asio::error_code& error_code) {
      verbose("karabiner: connect_failed %d", error_code);
      m_state.store(State::disconnected);
    });
    m_client.closed.connect([this] {
      verbose("karabiner: closed");
      m_state.store(State::disconnected);
    });
    m_client.error_occurred.connect([this](const asio::error_code& error_code) {
      error("karabiner: error_occurred %d", error_code);
      m_state.store(State::disconnected);
    });
    m_client.virtual_hid_keyboard_ready_response.connect([this](bool ready) {
      if (m_state.load() == State::initializing) {
        if (ready)
          m_state.store(State::connected);
      }
      else if (!ready) {
        m_state.store(State::disconnected);
        error("karabiner: virtual_hid_keyboard_ready = %d", ready);
      }
    });
  
    m_client.async_start();
    for (auto i = 0; (m_state.load() == State::initializing) && i < 30; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return (m_state.load() == State::connected);
  }

  bool send_key_event(const KeyEvent& event) {
    if (m_state.load() != State::connected)
      return false;

    const auto key = static_cast<uint16_t>(event.key);
    if (event.state == KeyState::Down)
      m_keyboard.keys.insert(key);
    else
      m_keyboard.keys.erase(key);
    
    m_client.async_post_report(m_keyboard);
    return true;
  }

  bool flush() {
    return true;
  }
};

//-------------------------------------------------------------------------

VirtualDevice::VirtualDevice() = default;
VirtualDevice::VirtualDevice(VirtualDevice&&) noexcept = default;
VirtualDevice& VirtualDevice::operator=(VirtualDevice&&) noexcept = default;
VirtualDevice::~VirtualDevice() = default;

bool VirtualDevice::create(const char* name) {
  static_init_pqrs_dispatcher();

  m_impl.reset();
  auto impl = std::make_unique<VirtualDeviceImpl>();
  if (!impl->create(name))
    return false;
  m_impl = std::move(impl);
  return true;
}

bool VirtualDevice::send_key_event(const KeyEvent& event) {
  return (m_impl && m_impl->send_key_event(event));
}

bool VirtualDevice::send_event(int type, int code, int value) {
  return false;
}

bool VirtualDevice::flush() {
  return (m_impl && m_impl->flush());
}
