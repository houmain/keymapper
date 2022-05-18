
#include "ButtonDebouncer.h"
#include <cmath>

#if defined(_WIN32)
# include "common/windows/win.h"
#else
# include <sys/time.h>
#endif

namespace {
#if defined(_WIN32)
  auto double_click_time() {
    const auto extra_time = std::chrono::milliseconds(10);
    return std::chrono::milliseconds(GetDoubleClickTime()) + extra_time;
  }

  int double_click_tolerance_x() {
    const auto SM_CXDOUBLECLICK = 36;
    return GetSystemMetrics(SM_CXDOUBLECLICK);
  }

  int double_click_tolerance_y() {
    const auto SM_CYDOUBLECLICK = 37;
    return GetSystemMetrics(SM_CYDOUBLECLICK);
  }
#else
  std::chrono::milliseconds double_click_time() {
    return std::chrono::milliseconds(500);
  }

  int double_click_tolerance_x() {
    return 5;
  }

  int double_click_tolerance_y() {
    return 5;
  }
#endif
} // namespace

void ButtonDebouncer::on_mouse_move(int x, int y) {
  m_mouse_x = x;
  m_mouse_y = y;
}

Duration ButtonDebouncer::on_key_down(Key key, bool prevent_double_click) {
  const auto now = Clock::now();
  const auto is_mouse_button = ::is_mouse_button(key);

#if defined(__linux)
  // ensure minimum delay between sending modifier and sending mouse button,
  // and between sending mouse button and sending keys,
  // otherwise they are likely applied in the wrong order (X11)
  const auto last_was_mouse_button =
    (m_last_down_time == m_last_mouse_down_time);
  if (is_mouse_button != last_was_mouse_button) {
    const auto min_delay = std::chrono::milliseconds(100);
    const auto next = m_last_down_time + min_delay;
    if (next > now)
      return next - now;
  }
#endif

  if (is_mouse_button) {
    if (prevent_double_click) {
      const auto next = m_last_mouse_down_time + double_click_time();
      const auto moved =
        (std::abs(m_mouse_x - m_last_down_x) > double_click_tolerance_x() ||
         std::abs(m_mouse_y - m_last_down_y) > double_click_tolerance_y());
      if (!moved && next > now)
        return next - now;
    }

    m_last_mouse_down_time = now;
    m_last_down_x = m_mouse_x;
    m_last_down_y = m_mouse_y;
  }

  m_last_down_time = now;
  return { };
}
