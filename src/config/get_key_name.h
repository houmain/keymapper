#pragma once

#include "runtime/Key.h"
#include <string>

const char* get_key_name(Key key);
Key get_key_by_name(std::string_view key_name);
std::string get_auto_virtual_name(Key index);
