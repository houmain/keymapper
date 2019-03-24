#pragma once

#include "runtime/Key.h"

unsigned int get_scan_code(Key key);
Key get_key(unsigned int scan_code);
