#pragma once

class KeyEvent;

int create_uinput_keyboard(const char* name);
void destroy_uinput_keyboard(int fd);
bool send_event(int fd, int type, int code, int value);
bool send_key_event(int fd, const KeyEvent& event);
bool flush_events(int fd);
