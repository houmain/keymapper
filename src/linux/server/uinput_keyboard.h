#pragma once

class KeySequence;

int create_uinput_keyboard(const char* name);
void destroy_uinput_keyboard(int fd);
bool send_event(int fd, int type, int code, int value);
bool send_key_sequence(int fd, const KeySequence& key_sequence);
