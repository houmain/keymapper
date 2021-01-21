
#include "keyboard.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

const auto EVDEV_MINORS = 32;

namespace {
  bool read_all(int fd, char* buffer, size_t length) {
    while (length != 0) {
      auto ret = ::read(fd, buffer, length);
      if (ret == -1 && errno == EINTR)
        continue;
      if (ret <= 0)
        return false;
      length -= static_cast<size_t>(ret);
      buffer += ret;
    }
    return true;
  }
} // namespace

bool is_keyboard(int fd) {
  auto version = int{ };
  if (::ioctl(fd, EVIOCGVERSION, &version) == -1 ||
      version != EV_VERSION)
    return false;

  auto devinfo = input_id{ };
  if (::ioctl(fd, EVIOCGID, &devinfo) != 0)
    return false;

  switch (devinfo.bustype) {
    case BUS_USB:
    case BUS_I8042:
    case BUS_ADB:
      break;
    default:
      return false;
  }

  const auto required_bits = (1 << EV_SYN) | (1 << EV_KEY) | (1 << EV_REP);
  auto bits = 0;
  if (::ioctl(fd, EVIOCGBIT(0, sizeof(bits)), &bits) == -1 ||
      (bits & required_bits) != required_bits)
    return false;

  return true;
}

bool wait_until_keys_released(int fd) {
  const auto retries = 1000;
  const auto sleep_ms = 5;
  for (auto i = 0; i < retries; ++i) {
    auto bits = std::array<char, (KEY_MAX + 7) / 8>();
    if (::ioctl(fd, EVIOCGKEY(bits.size()), bits.data()) == -1)
      return false;

    const auto all_keys_released =
      std::none_of(std::cbegin(bits), std::cend(bits),
        [](char bits) { return (bits != 0); });
    if (all_keys_released)
      return true;

    ::usleep(sleep_ms * 1000);
  }
  return false;
}

bool grab_keyboard(int fd, bool grab) {
  return (::ioctl(fd, EVIOCGRAB, (grab ? 1 : 0)) == 0);
}

int open_event_device(int index) {
  const auto paths = { "/dev/input/event%d", "/dev/event%d" };
  for (const auto path : paths) {
    auto buffer = std::array<char, 128>();
    std::snprintf(buffer.data(), buffer.size(), path, index);
    do {
      const auto fd = ::open(buffer.data(), O_RDONLY);
      if (fd >= 0)
        return fd;
    } while (errno == EINTR);
  }
  return -1;
}

std::vector<int> grab_keyboards() {
  auto fds = std::vector<int>();
  for (auto i = 0; i < EVDEV_MINORS; ++i) {
    const auto fd = open_event_device(i);
    if (fd >= 0) {
      if (is_keyboard(fd) &&
          wait_until_keys_released(fd) &&
          grab_keyboard(fd, true)) {
        fds.push_back(fd);
      }
      else {
        ::close(fd);
      }
    }
  }
  return fds;
}

void release_keyboards(const std::vector<int>& fds) {
  for (auto fd : fds)
    if (fd >= 0) {
      grab_keyboard(fd, false);
      ::close(fd);
    }
}

bool read_event(const std::vector<int>& fds, int* type, int* code, int* value) {
  auto rfds = fd_set{ };
  FD_ZERO(&rfds);
  auto max_fd = 0;
  for (auto fd : fds) {
    max_fd = std::max(max_fd, fd);
    FD_SET(fd, &rfds);
  }

  if (::select(max_fd + 1, &rfds, nullptr, nullptr, nullptr) > 0)
    for (auto fd : fds)
      if (FD_ISSET(fd, &rfds)) {
        auto ev = input_event{ };
        if (!read_all(fd, reinterpret_cast<char*>(&ev), sizeof(input_event)))
          return false;
        *type = ev.type;
        *code = ev.code;
        *value = ev.value;
        return true;
      }

  return false;
}
