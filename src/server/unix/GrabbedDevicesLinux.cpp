
#include "GrabbedDevices.h"
#include "common/output.h"
#include "common/Duration.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
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

  bool is_gamedevice(int fd) {
    // it is a gamedevice when it has an axis which is not commonly found on keyboards
    const auto common_abs_bits = bit<ABS_VOLUME> | bit<ABS_MISC>;
    auto abs_bits = uint64_t{ };
    return (::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), &abs_bits) >= 0 &&
       (abs_bits & (~common_abs_bits)) != 0);
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

    return ((has_keys(fd) || (grab_mice && is_mouse(fd))) && !is_gamedevice(fd));
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

  int open_event_device(const char* event_path) {
    do {
      const auto fd = ::open(event_path, O_RDONLY);
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
  struct Device {
    int event_id;
    int fd;
    IntRange abs_range_volume;
    IntRange abs_range_misc;
    bool disappeared;
  };

  std::string_view m_ignore_device_name;
  bool m_grab_mice{ };
  int m_device_monitor_fd{ -1 };
  std::vector<Device> m_grabbed_devices;
  std::vector<std::string> m_grabbed_device_names;
  bool m_devices_changed{ };

public:
  using Event = GrabbedDevices::Event;
  using Duration = GrabbedDevices::Duration;

  ~GrabbedDevicesImpl() {
    if (!m_grabbed_devices.empty()) {
      verbose("Ungrabbing all devices");
      for (const auto& device : m_grabbed_devices)
        ungrab_device(device);
    }
    release_device_monitor();
  }

  bool initialize(const char* ignore_device_name, bool grab_mice) {
    m_ignore_device_name = ignore_device_name;
    m_grab_mice = grab_mice;
    update();
    return true;
  }

  bool update_devices() {
    if (!m_devices_changed)
      return false;

    update();
    m_devices_changed = false;
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
      for (const auto& device : m_grabbed_devices) {
        max_fd = std::max(max_fd, device.fd);
        FD_SET(device.fd, &read_set);
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
        m_devices_changed = true;
        return { true, std::nullopt };
      }

      if (interrupt_fd >= 0 &&
          FD_ISSET(interrupt_fd, &read_set))
        return { true, std::nullopt };

      auto device_index = 0;
      for (const auto& device : m_grabbed_devices) {
        if (FD_ISSET(device.fd, &read_set)) {
          auto ev = input_event{ };
          if (!read_all(device.fd,
                reinterpret_cast<char*>(&ev), sizeof(input_event)))
            return { false, std::nullopt };

          // map from device range to default range
          if (ev.type == EV_ABS) {
            if (ev.code == ABS_VOLUME) {
              ev.value = map_to_range(ev.value, device.abs_range_volume, default_abs_range);
            }
            else if (ev.code == ABS_MISC) {
              ev.value = map_to_range(ev.value, device.abs_range_misc, default_abs_range);
            }
          }

          return { true, Event{ device_index, ev.type, ev.code, ev.value } };
        }
        ++device_index;
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
  
  bool grab_device(int event_id, int fd) {
    wait_until_keys_released(fd);
    if (!grab_event_device(fd, true))
      return false;

    m_grabbed_devices.push_back({
      event_id,
      ::dup(fd),
      get_device_abs_range(fd, ABS_VOLUME),
      get_device_abs_range(fd, ABS_MISC),
    });
    return true;
  }

  void ungrab_device(const Device& device) {
    wait_until_keys_released(device.fd);
    grab_event_device(device.fd, false);
    ::close(device.fd);
  }

  void update() {
    verbose("Updating device list");

    // reset device monitor
    initialize_device_monitor();

    // mark all devices as disappeared
    for (auto& device : m_grabbed_devices)
      device.disappeared = true;

    // grab new devices
    auto ec = std::error_code{ };
    for (auto const& entry : std::filesystem::directory_iterator("/dev/input", ec)) {
      const auto& path = entry.path();
      auto event_id = 0;
      if (!entry.is_character_file(ec) || 
          ::sscanf(path.c_str(), "/dev/input/event%d", &event_id) != 1)
        continue;

      if (const auto fd = open_event_device(path.c_str()); fd >= 0) {
        const auto device_name = get_device_name(fd);
        auto status = "ignored";
        if (device_name != m_ignore_device_name &&
            is_supported_device(fd, m_grab_mice)) {

          const auto it = std::find_if(m_grabbed_devices.begin(), m_grabbed_devices.end(),
            [&](const Device& device) { return device.event_id == event_id; });
          if (it == m_grabbed_devices.end()) {
            status = "grabbing failed";
            if (grab_device(event_id, fd))
              status = "grabbed";
          }
          else {
            status = "already grabbed";
            it->disappeared = false;
          }
        }
        ::close(fd);
        verbose("  %s %s (%s)", path.c_str(), status, device_name.c_str());
      }
      else {
        verbose("  %s opening failed", path.c_str());
      }
    }

    // ungrab disappeared devices
    for (auto it = m_grabbed_devices.begin(); it != m_grabbed_devices.end(); ) {
      if (it->disappeared) {
        ungrab_device(*it);
        verbose("  /dev/input/event%d ungrabbed", it->event_id);
        it = m_grabbed_devices.erase(it);
      }
      else {
        ++it;
      }
    }
    
    // collect grabbed device names
    m_grabbed_device_names.clear();
    for (const auto& device : m_grabbed_devices)
      m_grabbed_device_names.push_back(get_device_name(device.fd));
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

bool GrabbedDevices::update_devices() {
  return m_impl->update_devices();
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
