
#include "GrabbedDevices.h"
#include "common/output.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>

namespace {
  Key HIDKeyboardUsageToKey(int usage) {
    switch (usage) {
      case kHIDUsage_KeyboardA: return Key::A;
      case kHIDUsage_KeyboardB: return Key::B;
      case kHIDUsage_KeyboardC: return Key::C;
      case kHIDUsage_KeyboardD: return Key::D;
      case kHIDUsage_KeyboardE: return Key::E;
      case kHIDUsage_KeyboardF: return Key::F;
      case kHIDUsage_KeyboardG: return Key::G;
      case kHIDUsage_KeyboardH: return Key::H;
      case kHIDUsage_KeyboardI: return Key::I;
      case kHIDUsage_KeyboardJ: return Key::J;
      case kHIDUsage_KeyboardK: return Key::K;
      case kHIDUsage_KeyboardL: return Key::L;
      case kHIDUsage_KeyboardM: return Key::M;
      case kHIDUsage_KeyboardN: return Key::N;
      case kHIDUsage_KeyboardO: return Key::O;
      case kHIDUsage_KeyboardP: return Key::P;
      case kHIDUsage_KeyboardQ: return Key::Q;
      case kHIDUsage_KeyboardR: return Key::R;
      case kHIDUsage_KeyboardS: return Key::S;
      case kHIDUsage_KeyboardT: return Key::T;
      case kHIDUsage_KeyboardU: return Key::U;
      case kHIDUsage_KeyboardV: return Key::V;
      case kHIDUsage_KeyboardW: return Key::W;
      case kHIDUsage_KeyboardX: return Key::X;
      case kHIDUsage_KeyboardY: return Key::Y;
      case kHIDUsage_KeyboardZ: return Key::Z;
      case kHIDUsage_Keyboard1: return Key::Digit1;
      case kHIDUsage_Keyboard2: return Key::Digit2;
      case kHIDUsage_Keyboard3: return Key::Digit3;
      case kHIDUsage_Keyboard4: return Key::Digit4;
      case kHIDUsage_Keyboard5: return Key::Digit5;
      case kHIDUsage_Keyboard6: return Key::Digit6;
      case kHIDUsage_Keyboard7: return Key::Digit7;
      case kHIDUsage_Keyboard8: return Key::Digit8;
      case kHIDUsage_Keyboard9: return Key::Digit9;
      case kHIDUsage_Keyboard0: return Key::Digit0;
      case kHIDUsage_KeyboardReturnOrEnter: return Key::Enter;
      case kHIDUsage_KeyboardEscape: return Key::Escape;
      case kHIDUsage_KeyboardDeleteOrBackspace: return Key::Backspace;
      case kHIDUsage_KeyboardTab: return Key::Tab;
      case kHIDUsage_KeyboardSpacebar: return Key::Space;
      case kHIDUsage_KeyboardHyphen: return Key::Minus;
      case kHIDUsage_KeyboardEqualSign: return Key::Equal;
      case kHIDUsage_KeyboardOpenBracket: return Key::BracketLeft;
      case kHIDUsage_KeyboardCloseBracket	: return Key::BracketRight;
      case kHIDUsage_KeyboardBackslash: return Key::Backslash;
      //case kHIDUsage_KeyboardNonUSPound:
      case kHIDUsage_KeyboardSemicolon: return Key::Semicolon;
      case kHIDUsage_KeyboardQuote: return Key::Quote;
      case kHIDUsage_KeyboardGraveAccentAndTilde: return Key::Backquote;
      case kHIDUsage_KeyboardComma: return Key::Comma;
      case kHIDUsage_KeyboardPeriod: return Key::Period;
      case kHIDUsage_KeyboardSlash: return Key::Slash;
      case kHIDUsage_KeyboardCapsLock: return Key::CapsLock;
      case kHIDUsage_KeyboardF1: return Key::F1;
      case kHIDUsage_KeyboardF2: return Key::F2;
      case kHIDUsage_KeyboardF3: return Key::F3;
      case kHIDUsage_KeyboardF4: return Key::F4;
      case kHIDUsage_KeyboardF5: return Key::F5;
      case kHIDUsage_KeyboardF6: return Key::F6;
      case kHIDUsage_KeyboardF7: return Key::F7;
      case kHIDUsage_KeyboardF8: return Key::F8;
      case kHIDUsage_KeyboardF9: return Key::F9;
      case kHIDUsage_KeyboardF10: return Key::F10;
      case kHIDUsage_KeyboardF11: return Key::F11;
      case kHIDUsage_KeyboardF12: return Key::F12;
      case kHIDUsage_KeyboardPrintScreen: return Key::PrintScreen;
      case kHIDUsage_KeyboardScrollLock: return Key::ScrollLock;
      case kHIDUsage_KeyboardPause: return Key::Pause;
      case kHIDUsage_KeyboardInsert: return Key::Insert;
      case kHIDUsage_KeyboardHome: return Key::Home;
      case kHIDUsage_KeyboardPageUp: return Key::PageUp;
      case kHIDUsage_KeyboardDeleteForward: return Key::Delete;
      case kHIDUsage_KeyboardEnd: return Key::End;
      case kHIDUsage_KeyboardPageDown: return Key::PageDown;
      case kHIDUsage_KeyboardRightArrow: return Key::ArrowRight;
      case kHIDUsage_KeyboardLeftArrow: return Key::ArrowLeft;
      case kHIDUsage_KeyboardDownArrow: return Key::ArrowDown;
      case kHIDUsage_KeyboardUpArrow: return Key::ArrowUp;
      case kHIDUsage_KeypadNumLock: return Key::NumLock;
      case kHIDUsage_KeypadSlash: return Key::NumpadDivide;
      case kHIDUsage_KeypadAsterisk: return Key::NumpadMultiply;
      case kHIDUsage_KeypadHyphen: return Key::NumpadSubtract;
      case kHIDUsage_KeypadPlus: return Key::NumpadAdd;
      case kHIDUsage_KeypadEnter: return Key::NumpadEnter;
      case kHIDUsage_Keypad1: return Key::Numpad1;
      case kHIDUsage_Keypad2: return Key::Numpad2;
      case kHIDUsage_Keypad3: return Key::Numpad3;
      case kHIDUsage_Keypad4: return Key::Numpad4;
      case kHIDUsage_Keypad5: return Key::Numpad5;
      case kHIDUsage_Keypad6: return Key::Numpad6;
      case kHIDUsage_Keypad7: return Key::Numpad7;
      case kHIDUsage_Keypad8: return Key::Numpad8;
      case kHIDUsage_Keypad9: return Key::Numpad9;
      case kHIDUsage_Keypad0: return Key::Numpad0;
      case kHIDUsage_KeypadPeriod: return Key::NumpadDecimal;
      case kHIDUsage_KeyboardNonUSBackslash: return Key::IntlBackslash;
      case kHIDUsage_KeyboardApplication: return Key::LaunchApp1;
      case kHIDUsage_KeyboardPower: return Key::NumpadDecimal;
      case kHIDUsage_KeypadEqualSign: return Key::Equal;
      case kHIDUsage_KeyboardF13: return Key::F13;
      case kHIDUsage_KeyboardF14: return Key::F14;
      case kHIDUsage_KeyboardF15: return Key::F15;
      case kHIDUsage_KeyboardF16: return Key::F16;
      case kHIDUsage_KeyboardF17: return Key::F17;
      case kHIDUsage_KeyboardF18: return Key::F18;
      case kHIDUsage_KeyboardF19: return Key::F19;
      case kHIDUsage_KeyboardF20: return Key::F20;
      case kHIDUsage_KeyboardF21: return Key::F21;
      case kHIDUsage_KeyboardF22: return Key::F22;
      case kHIDUsage_KeyboardF23: return Key::F23;
      case kHIDUsage_KeyboardF24: return Key::F24;
      //case kHIDUsage_KeyboardExecute
      //case kHIDUsage_KeyboardHelp:
      //case kHIDUsage_KeyboardMenu
      //case kHIDUsage_KeyboardSelect
      //case kHIDUsage_KeyboardStop
      //case kHIDUsage_KeyboardAgain
      //case kHIDUsage_KeyboardUndo
      //case kHIDUsage_KeyboardCut
      //case kHIDUsage_KeyboardCopy
      //case kHIDUsage_KeyboardPaste
      //case kHIDUsage_KeyboardFind
      case kHIDUsage_KeyboardMute: return Key::AudioVolumeMute;
      case kHIDUsage_KeyboardVolumeUp: return Key::AudioVolumeUp;
      case kHIDUsage_KeyboardVolumeDown: return Key::AudioVolumeDown;
      case kHIDUsage_KeyboardLockingCapsLock: return Key::CapsLock;
      case kHIDUsage_KeyboardLockingNumLock: return Key::NumLock;
      case kHIDUsage_KeyboardLockingScrollLock: return Key::ScrollLock;
      case kHIDUsage_KeypadComma: return Key::NumpadComma;
      case kHIDUsage_KeypadEqualSignAS400: return Key::NumpadEqual;
      //case kHIDUsage_KeyboardInternational1
      //case kHIDUsage_KeyboardInternational2
      //case kHIDUsage_KeyboardInternational3
      //case kHIDUsage_KeyboardInternational4
      //case kHIDUsage_KeyboardInternational5
      //case kHIDUsage_KeyboardInternational6
      //case kHIDUsage_KeyboardInternational7
      //case kHIDUsage_KeyboardInternational8
      //case kHIDUsage_KeyboardInternational9
      //case kHIDUsage_KeyboardLANG1
      //case kHIDUsage_KeyboardLANG2
      //case kHIDUsage_KeyboardLANG3
      //case kHIDUsage_KeyboardLANG4
      //case kHIDUsage_KeyboardLANG5
      //case kHIDUsage_KeyboardLANG6
      //case kHIDUsage_KeyboardLANG7
      //case kHIDUsage_KeyboardLANG8
      //case kHIDUsage_KeyboardLANG9
      //case kHIDUsage_KeyboardAlternateErase
      case kHIDUsage_KeyboardSysReqOrAttention: return Key::Cancel;
      case kHIDUsage_KeyboardCancel: return Key::Cancel;
      //case kHIDUsage_KeyboardClear
      //case kHIDUsage_KeyboardPrior
      //case kHIDUsage_KeyboardReturn
      //case kHIDUsage_KeyboardSeparator
      //case kHIDUsage_KeyboardOut
      //case kHIDUsage_KeyboardOper
      //case kHIDUsage_KeyboardClearOrAgain
      //case kHIDUsage_KeyboardCrSelOrProps
      //case kHIDUsage_KeyboardExSel
      case kHIDUsage_KeyboardLeftControl: return Key::ControlLeft;
      case kHIDUsage_KeyboardLeftShift: return Key::ShiftLeft;
      case kHIDUsage_KeyboardLeftAlt: return Key::AltLeft;
      case kHIDUsage_KeyboardLeftGUI: return Key::MetaLeft;
      case kHIDUsage_KeyboardRightControl: return Key::ControlRight;
      case kHIDUsage_KeyboardRightShift: return Key::ShiftRight;
      case kHIDUsage_KeyboardRightAlt: return Key::AltRight;
      case kHIDUsage_KeyboardRightGUI: return Key::MetaRight;
    }
    return Key::none;
  }

