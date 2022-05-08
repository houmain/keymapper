#pragma once

#include "runtime/Key.h"
#include <string>

const char* get_key_name(const Key& key);
Key get_key_by_name(std::string_view key_name);
