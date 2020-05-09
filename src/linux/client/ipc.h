#pragma once

struct Config;

int initialize_ipc(const char* fifo_filename);
void shutdown_ipc(int fd);
bool send_config(int fd, const Config& config);
bool send_active_override_set(int fd, int index);
bool is_pipe_broken(int fd);
