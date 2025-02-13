
#include "VirtualDevices.h"
#include "DeviceDescLinux.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <chrono>
#include <map>

namespace {
  std::string get_forward_device_name(const std::string& device_name) {
    return device_name + "; " + VirtualDevices::name;
  }

  int open_uinput_device() {
    const auto paths = { "/dev/input/uinput", "/dev/uinput" };
    for (const auto path : paths) {
      do {
        const auto fd = ::open(path, O_WRONLY | O_NONBLOCK);
        if (fd >= 0)
          return fd;
      } while (errno == EINTR);
    }
    return -1;
  }

  int create_keyboard_device() {
    verbose("Creating virtual keyboard device '%s'", VirtualDevices::name);
    const auto fd = open_uinput_device();
    if (fd < 0)
      return -1;

    auto uinput = uinput_setup{ };
    std::strncpy(uinput.name, VirtualDevices::name, UINPUT_MAX_NAME_SIZE - 1);
    uinput.id.bustype = BUS_USB;
    uinput.id.vendor = VirtualDevices::vendor_id;
    uinput.id.product = VirtualDevices::product_id;
    uinput.id.version = 1;

    ::ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ::ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ::ioctl(fd, UI_SET_EVBIT, EV_REP);

    // used to be KEY_MAX, but on systems with older kernels this
    // created a device which did not output any events at all!
    const auto max_key = KEY_NUMERIC_0;
    for (auto i = KEY_ESC; i < max_key; ++i)
      ::ioctl(fd, UI_SET_KEYBIT, i);

    ::ioctl(fd, UI_SET_EVBIT, EV_REL);
    for (auto i = BTN_LEFT; i <= BTN_TASK; ++i)
      ::ioctl(fd, UI_SET_KEYBIT, i);
    ::ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
    ::ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);
    ::ioctl(fd, UI_SET_RELBIT, REL_WHEEL_HI_RES);
    ::ioctl(fd, UI_SET_RELBIT, REL_HWHEEL_HI_RES);

    // add absolute axes which are commonly found on keyboards
    ::ioctl(fd, UI_SET_EVBIT, EV_ABS);
    auto abs_setup = uinput_abs_setup{ };
    abs_setup.absinfo.minimum = 0;
    abs_setup.absinfo.maximum = 1023;
    abs_setup.code = ABS_VOLUME;
    ::ioctl(fd, UI_ABS_SETUP, &abs_setup);
    abs_setup.code = ABS_MISC;
    ::ioctl(fd, UI_ABS_SETUP, &abs_setup);

