
#include "VirtualDevices.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <atomic>
#include <optional>
#include <pqrs/karabiner/driverkit/virtual_hid_device_driver.hpp>
#include <pqrs/karabiner/driverkit/virtual_hid_device_service.hpp>
#include <IOKit/hid/IOHIDManager.h>

using namespace pqrs::karabiner::driverkit;

bool macos_toggle_fn = false;

class VirtualDevicesImpl {
private:
  enum class State : int { initializing, connected, disconnected };
  std::atomic<State> m_state{ };
  std::optional<virtual_hid_device_service::client> m_client;
  virtual_hid_device_driver::hid_report::keyboard_input m_keyboard;
  virtual_hid_device_driver::hid_report::consumer_input m_consumer;
  virtual_hid_device_driver::hid_report::generic_desktop_input m_desktop;
  virtual_hid_device_driver::hid_report::apple_vendor_top_case_input m_top_case_input;
  bool m_fn_key_hold{ };
  std::vector<Key> m_hold_function_keys;

  template<typename Report>
  void toggle_key(Report& report, uint16_t usage, bool down) {
    if (down) {
      report.keys.insert(usage);
    }
    else {
      report.keys.erase(usage);
    }
    m_client->async_post_report(report);
  }

public:
  VirtualDevicesImpl() {
    pqrs::dispatcher::extra::initialize_shared_dispatcher();
  }

  ~VirtualDevicesImpl() {
    close();
    pqrs::dispatcher::extra::terminate_shared_dispatcher();
  }

  bool create() {
    verbose("Creating virtual keyboard device '%s'", VirtualDevices::name);
    auto& client = m_client.emplace();

    client.connected.connect([this] {
      verbose("Karabiner connected");
      auto parameters = virtual_hid_device_service::virtual_hid_keyboard_parameters();
      parameters.set_vendor_id(pqrs::hid::vendor_id::value_t(VirtualDevices::vendor_id));
      parameters.set_product_id(pqrs::hid::product_id::value_t(VirtualDevices::product_id));
      parameters.set_country_code(pqrs::hid::country_code::us);
      m_client->async_virtual_hid_keyboard_initialize(parameters);
    });
    client.warning_reported.connect([](const std::string& message) {
      verbose("Karabiner warning: %s", message.c_str());
    });
    client.connect_failed.connect([this](const asio::error_code& error_code) {
      verbose("Karabiner connect failed: %d", error_code);
      m_state.store(State::disconnected);
    });
    client.closed.connect([this] {
      verbose("Karabiner closed");
      m_state.store(State::disconnected);
    });
    client.error_occurred.connect([this](const asio::error_code& error_code) {
      error("Karabiner error occurred: %d", error_code);
      m_state.store(State::disconnected);
    });
    client.driver_version_mismatched.connect([this](bool driver_version_mismatched) {
      if (driver_version_mismatched) {
        error("Karabiner driver version mismatched");
        m_state.store(State::disconnected);
      }
    });
    client.virtual_hid_keyboard_ready.connect([this](bool ready) {
      if (m_state.load() == State::initializing && ready)
        m_state.store(State::connected);
    });
  
    client.async_start();
    for (auto i = 0; (m_state.load() == State::initializing) && i < 30; i++)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return (m_state.load() == State::connected);
  }

