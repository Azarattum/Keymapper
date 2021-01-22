
#include "keyboard.h"
#include <cstdio>
#include <cerrno>
#include <array>
#include <algorithm>
#include <iterator>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/inotify.h>

const auto EVDEV_MINORS = 32;

namespace {
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

  std::string get_device_name(int fd) {
    auto name = std::array<char, 256>();
    if (::ioctl(fd, EVIOCGNAME(name.size()), name.data()) >= 0)
      return name.data();
    return "";
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

  bool read_event(const std::vector<int>& fds, int cancel_fd,
      int* type, int* code, int* value) {

    auto rfds = fd_set{ };
    FD_ZERO(&rfds);
    auto max_fd = 0;
    for (auto fd : fds) {
      max_fd = std::max(max_fd, fd);
      FD_SET(fd, &rfds);
    }

    if (cancel_fd >= 0) {
      max_fd = std::max(max_fd, cancel_fd);
      FD_SET(cancel_fd, &rfds);
    }

    if (::select(max_fd + 1, &rfds, nullptr, nullptr, nullptr) == -1)
      return false;

    if (cancel_fd >= 0 && FD_ISSET(cancel_fd, &rfds))
       return false;

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
} // namespace

class GrabbedKeyboards {
private:
  const char* m_ignore_device_name = "";
  int m_device_monitor_fd{ -1 };
  std::vector<int> m_event_fds;
  std::vector<int> m_grabbed_keyboard_fds;

public:
  ~GrabbedKeyboards() {
    for (auto keyboard_fd : m_grabbed_keyboard_fds) {
      grab_event_device(keyboard_fd, false);
      ::close(keyboard_fd);
    }

    if (m_device_monitor_fd >= 0)
      ::close(m_device_monitor_fd);
  }

  int device_monitor_fd() const {
    return m_device_monitor_fd;
  }

  const std::vector<int>& grabbed_keyboard_fds() const {
    return m_grabbed_keyboard_fds;
  }

  bool initialize(const char* ignore_device_name) {
    m_ignore_device_name = ignore_device_name;
    m_event_fds.resize(EVDEV_MINORS, -1);
    reset_device_monitor();
    return update();
  }

  void reset_device_monitor() {
    if (m_device_monitor_fd >= 0)
      ::close(m_device_monitor_fd);
    m_device_monitor_fd = create_event_device_monitor();
  }

  bool update() {
    // update grabbed keyboards
    auto event_id = 0;
    for (auto& event_fd : m_event_fds) {
      const auto fd = open_event_device(event_id);
      if (fd >= 0 && is_keyboard(fd)) {
        // keyboard, grab new ones
        if (event_fd < 0 &&
            get_device_name(fd) != m_ignore_device_name &&
            wait_until_keys_released(fd) &&
            grab_event_device(fd, true)) {
          event_fd = ::dup(fd);
        }
      }
      else {
        // no keyboard, ungrab previously grabbed
        if (event_fd >= 0) {
          grab_event_device(event_fd, false);
          ::close(event_fd);
          event_fd = -1;
        }
      }
      if (fd >= 0)
        ::close(fd);
      ++event_id;
    }

    // collect grabbed keyboard fds
    auto grabbed_keyboard_fds = std::vector<int>();
    for (auto event_fd : m_event_fds)
      if (event_fd >= 0)
        grabbed_keyboard_fds.push_back(event_fd);

    // check if they differ from previous list
    if (grabbed_keyboard_fds != m_grabbed_keyboard_fds) {
      m_grabbed_keyboard_fds = std::move(grabbed_keyboard_fds);
      reset_device_monitor();
      return true;
    }
    return false;
  }
};

void FreeGrabbedKeyboards::operator()(GrabbedKeyboards* keyboards) {
  delete keyboards;
}

GrabbedKeyboardsPtr grab_keyboards(const char* ignore_device_name) {
  auto keyboards = GrabbedKeyboardsPtr(new GrabbedKeyboards());
  if (!keyboards->initialize(ignore_device_name))
    return nullptr;
  return keyboards;
}

bool read_keyboard_event(GrabbedKeyboards& keyboards, int* type, int* code, int* value) {
  for (;;) {
    if (read_event(keyboards.grabbed_keyboard_fds(),
          keyboards.device_monitor_fd(), type, code, value))
      return true;

    // cancelled because a device was detected?
    if (!keyboards.update())
      return false;
  }
}