  std::string to_string(CFStringRef string) {
    const auto length = CFStringGetLength(string);
    const auto max_size= CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);  
    auto buffer = std::string(max_size, ' ');
    if (CFStringGetCString(string, buffer.data(), max_size, kCFStringEncodingUTF8))
      return buffer;
    return { };
  }

  std::string get_device_name(IOHIDDeviceRef device) {
    const auto property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    if (property)
      return to_string(static_cast<CFStringRef>(property));
    return { };
  }

  bool is_keyboard(IOHIDDeviceRef device) {
    return IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
  }

  bool is_mouse(IOHIDDeviceRef device) {
    return IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse);
  }

  bool is_supported_device(IOHIDDeviceRef device, bool grab_mice) {
    return (is_keyboard(device) || (grab_mice && is_mouse(device)));
  }

  bool can_read_from_file(int fd) {
    auto read_set = fd_set{ };
    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);
    auto timeout = timeval{ };
    return (select(fd + 1, &read_set, nullptr, nullptr, &timeout) > 0);
  }
} // namespace

//-------------------------------------------------------------------------

class GrabbedDevicesImpl {
private:
  using Event = GrabbedDevices::Event;
  using Duration = GrabbedDevices::Duration;

  IOHIDManagerRef m_hid_manager{ };
  bool m_grab_mice{ };
  bool m_devices_changed{ };
  std::vector<IOHIDDeviceRef> m_grabbed_devices;
  std::vector<std::string> m_grabbed_device_names;
  std::vector<Event> m_event_queue;
  size_t m_event_queue_pos{ };

public:
  ~GrabbedDevicesImpl() {
    ungrab_all_devices();
    IOHIDManagerClose(m_hid_manager, kIOHIDOptionsTypeNone);
  }

