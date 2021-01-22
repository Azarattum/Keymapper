#include "ipc.h"
#include "GrabbedKeyboards.h"
#include "uinput_keyboard.h"
#include "runtime/Stage.h"
#include <linux/uinput.h>
#include <regex>

namespace {
  const auto ipc_fifo_filename = "/tmp/hotkeyer";
  const auto uinput_keyboard_name = "Hotkeyer";
}

int main() {
  std::regex escape("(\"|\\\\)");

  // wait for client connection loop
  for (;;) {
    const auto ipc_fd = initialize_ipc(ipc_fifo_filename);
    if (ipc_fd < 0)
      return 1;
    char *name = read_name(ipc_fd);
    char *env = read_env(ipc_fd);

    const auto stage = read_config(ipc_fd);
    if (stage) {
      // client connected
      const auto uinput_fd = create_uinput_keyboard(uinput_keyboard_name);
      if (uinput_fd >= 0) {
        const auto grabbed_keyboards = grab_keyboards(uinput_keyboard_name);
        if (grabbed_keyboards) {
          // main loop
          for (;;) {
            // wait for next key event
            auto type = 0;
            auto code = 0;
            auto value = 0;
            if (!read_keyboard_event(*grabbed_keyboards, &type, &code, &value))
              break;

            // let client update configuration
            if (!stage->is_output_down())
              if (!update_ipc(ipc_fd, *stage))
                break;

            if (type == EV_KEY) {
              // translate key events
              const auto event = KeyEvent{
                static_cast<KeyCode>(code),
                (value == 0 ? KeyState::Up : KeyState::Down),
              };
              auto action = stage->apply_input(event);
              if (action.type == ActionType::Command) {
                std::string execute = "su \"";
                execute.append(std::regex_replace(name, escape, "\\$1"));
                execute.append("\" -c \"");
                execute.append("export");
                execute.append(std::regex_replace(env, escape, "\\$1"));
                execute.append("; ");
                execute.append(std::regex_replace(action.command, escape, "\\$1"));
                execute.append(" &\" > /dev/null 2>&1");

                system(execute.c_str());
              }
              
              send_key_sequence(uinput_fd, action.sequence);
              stage->reuse_buffer(std::move(action.sequence));
            }
            else if (type != EV_SYN &&
                     type != EV_MSC) {
              // forward other events
              send_event(uinput_fd, type, code, value);
            }
          }
        }
      }
      destroy_uinput_keyboard(uinput_fd);
    }
    shutdown_ipc(ipc_fd);
  }
}