
#include "uinput_keyboard.h"
#include "runtime/KeyEvent.h"
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>

namespace {
  std::vector<KeyCode> g_down_keys;

  int get_key_event_value(KeyEvent event) {
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
  };
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

int create_uinput_keyboard(const char* name) {
  const auto fd = open_uinput_device();
  if (fd < 0)
    return -1;

  auto uinput = uinput_user_dev{ };
  std::strncpy(uinput.name, name, UINPUT_MAX_NAME_SIZE - 1);
  uinput.id.bustype = BUS_I8042;
  uinput.id.vendor = 1;
  uinput.id.product = 1;
  uinput.id.version = 1;

  ::ioctl(fd, UI_SET_EVBIT, EV_SYN);
  ::ioctl(fd, UI_SET_EVBIT, EV_KEY);
  ::ioctl(fd, UI_SET_EVBIT, EV_REP);
  for (auto i = 0; i < KEY_MAX; ++i)
    ::ioctl(fd, UI_SET_KEYBIT, i);

  if (::write(fd, &uinput, sizeof(uinput)) != sizeof(uinput)) {
    ::close(fd);
    return -1;
  }

  if (::ioctl(fd, UI_DEV_CREATE) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

void destroy_uinput_keyboard(int fd) {
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
  return send_event(fd, EV_KEY, event.key, get_key_event_value(event));
}

bool flush_events(int fd) {
  return send_event(fd, EV_SYN, SYN_REPORT, 0);
}

