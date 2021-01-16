
#include "ipc.h"
#include "config/Config.h"
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <pwd.h>
#include <string.h>
#include <regex>

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

  void send(int fd, const Action& action) {
    send(fd, static_cast<uint8_t>(action.type));
    send(fd, action.sequence);
    send(fd, static_cast<uint8_t>(action.command.size()));
    write_all(fd, action.command.c_str(), action.command.size());
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

bool send_name(int fd) {
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  char *name = 0;
  if (pw)
      name = pw->pw_name;
  
  send(fd, static_cast<uint16_t>(strlen(name)));
  write_all(fd, name, strlen(name));

  return !g_pipe_broken;
}

bool send_environment(int fd) {
  std::regex escape("(\"|\\\\)");
  std::string env;

  for (int i = 0; environ[i] != NULL; i++) {
    env.append(" \"");
    env.append(std::regex_replace(environ[i], escape, "\\$1"));
    env.append("\"");
  }

  send(fd, static_cast<uint16_t>(strlen(env.c_str())));
  write_all(fd, env.c_str(), strlen(env.c_str()));

  return !g_pipe_broken;
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
