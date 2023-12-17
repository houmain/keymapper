
#include "VirtualDevice.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <atomic>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>

using namespace pqrs::karabiner::driverkit;

namespace {
  int g_shared_dispatcher_initialized;
} // namespace

class VirtualDeviceImpl {
private:
  enum class State : int { initializing, connected, disconnected };
  std::atomic<State> m_state{ };
  virtual_hid_device_service::client m_client;
  virtual_hid_device_driver::hid_report::keyboard_input m_keyboard;

public:
  bool create([[maybe_unused]] const char* name) {
    m_client.connected.connect([this] {
      verbose("Karabiner connected");
      m_client.async_virtual_hid_keyboard_initialize(pqrs::hid::country_code::us);
    });
    m_client.warning_reported.connect([](const std::string& message) {
      verbose("Karabiner warning: %s", message.c_str());
    });
    m_client.connect_failed.connect([this](const asio::error_code& error_code) {
      verbose("Karabiner connect failed: %d", error_code);
      m_state.store(State::disconnected);
    });
    m_client.closed.connect([this] {
      verbose("Karabiner closed");
      m_state.store(State::disconnected);
    });
    m_client.error_occurred.connect([this](const asio::error_code& error_code) {
      error("Karabiner error occurred: %d", error_code);
      m_state.store(State::disconnected);
    });
    m_client.driver_version_mismatched.connect([this](bool driver_version_mismatched) {
      if (driver_version_mismatched) {
        error("Karabiner driver version mismatched");
        m_state.store(State::disconnected);
      }
    });
    m_client.virtual_hid_keyboard_ready.connect([this](bool ready) {
      if (m_state.load() == State::initializing && ready)
        m_state.store(State::connected);
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

VirtualDevice::VirtualDevice() {
  if (g_shared_dispatcher_initialized++ == 0)
    pqrs::dispatcher::extra::initialize_shared_dispatcher();    
}

VirtualDevice::VirtualDevice(VirtualDevice&&) noexcept = default;
VirtualDevice& VirtualDevice::operator=(VirtualDevice&&) noexcept = default;

VirtualDevice::~VirtualDevice() {
  if (--g_shared_dispatcher_initialized == 0)
    pqrs::dispatcher::extra::terminate_shared_dispatcher();
}

bool VirtualDevice::create(const char* name) {
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
