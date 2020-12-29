#pragma once

#include <memory>

class Stage;

int initialize_ipc(const char* fifo_filename);
void shutdown_ipc(int fd);
char *read_name(int fd);
std::shared_ptr<Stage> read_config(int fd);
bool update_ipc(int fd, Stage& stage);
