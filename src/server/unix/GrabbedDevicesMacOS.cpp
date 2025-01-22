
#include "GrabbedDevices.h"
#include "VirtualDevices.h"
#include "common/output.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>

bool macos_iso_keyboard = false;

namespace {
  std::string to_string(CFStringRef string) {
    if (!string)
      return { };
    const auto length = CFStringGetLength(string);
    const auto max_size = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    auto buffer = std::string(max_size, '\0');
    if (CFStringGetCString(string, buffer.data(), max_size, kCFStringEncodingUTF8))
      return buffer.c_str();
    return { };
  }

  std::string to_string(CFNumberRef number) {
    if (!number)
      return { };
    auto value = long{ };
    if (CFNumberGetValue(number, CFNumberType::kCFNumberLongType, &value))
      return std::to_string(value);
    return { };
  }

  long to_long(CFNumberRef number) {
    if (!number)
      return { };
    auto value = long{ };
    if (CFNumberGetValue(number, CFNumberType::kCFNumberLongType, &value))
      return value;
    return { };
  }

  std::string get_device_name(IOHIDDeviceRef device) {
    const auto property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
    return to_string(static_cast<CFStringRef>(property));
  }

  std::string get_device_serial(IOHIDDeviceRef device) {
    const auto property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDSerialNumberKey));
    return to_string(static_cast<CFStringRef>(property));
  }

  bool is_builtin_device(IOHIDDeviceRef device) {
    const auto property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDBuiltInKey));
    return (to_string(static_cast<CFNumberRef>(property)) == "1");
  }

  std::string get_device_number(IOHIDDeviceRef device, CFStringRef key) {
    const auto property = IOHIDDeviceGetProperty(device, key);
    return to_string(static_cast<CFNumberRef>(property));
  }

  long get_device_long(IOHIDDeviceRef device, CFStringRef key) {
    const auto property = IOHIDDeviceGetProperty(device, key);
    return to_long(static_cast<CFNumberRef>(property));
  }

  std::string get_device_id(IOHIDDeviceRef device) {
    auto serial = get_device_serial(device);
    if (!serial.empty())
      return serial;
    return std::string(is_builtin_device(device) ? "builtin" : "external") +
      ":" + get_device_number(device, CFSTR(kIOHIDPrimaryUsagePageKey)) +
      ":" + get_device_number(device, CFSTR(kIOHIDPrimaryUsageKey));
  }

  long get_device_vendor_id(IOHIDDeviceRef device) {
    return get_device_long(device, CFSTR(kIOHIDVendorIDKey));
  }

  bool is_keyboard(IOHIDDeviceRef device) {
    return IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
  }

  bool is_mouse(IOHIDDeviceRef device) {
    return IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse);
  }

  bool is_grabbed_by_default(IOHIDDeviceRef device, bool grab_mice) {
    return (is_keyboard(device) ||
      (grab_mice && is_mouse(device)));
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
  std::vector<GrabDeviceFilter> m_grab_filters;
  bool m_devices_changed{ };
  std::vector<IOHIDDeviceRef> m_grabbed_devices;
  std::vector<DeviceDesc> m_grabbed_device_descs;
  std::vector<Event> m_event_queue;
  size_t m_event_queue_pos{ };

public:
  ~GrabbedDevicesImpl() {
    if (!m_grabbed_devices.empty()) {
      verbose("Ungrabbing all devices");
      for (const auto& device : m_grabbed_devices)
        ungrab_device(device);
    }
    if (m_hid_manager)
      IOHIDManagerClose(m_hid_manager, kIOHIDOptionsTypeNone);
  }

  bool initialize(bool grab_mice, std::vector<GrabDeviceFilter> grab_filters) {
    // TODO: disable mouse grabbing until forwarding is implemented
    m_grab_mice = grab_mice = false;
    m_grab_filters = std::move(grab_filters);

    m_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!m_hid_manager)
      return false;
    IOHIDManagerSetDeviceMatching(m_hid_manager, nullptr);
    IOHIDManagerScheduleWithRunLoop(m_hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerRegisterDeviceMatchingCallback(m_hid_manager, &devices_changed_callback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_hid_manager, &devices_changed_callback, this);
    update();
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

      if (m_devices_changed)
        return { true, std::nullopt };

      // TODO: do not poll. see https://stackoverflow.com/questions/48434976/cfsocket-data-callbacks
      auto poll_timeout = (timeout.has_value() ? timeout.value() : Duration::max());
      if (interrupt_fd >=0) {
        if (can_read_from_file(interrupt_fd))
          return { true, std::nullopt };
        poll_timeout = std::min(poll_timeout, Duration(std::chrono::milliseconds(100)));
      }

      CFRunLoopRunInMode(kCFRunLoopDefaultMode, poll_timeout.count(), true);

      if (std::chrono::steady_clock::now() >= timeout_at)  
        return { true, std::nullopt };
    }
  }

  bool update_devices() {
    if (!m_devices_changed)
      return false;

    update();
    m_devices_changed = false;
    return true;
  }

   const std::vector<DeviceDesc>& grabbed_device_descs() const {
    return m_grabbed_device_descs;
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
    const auto device = IOHIDElementGetDevice(element);
    const auto usage = IOHIDElementGetUsage(element);
    const auto val = IOHIDValueGetIntegerValue(value);
    static_cast<GrabbedDevicesImpl*>(context)->handle_input(device, page, usage, val);
  }

  bool grab_device(IOHIDDeviceRef device) {
    const auto result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice);
    if (result != kIOReturnSuccess)
      return false;

    IOHIDDeviceRegisterInputValueCallback(device, &input_value_callback, this);
    return true;
  }

  void ungrab_device(IOHIDDeviceRef device) {
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
  }

  void handle_devices_changed() {
    m_devices_changed = true;
  }

  void handle_input(IOHIDDeviceRef device, int page, int code, int value) {
    const auto device_index = std::distance(m_grabbed_devices.begin(), 
      std::find(m_grabbed_devices.begin(), m_grabbed_devices.end(), device));
    m_event_queue.push_back({ static_cast<int>(device_index), page, code, value });
  }

  void update() {
    verbose("Updating device list");

    // get devices
    const auto device_set = IOHIDManagerCopyDevices(m_hid_manager);
    const auto device_count = CFSetGetCount(device_set);
    auto devices = std::vector<IOHIDDeviceRef>();
    devices.resize(device_count);
    CFSetGetValues(device_set, (const void**)devices.data());
    std::sort(devices.begin(), devices.end());

    // update grabbed devices
    auto previously_grabbed = std::move(m_grabbed_devices);
    for (auto i = 0; i < device_count; ++i) {
      const auto device = devices[i];
      const auto vendor_id = get_device_vendor_id(device);
      const auto is_virtual_device = (vendor_id == VirtualDevices::vendor_id);
      const auto device_name = (is_virtual_device ? 
        std::string(VirtualDevices::name) : get_device_name(device));
      const auto device_id = (is_virtual_device ?
        std::to_string(vendor_id) : get_device_id(device));
      if (device_name.empty())
        continue;

      auto status = "ignored";
      if (!is_virtual_device) {
        status = "skipped";
        if (evaluate_grab_filters(m_grab_filters, device_name, device_id,
              is_grabbed_by_default(device, m_grab_mice))) {
          const auto it = std::find(previously_grabbed.begin(), 
            previously_grabbed.end(), device);
          if (it == previously_grabbed.end()) {
            status = "grabbing failed";
            if (grab_device(device)) {
              status = "grabbed";
              m_grabbed_devices.push_back(device);
            }
          }
          else {
            status = "already grabbed";
            m_grabbed_devices.push_back(std::exchange(*it, nullptr));
          }
        }
      }
      verbose("  '%s' %s (%s)", device_name.c_str(), status, device_id.c_str());
    }

    // ungrab previously grabbed
    for (auto i = 0u; i < previously_grabbed.size(); ++i)
      if (auto device = previously_grabbed[i]) {
        ungrab_device(device);
        verbose("  '%s' ungrabbed", m_grabbed_device_descs[i].name.c_str());
      }

    m_grabbed_device_descs.clear();
    for (auto device : m_grabbed_devices)
      m_grabbed_device_descs.push_back({
        get_device_name(device),
        get_device_id(device),
      });
  }
};