    if (::ioctl(fd, UI_DEV_SETUP, &uinput) < 0 ||
        ::ioctl(fd, UI_DEV_CREATE) < 0) {
      ::close(fd);
      return -1;
    }
    return fd;
  }

  int create_forward_device(const std::string& name, const DeviceDescLinux& desc) {
    verbose("Creating virtual forward device '%s'", name.c_str());
    const auto fd = open_uinput_device();
    if (fd < 0)
      return -1;

    auto uinput = uinput_setup{ };
    std::strncpy(uinput.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    uinput.id.bustype = BUS_USB;
    uinput.id.vendor = desc.vendor_id;
    uinput.id.product = desc.product_id;
    uinput.id.version = desc.version_id;

    ::ioctl(fd, UI_SET_EVBIT, EV_SYN);

    if (!desc.keys.empty()) {
      ::ioctl(fd, UI_SET_EVBIT, EV_KEY);
      for (auto key_code : desc.keys)
        ::ioctl(fd, UI_SET_KEYBIT, key_code);
    }

    if (desc.rel_axes) {
      ::ioctl(fd, UI_SET_EVBIT, EV_REL);
      for (uint64_t code = 0, bits = desc.rel_axes; bits > 0; bits >>= 1, ++code)
        if (bits & 0x01)
          ::ioctl(fd, UI_SET_RELBIT, code);
    }

    if (!desc.abs_axes.empty()) {
      ::ioctl(fd, UI_SET_EVBIT, EV_ABS);
      for (const auto& abs_axis : desc.abs_axes) {
        auto abs_setup = uinput_abs_setup{ };
        abs_setup.code = abs_axis.code;
        abs_setup.absinfo.value = abs_axis.value;
        abs_setup.absinfo.minimum = abs_axis.minimum;
        abs_setup.absinfo.maximum = abs_axis.maximum;
        abs_setup.absinfo.fuzz = abs_axis.fuzz;
        abs_setup.absinfo.flat = abs_axis.flat;
        abs_setup.absinfo.resolution = abs_axis.resolution;
        ::ioctl(fd, UI_ABS_SETUP, &abs_setup);
      }
    }

    if (desc.rep_events)
      ::ioctl(fd, UI_SET_EVBIT, EV_REP);

    if (desc.misc_events) {
      ::ioctl(fd, UI_SET_EVBIT, EV_MSC);
      for (uint64_t code = 0, bits = desc.misc_events; bits > 0; bits >>= 1, ++code)
        if (bits & 0x01)
          ::ioctl(fd, UI_SET_MSCBIT, code);
    }
    
    for (uint64_t code = 0, bits = desc.properties; bits > 0; bits >>= 1, ++code)
      if (bits & 0x01)
        ::ioctl(fd, UI_SET_PROPBIT, code);
        
    if (::ioctl(fd, UI_DEV_SETUP, &uinput) < 0 ||
        ::ioctl(fd, UI_DEV_CREATE) < 0) {
      ::close(fd);
      return -1;
    }
    return fd;
  }

  void destroy_uinput_device(int fd) {
    if (fd >= 0) {
      verbose("Destroying virtual device");
      ::ioctl(fd, UI_DEV_DESTROY);
      ::close(fd);
    }
  }

  bool has_mouse_axes(const DeviceDescLinux& desc) {
    return static_cast<bool>(desc.rel_axes & (REL_X | REL_Y));
  }

  class VirtualDevice {
  private:
    const int m_uinput_fd{ -1 };
    const bool m_has_mouse_axes{ false };
    std::vector<Key> m_down_keys;
    int m_highres_wheel_accumulators[2]{ };

  public:
    explicit VirtualDevice(int uinput_fd)
      : m_uinput_fd(uinput_fd) {
    }

    VirtualDevice(int uinput_fd, const DeviceDescLinux& desc)
      : m_uinput_fd(uinput_fd),
        m_has_mouse_axes(::has_mouse_axes(desc)) {
    }

    ~VirtualDevice() {
      destroy_uinput_device(m_uinput_fd);
    }

    bool has_mouse_axes() const {
      return m_has_mouse_axes;
    }

    int update_key_state(const KeyEvent& event) {
      const auto release = 0;
      const auto press = 1;
      const auto autorepeat = 2;

      const auto it = std::find(begin(m_down_keys), end(m_down_keys), event.key);
      if (event.state == KeyState::Up) {
        if (it != m_down_keys.end())
          m_down_keys.erase(
                std::remove(begin(m_down_keys), end(m_down_keys), event.key),
                end(m_down_keys));
        return release;
      }

      if (it != m_down_keys.end())
        return autorepeat;

      m_down_keys.push_back(event.key);
      return press;
    }
    
    int update_lowres_wheel(bool vertical, int highres_value) {
      auto& accumulator = m_highres_wheel_accumulators[vertical ? 1 : 0];
      accumulator += highres_value;
      const auto lowres_value = accumulator / 120;
      accumulator -= lowres_value * 120;
      return lowres_value;
    }

    bool send_event(int type, int code, int value) {
      auto event = input_event{ };
      ::gettimeofday(&event.time, nullptr);
      event.type = static_cast<unsigned short>(type);
      event.code = static_cast<unsigned short>(code);
      event.value = value;
      do {
        const auto result = ::write(m_uinput_fd, &event, sizeof(event));
        if (result >= 0)
          return (result == sizeof(event));
      } while (errno == EINTR);

      return false;
    }
  };
} // namespace

