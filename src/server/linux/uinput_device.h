#pragma once

class KeyEvent;

int create_uinput_device(const char* name, bool add_mouse_functions);
void destroy_uinput_device(int fd);
bool send_event(int fd, int type, int code, int value);
bool send_key_event(int fd, const KeyEvent& event);