  bool initialize([[maybe_unused]] const char* ignore_device_name, bool grab_mice) {
    // TODO: disable mouse grabbing until forwarding is implemented
    m_grab_mice = grab_mice = false;

    m_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!m_hid_manager)
      return false;
    IOHIDManagerSetDeviceMatching(m_hid_manager, nullptr);
    IOHIDManagerRegisterDeviceMatchingCallback(m_hid_manager, &devices_changed_callback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_hid_manager, &devices_changed_callback, this);
    IOHIDManagerSetDeviceMatching(m_hid_manager, nullptr);
    IOHIDManagerScheduleWithRunLoop(m_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    update_grabbed_devices();
    return true;
  }

  std::pair<bool, std::optional<Event>> read_input_event(
      std::optional<Duration> timeout, int interrupt_fd) {      
        
    const auto timeout_at = (timeout ? 
        std::chrono::steady_clock::now() + *timeout : 
        std::chrono::steady_clock::time_point::max());

    for (;;) {
      if (m_event_queue_pos < m_event_queue.size())
        return { true, m_event_queue[m_event_queue_pos++] };

      m_event_queue.clear();
      m_event_queue_pos = 0;

      if (std::exchange(m_devices_changed, false))
        update_grabbed_devices();

      // TODO: do not poll. see https://stackoverflow.com/questions/48434976/cfsocket-data-callbacks
      auto poll_timeout = (timeout.has_value() ? timeout.value() : Duration::max());
      if (interrupt_fd >=0) {
        if (can_read_from_file(interrupt_fd))
          return { true, std::nullopt };
        poll_timeout = std::min(poll_timeout, Duration(std::chrono::milliseconds(100)));
      }

      const auto result = CFRunLoopRunInMode(
        kCFRunLoopDefaultMode, poll_timeout.count(), true);

      if (std::chrono::steady_clock::now() >= timeout_at)  
        return { true, std::nullopt };
    }
  }

