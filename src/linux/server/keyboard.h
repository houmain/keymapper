#pragma once

int grab_first_keyboard();
void release_keyboard(int fd);
bool read_event(int fd, int* type, int* code, int* value);
