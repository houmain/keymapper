#pragma once

#include "runtime/KeyEvent.h"
#include "win.h"

extern const int update_interval_ms;

void update_configuration();
bool update_focused_window();
void validate_state(bool check_accessibility);
KeySequence apply_input(KeyEvent event);
void reuse_buffer(KeySequence&& buffer);

int run_interception();
int run_hook(HINSTANCE instance);

void error(const char* format, ...);
void verbose(const char* format, ...);