  const std::vector<std::string>& grabbed_device_names() const {
    return m_grabbed_device_names;
  }

private:
  static void devices_changed_callback(void *context, IOReturn result,
      void *sender, IOHIDDeviceRef device) {
    static_cast<GrabbedDevicesImpl*>(context)->handle_devices_changed();
  }

  static void input_value_callback(void* context, IOReturn result,
      void* sender, IOHIDValueRef value) {
    const auto element = IOHIDValueGetElement(value);
    const auto page = IOHIDElementGetUsagePage(element);
    if (page == kHIDPage_KeyboardOrKeypad) {
      const auto usage = IOHIDElementGetUsage(element);
      const auto val = IOHIDValueGetIntegerValue(value);
      if (usage >= kHIDUsage_KeyboardA &&
          usage <= kHIDUsage_KeyboardRightGUI)
        static_cast<GrabbedDevicesImpl*>(context)->handle_input(usage, val);
    }
  }

  bool grab_device(IOHIDDeviceRef device) {
    verbose("Grabbing device '%s'", get_device_name(device).c_str());
    const auto result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
    if (result != kIOReturnSuccess) {
      error("Grabbing device failed");
      return false;
    }
    IOHIDDeviceRegisterInputValueCallback(device, &input_value_callback, this);
    return true;
  }