//-------------------------------------------------------------------------

class VirtualDevicesImpl {
private:
  std::unique_ptr<VirtualDevice> m_keyboard;
  std::map<int, VirtualDevice> m_forward_devices;
  std::vector<VirtualDevice*> m_devices;
  VirtualDevice* m_last_active_mouse{ };

public:
  bool create_keyboard_device() {
    const auto uinput_fd = ::create_keyboard_device();
    if (uinput_fd < 0)
      return false;
    m_keyboard = std::make_unique<VirtualDevice>(uinput_fd);
    return true;
  }

  bool update_forward_devices(const std::vector<DeviceDesc>& device_descs) {
    auto prev = std::move(m_forward_devices);
    m_last_active_mouse = nullptr;
    m_forward_devices.clear();
    m_devices.clear();
    for (const auto& desc : device_descs) {
      // by default forward events using virtual keyboard
      auto& device = m_devices.emplace_back(m_keyboard.get());
      
      // create virtual forward device for event id (reuse existing)      
      if (const auto* desc_ext = static_cast<const DeviceDescLinux*>(desc.ext.get())) {
        if (auto node = prev.extract(desc_ext->event_id)) {
          device = &m_forward_devices.insert(std::move(node)).position->second;
        }
        else {
          const auto uinput_fd = ::create_forward_device(
            get_forward_device_name(desc.name), *desc_ext);
          if (uinput_fd < 0)
             return false;
          device = &m_forward_devices.emplace(std::piecewise_construct,
            std::forward_as_tuple(desc_ext->event_id), 
            std::forward_as_tuple(uinput_fd, *desc_ext)).first->second;
        }
      }
    }
    return true;
  }

  bool forward_event(int device_index, int type, int code, int value) {
    auto device = m_devices[device_index];

    if (device->has_mouse_axes())
      m_last_active_mouse = device;

    return device->send_event(type, code, value);
  }

  bool send_key_event(const KeyEvent& event) {
    auto device = m_keyboard.get();

    // send mouse buttons and wheel events with last active mouse
    if (m_last_active_mouse && (is_mouse_button(event.key) || is_mouse_wheel(event.key)))
      device = m_last_active_mouse;

    if (is_mouse_wheel(event.key)) {
      const auto vertical = (event.key == Key::WheelUp || event.key == Key::WheelDown);
      const auto negative = (event.key == Key::WheelDown || event.key == Key::WheelLeft);
      const auto value = (event.value ? event.value : 120) * (negative ? -1 : 1);
      device->send_event(EV_REL, (vertical ? REL_WHEEL_HI_RES : REL_HWHEEL_HI_RES), value);
      if (auto lowres_value = device->update_lowres_wheel(vertical, value))
        device->send_event(EV_REL, (vertical ? REL_WHEEL : REL_HWHEEL), lowres_value);
    }
    else {
      if (!device->send_event(EV_KEY, *event.key, device->update_key_state(event)))
        return false;
    }
    return device->send_event(EV_SYN, SYN_REPORT, 0);
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
  if (!impl->create_keyboard_device())
    return false;
  m_impl = std::move(impl);
  return true;
}

bool VirtualDevices::update_forward_devices(const std::vector<DeviceDesc>& device_descs) {
  return (m_impl && m_impl->update_forward_devices(device_descs));
}

bool VirtualDevices::send_key_event(const KeyEvent& event) {
  return (m_impl && m_impl->send_key_event(event));
}

bool VirtualDevices::forward_event(int device_index, int type, int code, int value) {
  return (m_impl && m_impl->forward_event(device_index, type, code, value));
}

bool VirtualDevices::flush() {
  return true;
}
