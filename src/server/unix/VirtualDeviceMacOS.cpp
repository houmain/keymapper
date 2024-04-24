
#include "VirtualDevice.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <atomic>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>
#include <IOKit/hid/IOHIDManager.h>

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
  virtual_hid_device_driver::hid_report::consumer_input m_consumer;
  virtual_hid_device_driver::hid_report::generic_desktop_input m_desktop;
  bool m_fn_key_hold{ };

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

    // TODO: FN keys are currently hardcoded for my device, find out how to map correctly
    if (!m_fn_key_hold && event.key == Key::F6) {
      const auto key = kHIDUsage_GD_DoNotDisturb;
      if (event.state == KeyState::Down)
        m_desktop.keys.insert(key);
      else
        m_desktop.keys.erase(key);

      m_client.async_post_report(m_desktop);
    }
    else if (!m_fn_key_hold && event.key >= Key::F1 && event.key <= Key::F12) {
      const auto key = [&]() {
        switch (event.key) {
          default:
          case Key::F1: return kHIDUsage_Csmr_DisplayBrightnessDecrement;
          case Key::F2: return kHIDUsage_Csmr_DisplayBrightnessIncrement;
          case Key::F3: return kHIDUsage_Csmr_ACDesktopShowAllWindows;
          case Key::F4: return kHIDUsage_Csmr_ACSearch;
          case Key::F5: return kHIDUsage_Csmr_VoiceCommand;
          case Key::F7: return kHIDUsage_Csmr_ScanPreviousTrack;
          case Key::F8: return kHIDUsage_Csmr_PlayOrPause;
          case Key::F9: return kHIDUsage_Csmr_ScanNextTrack;
          case Key::F10: return kHIDUsage_Csmr_Mute;
          case Key::F11: return kHIDUsage_Csmr_VolumeDecrement;
          case Key::F12: return kHIDUsage_Csmr_VolumeIncrement;
        }
      }();
      if (event.state == KeyState::Down)
        m_consumer.keys.insert(key);
      else
        m_consumer.keys.erase(key);

      m_client.async_post_report(m_consumer);
    }
    else {
      const auto key = static_cast<uint16_t>(event.key);
      if (event.state == KeyState::Down)
        m_keyboard.keys.insert(key);
      else
        m_keyboard.keys.erase(key);

      m_client.async_post_report(m_keyboard);
    }
    return true;
  }

  bool flush() {
    return true;
  }

  bool send_event(int page, int usage, int value) {
    if (page == kHIDPage_KeyboardOrKeypad &&
        (usage < kHIDUsage_KeyboardA ||
         usage > kHIDUsage_KeyboardRightGUI))
      return false;

  #if !defined(NDEBUG)
    verbose("PAGE: %04x, USAGE: %04x, VALUE: %04x", page, usage, value);
  #endif

    if (page == 0xFF) {
      m_fn_key_hold = (value != 0);
    }
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
  return (m_impl && m_impl->send_event(type, code, value));
}

bool VirtualDevice::flush() {
  return (m_impl && m_impl->flush());
}
