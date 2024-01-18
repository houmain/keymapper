
#include "VirtualDevice.h"
#include "runtime/KeyEvent.h"
#include "common/output.h"
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <chrono>

namespace {
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

  int create_uinput_device(const char* name) {
    const auto fd = open_uinput_device();
    if (fd < 0)
      return -1;

    auto uinput = uinput_setup{ };
    std::strncpy(uinput.name, name, UINPUT_MAX_NAME_SIZE - 1);
    uinput.id.bustype = BUS_USB;
    uinput.id.vendor = 0xD1CE;
    uinput.id.product = 1;
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
    ::ioctl(fd, UI_SET_RELBIT, REL_X);
    ::ioctl(fd, UI_SET_RELBIT, REL_Y);
    ::ioctl(fd, UI_SET_RELBIT, REL_Z);
    ::ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
    ::ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);
#if defined(REL_WHEEL_HI_RES)
    ::ioctl(fd, UI_SET_RELBIT, REL_WHEEL_HI_RES);
    ::ioctl(fd, UI_SET_RELBIT, REL_HWHEEL_HI_RES);
#endif

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

  void destroy_uinput_device(int fd) {
    if (fd >= 0) {
      verbose("Destroying virtual device");
      ::ioctl(fd, UI_DEV_DESTROY);
      ::close(fd);
    }
  }
} // namespace

//-------------------------------------------------------------------------

class VirtualDeviceImpl {
private:
  int m_uinput_fd{ -1 };
  std::vector<Key> m_down_keys;

  int get_key_event_value(const KeyEvent& event) {
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

public:
  ~VirtualDeviceImpl() {
    destroy_uinput_device(m_uinput_fd);
  }

  bool create(const char* name) {
    if (m_uinput_fd >= 0)
      return false;
    m_uinput_fd = create_uinput_device(name);
    return (m_uinput_fd >= 0);
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

  bool send_key_event(const KeyEvent& event) {
    return send_event(EV_KEY, *event.key, get_key_event_value(event)) &&
           send_event(EV_SYN, SYN_REPORT, 0);
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
  return (m_impl && m_impl->send_event(type, code, value));
}

bool VirtualDevice::flush() {
  return true;
}
