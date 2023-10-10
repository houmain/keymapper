
#include "VirtualDevice.h"
#include "runtime/KeyEvent.h"
#include <CoreGraphics/CGEvent.h>
#include <CoreGraphics/CGEventSource.h>

class VirtualDeviceImpl {
private:
  CGEventSourceRef m_event_source{ };  
  CGEventFlags m_left_modifiers_down{ };
  CGEventFlags m_right_modifiers_down{ };

public:
  ~VirtualDeviceImpl() {
    if (m_event_source)
      CFRelease(m_event_source);
  }

  bool create([[maybe_unused]] const char* name) {
    if (m_event_source)
      return false;
    m_event_source = CGEventSourceCreate(
      kCGEventSourceStateHIDSystemState);
    return (m_event_source != nullptr);
  }

  CGEventFlags get_left_modifier(const KeyEvent& event) {
    switch (event.key) {
      case Key::AltLeft: return kCGEventFlagMaskAlternate;
      case Key::ControlLeft: return kCGEventFlagMaskControl;
      case Key::MetaLeft: return kCGEventFlagMaskCommand;
      case Key::ShiftLeft: return kCGEventFlagMaskShift;
      default: return { };
    }
  }

  CGEventFlags get_right_modifier(const KeyEvent& event) {
    switch (event.key) {
      case Key::AltRight: return kCGEventFlagMaskAlternate;
      case Key::ControlRight: return kCGEventFlagMaskControl;
      case Key::MetaRight: return kCGEventFlagMaskCommand;
      case Key::ShiftRight: return kCGEventFlagMaskShift;
      default: return { };
    }
  }  

  bool send_key_event(const KeyEvent& event) {
    // special handling since 0 is reserved for Key::none
    const auto key = 
      (event.key == Key::A ? CGKeyCode{ 0 } :
       static_cast<CGKeyCode>(event.key));
    const auto down = (event.state == KeyState::Down);

    const auto event_ref = CGEventCreateKeyboardEvent(m_event_source, key, down);
    const auto left_modifier = get_left_modifier(event);
    const auto right_modifier = get_right_modifier(event);
    if (left_modifier || right_modifier) {
      if (down) {
        m_left_modifiers_down |= left_modifier;
        m_right_modifiers_down |= right_modifier;
      }
      else {
        m_left_modifiers_down &= ~left_modifier;
        m_right_modifiers_down &= ~right_modifier;
      }
    }
    else {
      CGEventSetFlags(event_ref, m_left_modifiers_down | m_right_modifiers_down);
    }
    CGEventPost(kCGHIDEventTap, event_ref);
    CFRelease(event_ref);
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
