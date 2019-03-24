#pragma once

#include <memory>

class Stage;

int initialize_ipc(const char* fifo_filename);
void shutdown_ipc(int fd);
std::unique_ptr<Stage> read_config(int fd);
bool update_ipc(int fd, Stage& stage);