  void close() {
    if (m_client) {
      verbose("Destroying virtual device");
      m_client->async_stop();
      for (auto i = 0; (m_state.load() == State::connected) && i < 20; i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    m_client.reset();
  }

  void send_key_event(const KeyEvent& event) {
    if (m_state.load() != State::connected)
      return;

    const auto down = (event.state == KeyState::Down);

    const auto is_function_key = [](Key key) {
      return ((key >= Key::F1 && key <= Key::F12) ||
              (key >= Key::ArrowRight && key <= Key::ArrowUp) ||
              key == Key::Backspace);
    };

    const auto toggle_function_key = [&]() {
      // when it is pressed with FN, then also release it with FN
      if (down) {
        if (macos_toggle_fn == m_fn_key_hold) {
          m_hold_function_keys.push_back(event.key);
          return true;
        }
      }
      else {
        const auto it = std::find(m_hold_function_keys.begin(),
          m_hold_function_keys.end(), event.key);
        if (it != m_hold_function_keys.end()) {
          m_hold_function_keys.erase(it);
          return true;
        }
      }
      return false;
    };

    // TODO: FN keys are currently hardcoded for my device, find out how to map correctly
    if (is_function_key(event.key) && toggle_function_key())
      switch (event.key) {
        case Key::F1: return toggle_key(m_consumer, kHIDUsage_Csmr_DisplayBrightnessDecrement, down);
        case Key::F2: return toggle_key(m_consumer, kHIDUsage_Csmr_DisplayBrightnessIncrement, down);
        case Key::F3: return toggle_key(m_consumer, kHIDUsage_Csmr_ACDesktopShowAllWindows, down);
        case Key::F4: return toggle_key(m_consumer, kHIDUsage_Csmr_ACSearch, down);
        case Key::F5: return toggle_key(m_consumer, kHIDUsage_Csmr_VoiceCommand, down);
        case Key::F6: return toggle_key(m_desktop, kHIDUsage_GD_DoNotDisturb, down);
        case Key::F7: return toggle_key(m_consumer, kHIDUsage_Csmr_ScanPreviousTrack, down);
        case Key::F8: return toggle_key(m_consumer, kHIDUsage_Csmr_PlayOrPause, down);
        case Key::F9: return toggle_key(m_consumer, kHIDUsage_Csmr_ScanNextTrack, down);
        case Key::F10: return toggle_key(m_consumer, kHIDUsage_Csmr_Mute, down);
        case Key::F11: return toggle_key(m_consumer, kHIDUsage_Csmr_VolumeDecrement, down);
        case Key::F12: return toggle_key(m_consumer, kHIDUsage_Csmr_VolumeIncrement, down);
        case Key::ArrowRight: return toggle_key(m_keyboard, *Key::End, down);
        case Key::ArrowLeft: return toggle_key(m_keyboard, *Key::Home, down);
        case Key::ArrowDown: return toggle_key(m_keyboard, *Key::PageDown, down);
        case Key::ArrowUp: return toggle_key(m_keyboard, *Key::PageUp, down);
        case Key::Backspace: return toggle_key(m_keyboard, *Key::Delete, down);
        default: break;
      }

    toggle_key(m_keyboard, *event.key, down);
  }

  bool flush() {
    return true;
  }

  bool send_event(int page, int usage, int value) {
    const auto down = (value != 0);
    switch (page) {
      case kHIDPage_KeyboardOrKeypad:
        return true;

      case kHIDPage_Consumer:
        toggle_key(m_consumer, usage, down);
        return true;

      case 0xFF:
        toggle_key(m_top_case_input, usage, down);
        m_fn_key_hold = down;
        return true;
    }
  #if !defined(NDEBUG)
    verbose("PAGE: %04x, USAGE: %04x, VALUE: %04x", page, usage, value);
  #endif
    return true;
  }
};

//-------------------------------------------------------------------------

VirtualDevices::VirtualDevices() = default;
VirtualDevices::VirtualDevices(VirtualDevices&&) noexcept = default;
VirtualDevices& VirtualDevices::operator=(VirtualDevices&&) noexcept = default;
VirtualDevices::~VirtualDevices() = default;

bool VirtualDevices::create_keyboard_device() {
  m_impl.reset();
  auto impl = std::make_unique<VirtualDevicesImpl>();
  if (!impl->create())
    return false;
  m_impl = std::move(impl);
  return true;
}

bool VirtualDevices::update_forward_devices(const std::vector<DeviceDesc>& device_descs) {
  return true;
}

bool VirtualDevices::send_key_event(const KeyEvent& event) {
  if (!m_impl)
    return false;
  m_impl->send_key_event(event);
  return true;
}

bool VirtualDevices::forward_event(int device_index, int type, int code, int value) {
  return (m_impl && m_impl->send_event(type, code, value));
}

bool VirtualDevices::flush() {
  return (m_impl && m_impl->flush());
}