  void ungrab_device(IOHIDDeviceRef device, const std::string& device_name) {
    verbose("Ungrabbing device '%s'", device_name.c_str());
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
  }

  void ungrab_all_devices() {
    for (auto i = 0u; i < m_grabbed_devices.size(); ++i)
      ungrab_device(m_grabbed_devices[i], m_grabbed_device_names[i]);
    m_grabbed_devices.clear();
    m_grabbed_device_names.clear();
  }

  void handle_devices_changed() {
    m_devices_changed = true;
  }

  void handle_input(int code, int value) {
    m_event_queue.push_back({ 0, 0, code, value });
  }

  void update_grabbed_devices() {
    verbose("Updating device list");

    // get devices
    const auto device_set = IOHIDManagerCopyDevices(m_hid_manager);
    const auto device_count = CFSetGetCount(device_set);
    auto devices = std::vector<IOHIDDeviceRef>();
    devices.resize(device_count);
    CFSetGetValues(device_set, (const void**)devices.data());

    // update grabbed devices
    auto previously_grabbed = std::move(m_grabbed_devices);
    for (auto i = 0; i < device_count; ++i) {
      const auto device = devices[i];
      const auto it = std::find(previously_grabbed.begin(), 
        previously_grabbed.end(), device);
      if (it == previously_grabbed.end()) {
        if (is_supported_device(device, m_grab_mice) &&
            grab_device(device))
          m_grabbed_devices.push_back(device);
      }
      else {
        m_grabbed_devices.push_back(std::exchange(*it, nullptr));
      }
    }

    // ungrab previously grabbed
    for (auto i = 0u; i < previously_grabbed.size(); ++i)
      if (auto device = previously_grabbed[i])
        ungrab_device(device, m_grabbed_device_names[i]);

    m_grabbed_device_names.clear();
    for (auto device : m_grabbed_devices)
      m_grabbed_device_names.push_back(get_device_name(device));
  }
};

//-------------------------------------------------------------------------

GrabbedDevices::GrabbedDevices()
  : m_impl(std::make_unique<GrabbedDevicesImpl>()) {
}

GrabbedDevices::GrabbedDevices(GrabbedDevices&&) noexcept = default;
GrabbedDevices& GrabbedDevices::operator=(GrabbedDevices&&) noexcept = default;
GrabbedDevices::~GrabbedDevices() = default;

bool GrabbedDevices::grab(const char* ignore_device_name, bool grab_mice) {
  return m_impl->initialize(ignore_device_name, grab_mice);
}

auto GrabbedDevices::read_input_event(std::optional<Duration> timeout, int interrupt_fd)
    -> std::pair<bool, std::optional<Event>> {
  return m_impl->read_input_event(timeout, interrupt_fd);
}

const std::vector<std::string>& GrabbedDevices::grabbed_device_names() const {
  return m_impl->grabbed_device_names();
}

std::optional<KeyEvent> to_key_event(const GrabbedDevices::Event& event) {    
  return KeyEvent{
    HIDKeyboardUsageToKey(event.code),
    (event.value == 0 ? KeyState::Up : KeyState::Down),
  };
}