//-------------------------------------------------------------------------

GrabbedDevices::GrabbedDevices()
  : m_impl(std::make_unique<GrabbedDevicesImpl>()) {
}

GrabbedDevices::GrabbedDevices(GrabbedDevices&&) noexcept = default;
GrabbedDevices& GrabbedDevices::operator=(GrabbedDevices&&) noexcept = default;
GrabbedDevices::~GrabbedDevices() = default;

bool GrabbedDevices::grab(bool grab_mice,std::vector<GrabDeviceFilter> grab_filters) {
  return m_impl->initialize(grab_mice, std::move(grab_filters));
}

bool GrabbedDevices::update_devices() {
  return m_impl->update_devices();
}

auto GrabbedDevices::read_input_event(std::optional<Duration> timeout, int interrupt_fd)
    -> std::pair<bool, std::optional<Event>> {
  return m_impl->read_input_event(timeout, interrupt_fd);
}

const std::vector<DeviceDesc>& GrabbedDevices::grabbed_device_descs() const {
  return m_impl->grabbed_device_descs();
}

std::optional<KeyEvent> to_key_event(const GrabbedDevices::Event& event) {
  const auto page = event.type;
  const auto usage = event.code;
  if (page != kHIDPage_KeyboardOrKeypad ||
      usage < kHIDUsage_KeyboardA ||
      usage > kHIDUsage_KeyboardRightGUI)
    return { };

  auto key = static_cast<Key>(event.code);
  if (macos_iso_keyboard) {
    // swap IntlBackslash and Backquote keys
    // https://github.com/pqrs-org/Karabiner-Elements/issues/1365#issuecomment-386801671
    if (key == Key::IntlBackslash)
      key = Key::Backquote;
    else if (key == Key::Backquote)
      key = Key::IntlBackslash;
  }

  return KeyEvent{
    key,
    (event.value == 0 ? KeyState::Up : KeyState::Down),
  };
}
