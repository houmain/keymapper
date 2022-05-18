#pragma once

#include <chrono>

using Duration = std::chrono::duration<double>;
using Clock = std::chrono::steady_clock;

struct timeval to_timeval(const Duration& duration);
