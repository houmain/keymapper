#pragma once

#include <cstdint>
#include "runtime/Key.h"
#include "common/Duration.h"

class ButtonDebouncer {
private:
  int m_mouse_x{ };
  int m_mouse_y{ };

  Clock::time_point m_last_down_time{ };
  Clock::time_point m_last_mouse_down_time{ };
  int m_last_down_x{ };
  int m_last_down_y{ };

public:
  void on_mouse_move(int x, int y);
  Duration on_key_down(Key key, bool prevent_double_click);
};
