
#include "ipc.h"
#include "keyboard.h"
#include "uinput_keyboard.h"
#include "runtime/Stage.h"
#include <unistd.h> 
#include <linux/uinput.h>
#include <regex> 
#include <pthread.h>

namespace {
  const auto ipc_fifo_filename = "/tmp/keymapper";
  const auto uinput_keyboard_name = "Keymapper";
  const std::vector<int> *keyboards_ptr = nullptr;
  bool is_pipe_broken = false;
}

struct hook_data {
  char *name;
  std::shared_ptr<Stage> stage;
  int ipc_fd;
  int keyboard;
  int input;
};

void *hook(void *thread_arg) {
  std::regex escape("(\"|\\\\)");

  hook_data data = *((hook_data *) thread_arg);
  if (data.input >= 0) {
    // main loop
    while (!is_pipe_broken) {
      // wait for next key event
      auto type = 0;
      auto code = 0;
      auto value = 0;
      if (!read_event(data.keyboard, &type, &code, &value))
        break;

      // let client update configuration
      if (!data.stage->is_output_down())
        if (!update_ipc(data.ipc_fd, *data.stage))
          break;

      if (type == EV_KEY) {
        // translate key events
        const auto event = KeyEvent{
          static_cast<KeyCode>(code),
          (value == 0 ? KeyState::Up : KeyState::Down),
        };
        auto action =  data.stage->apply_input(event);
        if (action.type == ActionType::Command) {
          std::string execute = "su \"";
          execute.append(std::regex_replace(data.name, escape, "\\$1"));
          execute.append("\" -c \"");
          execute.append(std::regex_replace(action.command, escape, "\\$1"));
          execute.append(" &\" > /dev/null 2>&1");

          system(execute.c_str());
        }

        auto output = action.sequence;
        send_key_sequence(data.input, output);
        data.stage->reuse_buffer(std::move(output));
      }
      else if (type != EV_SYN &&
              type != EV_MSC) {
        // forward other events
        send_event(data.input, type, code, value);
      }
    }
    destroy_uinput_keyboard(data.input);
  }
  
  release_keyboard(data.keyboard);
  is_pipe_broken = true;
  pthread_exit(NULL);
}

void release_all() {
  if (!keyboards_ptr) return;
  for (auto &&kb : *keyboards_ptr)
    release_keyboard(kb);
}

int main() {
  atexit(release_all);
  for (;;) {
    is_pipe_broken = false;
    const auto ipc_fd = initialize_ipc(ipc_fifo_filename);
    if (ipc_fd < 0)
      return 1;
    char *name = read_name(ipc_fd);
    if (name) {
      const auto stage = read_config(ipc_fd);
      if (stage) {
        const auto keyboards = grab_all_keyboards();
        if (!keyboards.empty()) {
          keyboards_ptr = &keyboards;
          const auto uinput_fd = create_uinput_keyboard(uinput_keyboard_name);
          pthread_t threads[keyboards.size()];
          hook_data args[keyboards.size()];

          for (int i = 0; i < keyboards.size(); i++)
          {
            args[i].name = name;
            args[i].ipc_fd = ipc_fd;
            args[i].stage = stage;
            args[i].input = uinput_fd;
            args[i].keyboard = keyboards[i];

            pthread_create(&threads[i], NULL, hook, (void *)&args[i]);
          }

          while (!is_pipe_broken) {
            usleep(50 * 1000);
          }
          keyboards_ptr = nullptr;
        }
      }
      delete name;
    }
    shutdown_ipc(ipc_fd);
  }
}
