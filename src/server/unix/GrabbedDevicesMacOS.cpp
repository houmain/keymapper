
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
  const char* m_ignore_device_name{ };
  bool m_grab_mice{ };
  bool m_devices_changed{ };
  std::vector<IOHIDDeviceRef> m_grabbed_devices;
  std::vector<std::string> m_grabbed_device_names;
  std::vector<Event> m_event_queue;
  size_t m_event_queue_pos{ };

public:
  ~GrabbedDevicesImpl() {
    ungrab_all_devices();
    if (m_hid_manager)
      IOHIDManagerClose(m_hid_manager, kIOHIDOptionsTypeNone);
  }

  bool initialize([[maybe_unused]] const char* ignore_device_name, bool grab_mice) {
    m_ignore_device_name = "Karabiner";
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
    const auto name = get_device_name(device);
    if (name.find(m_ignore_device_name) != std::string::npos)
      return false;

    verbose("Grabbing device '%s'", name.c_str());
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
    static_cast<Key>(event.code),
    (event.value == 0 ? KeyState::Up : KeyState::Down),
  };
}
