
#include "ipc.h"
#include "config/Config.h"
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ipc.h>

namespace {
  auto g_pipe_broken = false;

  bool write_all(int fd, const char* buffer, size_t length) {
    while (length != 0) {
      auto ret = ::write(fd, buffer, length);
      if (ret == -1 && errno == EINTR)
        continue;
      if (ret <= 0)
        return false;
      length -= static_cast<size_t>(ret);
      buffer += ret;
    }
    return true;
  }

  template<typename T, typename = std::enable_if_t<std::is_trivial_v<T>>>
  void send(int fd, const T& value) {
    if (!write_all(fd, reinterpret_cast<const char*>(&value), sizeof(T)))
      g_pipe_broken = true;
  }

  void send(int fd, const KeySequence& sequence) {
    send(fd, static_cast<uint8_t>(sequence.size()));
    for (const auto& event : sequence) {
      send(fd, event.key);
      send(fd, event.state);
    }
  }
} // namespace

int initialize_ipc(const char* fifo_filename) {
  ::signal(SIGPIPE, [](int) { g_pipe_broken = true; });
  g_pipe_broken = false;

  return ::open(fifo_filename, O_WRONLY);
}

void shutdown_ipc(int fd) {
  ::close(fd);
}

bool send_config(int fd, const Config& config) {
  // send mappings
  send(fd, static_cast<uint16_t>(config.commands.size()));
  for (const auto& command : config.commands) {
    send(fd, command.input);
    send(fd, command.default_mapping);
  }

  // send mapping overrides
  // for each context find the mappings belonging to it
  send(fd, static_cast<uint16_t>(config.contexts.size()));
  auto context_mappings = std::vector<std::pair<int, const ContextMapping*>>();
  for (auto i = 0u; i < config.contexts.size(); ++i) {
    context_mappings.clear();
    for (auto j = 0u; j < config.commands.size(); ++j) {
      const auto& command = config.commands[j];
      for (const auto& context_mapping : command.context_mappings)
        if (context_mapping.context_index == static_cast<int>(i))
          context_mappings.emplace_back(j, &context_mapping);
    }
    send(fd, static_cast<uint16_t>(context_mappings.size()));
    for (const auto& mapping : context_mappings) {
      send(fd, static_cast<uint16_t>(mapping.first));
      send(fd, mapping.second->output);
    }
  }
  return !g_pipe_broken;
}

bool is_pipe_broken(int fd) {
  auto pfd = pollfd{ fd, POLLERR, 0 };
  return (::poll(&pfd, 1, 0) < 0 || (pfd.revents & POLLERR));
}

bool send_active_override_set(int fd, int index) {
  send(fd, static_cast<uint8_t>(1));
  send(fd, static_cast<uint8_t>(index));
  return !g_pipe_broken;
}
