#include <vector>
#pragma once

std::vector<int> grab_all_keyboards();
void release_keyboard(int fd);
bool read_event(int fd, int* type, int* code, int* value);
