
#include "uinput_device.h"
#include "runtime/KeyEvent.h"
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <chrono>

namespace {
  const auto min_time_before_mouse_button = std::chrono::milliseconds(100);

  std::vector<Key> g_down_keys;
  std::chrono::system_clock::time_point g_last_event_time;
  bool g_last_event_was_mouse_button;

  int get_key_event_value(const KeyEvent& event) {
    const auto release = 0;
    const auto press = 1;
    const auto autorepeat = 2;

    const auto it = std::find(begin(g_down_keys), end(g_down_keys), event.key);
    if (event.state == KeyState::Up) {
      if (it != g_down_keys.end())
        g_down_keys.erase(
          std::remove(begin(g_down_keys), end(g_down_keys), event.key),
          end(g_down_keys));
      return release;
    }

    if (it != g_down_keys.end())
      return autorepeat;

    g_down_keys.push_back(event.key);
    return press;
  }
} // namespace

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
  ::ioctl(fd, UI_SET_EVBIT, EV_REL);

  // used to be KEY_MAX, but on systems with older kernels this
  // created a device which did not output any events at all!
  const auto max_key = KEY_NUMERIC_0;
  for (auto i = KEY_ESC; i < max_key; ++i)
    ::ioctl(fd, UI_SET_KEYBIT, i);

  for (auto i = BTN_LEFT; i <= BTN_TASK; ++i)
    ::ioctl(fd, UI_SET_KEYBIT, i);

  ::ioctl(fd, UI_SET_RELBIT, REL_X);
  ::ioctl(fd, UI_SET_RELBIT, REL_Y);
  ::ioctl(fd, UI_SET_RELBIT, REL_Z);
  ::ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
  ::ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);

  if (::ioctl(fd, UI_DEV_SETUP, &uinput) < 0 ||
      ::ioctl(fd, UI_DEV_CREATE) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

void destroy_uinput_device(int fd) {
  if (fd >= 0) {
    ::ioctl(fd, UI_DEV_DESTROY);
    ::close(fd);
  }
}

bool send_event(int fd, int type, int code, int value) {
  auto event = input_event{ };
  ::gettimeofday(&event.time, nullptr);
  event.type = static_cast<unsigned short>(type);
  event.code = static_cast<unsigned short>(code);
  event.value = value;
  do {
    const auto result = ::write(fd, &event, sizeof(event));
    if (result >= 0)
      return (result == sizeof(event));
  } while (errno == EINTR);

  return false;
}

bool send_key_event(int fd, const KeyEvent& event) {
  // ensure minimum delay between sending modifier and sending mouse button
  // and between sending mouse button and sending keys,
  // otherwise they are likely applied in the wrong order
  const auto is_mouse_event = is_mouse_button(event.key);
  if (g_last_event_was_mouse_button != is_mouse_event) {
    const auto elapsed = (std::chrono::system_clock::now() - g_last_event_time);
    const auto delay = std::chrono::duration_cast<std::chrono::microseconds>(
        min_time_before_mouse_button - elapsed);
    if (delay > std::chrono::seconds::zero())
      ::usleep(static_cast<__useconds_t>(delay.count()));
    g_last_event_was_mouse_button = is_mouse_event;
  }
  g_last_event_time = std::chrono::system_clock::now();

  return send_event(fd, EV_KEY, *event.key, get_key_event_value(event)) &&
         send_event(fd, EV_SYN, SYN_REPORT, 0);
}
