
#include "GrabbedDevices.h"
#include "common/output.h"
#include "common/Duration.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/inotify.h>

namespace {
  struct IntRange {
    int min;
    int max;
  };

  const auto default_abs_range = IntRange{ 0, 1023 };

  template<uint64_t Value> uint64_t bit = (1ull << Value);

  int map_to_range(int value, const IntRange& from, const IntRange& to) {
    const auto range = (from.max - from.min);
    if (!range)
      return to.min;
    return (value - from.min) * (to.max - to.min) / range + to.min;
  }

  bool has_keys(int fd) {
    auto key_bits = std::array<uint64_t, 4>{ };
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), &key_bits) >= 0)
      for (auto bits : key_bits)
        if (bits != 0x00)
          return true;
    return false;
  }

  bool is_mouse(int fd) {
    // it is a mouse when it has a relative X- and Y-axis
    const auto required_rel_bits = bit<REL_X> | bit<REL_Y>;
    auto rel_bits = uint64_t{ };
    return (::ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), &rel_bits) >= 0 &&
       (rel_bits & required_rel_bits) == required_rel_bits);
  }

  bool is_supported_device(int fd, bool grab_mice) {
    auto version = int{ };
    if (::ioctl(fd, EVIOCGVERSION, &version) == -1 ||
        version != EV_VERSION)
      return false;

    auto ev_bits = uint64_t{ };
    if (::ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), &ev_bits) == -1)
      return false;

    const auto required_ev_bits = bit<EV_SYN> | bit<EV_KEY>;
    if ((ev_bits & required_ev_bits) != required_ev_bits)
      return false;

    return (has_keys(fd) || (grab_mice && is_mouse(fd)));
  }

  std::string get_device_name(int fd) {
    auto name = std::array<char, 256>();
    if (::ioctl(fd, EVIOCGNAME(name.size()), name.data()) >= 0)
      return name.data();
    return "";
  }

  IntRange get_device_abs_range(int fd, int abs_event) {
    auto absinfo = input_absinfo{ };
    if (::ioctl(fd, EVIOCGABS(abs_event), &absinfo) >= 0)
      return { absinfo.minimum, absinfo.maximum };
    return default_abs_range;
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

  bool grab_event_device(int fd, bool grab) {
    return (::ioctl(fd, EVIOCGRAB, (grab ? 1 : 0)) == 0);
  }

  int open_event_device(int index) {
    auto buffer = std::array<char, 32>();
    std::snprintf(buffer.data(), buffer.size(), "/dev/input/event%d", index);
    do {
      const auto fd = ::open(buffer.data(), O_RDONLY);
      if (fd >= 0)
        return fd;
    } while (errno == EINTR);

    return -1;
  }

  int create_event_device_monitor() {
    auto fd = ::inotify_init();
    if (fd >= 0) {
      auto ret = ::inotify_add_watch(fd, "/dev/input", IN_CREATE | IN_DELETE);
      if (ret == -1) {
        ::close(fd);
        fd = -1;
      }
    }
    return fd;
  }

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

//-------------------------------------------------------------------------

class GrabbedDevicesImpl {
private:
  struct AbsRanges {
    IntRange volume;
    IntRange misc;
  };

  const char* m_ignore_device_name{ };
  bool m_grab_mice{ };
  std::unordered_map<int, int> m_event_fds;
  int m_device_monitor_fd{ -1 };
  std::vector<int> m_grabbed_device_fds;
  std::vector<std::string> m_grabbed_device_names;
  std::vector<AbsRanges> m_grabbed_device_abs_ranges;

public:
  using Event = GrabbedDevices::Event;
  using Duration = GrabbedDevices::Duration;

  GrabbedDevicesImpl() { }

  ~GrabbedDevicesImpl() {
    ungrab_all_devices();
    release_device_monitor();
  }

  bool initialize(const char* ignore_device_name, bool grab_mice) {
    m_ignore_device_name = ignore_device_name;
    m_grab_mice = grab_mice;
    update();
    return true;
  }

  const std::vector<std::string>& grabbed_device_names() const {
    return m_grabbed_device_names;
  }

  std::pair<bool, std::optional<Event>> read_input_event(
        std::optional<Duration> timeout, int interrupt_fd) {
    for (;;) {
      auto read_set = fd_set{ };
      FD_ZERO(&read_set);
      auto max_fd = 0;
      for (auto fd : m_grabbed_device_fds) {
        max_fd = std::max(max_fd, fd);
        FD_SET(fd, &read_set);
      }

      if (m_device_monitor_fd >= 0) {
        max_fd = std::max(max_fd, m_device_monitor_fd);
        FD_SET(m_device_monitor_fd, &read_set);
      }

      if (interrupt_fd >= 0) {
        max_fd = std::max(max_fd, interrupt_fd);
        FD_SET(interrupt_fd, &read_set);
      }

      auto timeoutval = (timeout ? to_timeval(timeout.value()) : timeval{ });
      const auto result = ::select(max_fd + 1, &read_set,
        nullptr, nullptr, (timeout ? &timeoutval : nullptr));
      if (result == -1 && errno == EINTR)
        continue;

      if (result < 0)
        return { false, std::nullopt };

      if (m_device_monitor_fd >= 0 &&
          FD_ISSET(m_device_monitor_fd, &read_set)) {
        update();
        continue;
      }

      if (interrupt_fd >= 0 &&
          FD_ISSET(interrupt_fd, &read_set))
        return { true, std::nullopt };

      for (auto i = 0; i < static_cast<int>(m_grabbed_device_fds.size()); ++i)
        if (FD_ISSET(m_grabbed_device_fds[i], &read_set)) {
          auto ev = input_event{ };
          if (!read_all(m_grabbed_device_fds[i],
                reinterpret_cast<char*>(&ev), sizeof(input_event)))
            return { false, std::nullopt };

          // map from device range to default range
          if (ev.type == EV_ABS) {
            const auto& ranges = m_grabbed_device_abs_ranges[i];
            if (ev.code == ABS_VOLUME) {
              ev.value = map_to_range(ev.value, ranges.volume, default_abs_range);
            }
            else if (ev.code == ABS_MISC) {
              ev.value = map_to_range(ev.value, ranges.misc, default_abs_range);
            }
          }

          return { true, Event{ i, ev.type, ev.code, ev.value } };
        }

      // timeout
      return { true, std::nullopt };
    }
  }

private:
  void initialize_device_monitor() {
    release_device_monitor();
    m_device_monitor_fd = create_event_device_monitor();
  }

  void release_device_monitor() {
    if (m_device_monitor_fd >= 0) {
      ::close(m_device_monitor_fd);
      m_device_monitor_fd = -1;
    }
  }

  void grab_device(int event_id, int fd) {
    if (!m_event_fds.contains(event_id)) {
      const auto device_name = get_device_name(fd);
      if (device_name != m_ignore_device_name) {
        verbose("Grabbing device event%i '%s'", event_id, device_name.c_str());
        wait_until_keys_released(fd);
        if (grab_event_device(fd, true)) {
          m_event_fds[event_id] = ::dup(fd);
        }
        else {
          error("Grabbing device failed");
        }
      }
    }
  }

  void ungrab_device(int event_id, int& event_fd) {
    if (event_fd >= 0) {
      verbose("Ungrabbing device event%i", event_id);
      wait_until_keys_released(event_fd);
      grab_event_device(event_fd, false);
      ::close(event_fd);
      event_fd = -1;
    }
  }

  void ungrab_device(int event_id) {
    auto event_fd_lookup = m_event_fds.find(event_id);
    if (event_fd_lookup != m_event_fds.end())
      ungrab_device(event_fd_lookup->first, event_fd_lookup->second);
  }

  void ungrab_all_devices() {
    for (auto & [event_id, event_fd] : m_event_fds)
      ungrab_device(event_id, event_fd);
    m_grabbed_device_fds.clear();
    m_grabbed_device_names.clear();
    m_grabbed_device_abs_ranges.clear();
  }

  void update() {
    verbose("Updating device list");

    // reset device monitor
    initialize_device_monitor();

    // update grabbed devices
    for (auto const& entry : std::filesystem::directory_iterator("/dev/input")) {
      int event_id;
      if (!entry.is_character_file()
        || sscanf(entry.path().c_str(), "/dev/input/event%d", &event_id) != 1)
        continue;

      const auto fd = open_event_device(event_id);
      if (fd >= 0 && is_supported_device(fd, m_grab_mice)) {
        // grab new ones
        grab_device(event_id, fd);
      }
      else {
        // ungrab previously grabbed
        ungrab_device(event_id);
      }
      if (fd >= 0)
        ::close(fd);
    }

    // collect grabbed device fds
    m_grabbed_device_fds.clear();
    m_grabbed_device_names.clear();
    m_grabbed_device_abs_ranges.clear();
    for (auto event_fd_pair : m_event_fds) {
      auto event_fd = event_fd_pair.second;
      if (event_fd >= 0) {
        m_grabbed_device_fds.push_back(event_fd);
        m_grabbed_device_names.push_back(get_device_name(event_fd));
        m_grabbed_device_abs_ranges.push_back({
          get_device_abs_range(event_fd, ABS_VOLUME),
          get_device_abs_range(event_fd, ABS_MISC),
        });
      }
    }
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
  if (event.type != EV_KEY)
    return { };
    
  return KeyEvent{
    static_cast<Key>(event.code),
    (event.value == 0 ? KeyState::Up : KeyState::Down),
  };
}
