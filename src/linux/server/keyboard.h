#pragma once

#include <vector>

std::vector<int> grab_keyboards();
void release_keyboards(const std::vector<int>& fds);
bool read_event(const std::vector<int>& fds, int* type, int* code, int* value);
