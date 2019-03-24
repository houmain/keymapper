#pragma once

#include "runtime/Key.h"
#include <string>

const char* get_key_name(const Key& key);

/// case insensitive
Key get_key_by_name(std::string key_name);
