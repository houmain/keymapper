
#include "common.h"

#if defined(ENABLE_INTERCEPTION)

#define INTERCEPTION_STATIC
#include "interception.h"
#include <cassert>

#define INIT_PROC(NAME) const auto NAME = \
  reinterpret_cast<decltype(::NAME)*>(GetProcAddress(handle, #NAME))

namespace {
  KeyEvent get_key_event(const InterceptionKeyStroke& stroke) {
    const auto key = static_cast<KeyCode>(stroke.code |
      (stroke.state & INTERCEPTION_KEY_E0 ? 0xE000u : 0u));
    const auto state = ((stroke.state & INTERCEPTION_KEY_UP) ?
      KeyState::Up : KeyState::Down);
    return KeyEvent{ key, state };
  }

  InterceptionKeyStroke get_interception_stroke(const KeyEvent& event) {
    auto scan_code = static_cast<unsigned short>(event.key);
    auto state = static_cast<unsigned short>(event.state == KeyState::Up ?
      INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN);
    if (scan_code & 0xE000) {
      scan_code &= ~0xE000;
      state |= INTERCEPTION_KEY_E0;
    }
    return InterceptionKeyStroke{ scan_code, state, 0 };
  }
} // namespace

int run_interception() {
  verbose("Loading interception.dll");
  const auto handle = LoadLibraryA("interception.dll");
  if (!handle) {
    error("interception.dll missing");
    return 1;
  }

  INIT_PROC(interception_create_context);
  INIT_PROC(interception_set_filter);
  INIT_PROC(interception_is_keyboard);
  INIT_PROC(interception_wait_with_timeout);
  INIT_PROC(interception_receive);
  INIT_PROC(interception_send);
  if (!interception_create_context ||
      !interception_set_filter ||
      !interception_is_keyboard ||
      !interception_wait_with_timeout ||
      !interception_receive ||
      !interception_send) {
    FreeLibrary(handle);
    error("interception.dll invalid");
    return 1;
  }

  verbose("Initializing interception");
  auto context = interception_create_context();
  if (!context) {
    error("Initializing interception failed");
    return false;
  }

  interception_set_filter(context, interception_is_keyboard,
    INTERCEPTION_FILTER_KEY_DOWN | INTERCEPTION_FILTER_KEY_UP | INTERCEPTION_FILTER_KEY_E0);

  verbose("Entering update loop");
  InterceptionStroke stroke;
  for (;;) {
    auto device = interception_wait_with_timeout(context,
      static_cast<unsigned long>(update_interval_ms));

    if (interception_receive(context, device, &stroke, 1) <= 0) {
      update_configuration();
      update_focused_window();
      continue;
    }

    auto* keystroke = reinterpret_cast<InterceptionKeyStroke*>(&stroke);
    const auto input = get_key_event(*keystroke);
    auto output = apply_input(input);
    for (const auto& event : output) {
      *keystroke = get_interception_stroke(event);
      interception_send(context, device, &stroke, 1);
    }
    reuse_buffer(std::move(output));
  }
  // unreachable for now
  //INIT_PROC(interception_destroy_context);
  //interception_destroy_context(context);
  //FreeLibrary(handle);
  //return 0;
}

#else // !defined(ENABLE_INTERCEPTION)

int run_interception() {
  error("Interception support not compiled in");
  return 1;
}

#endif // !defined(ENABLE_INTERCEPTION)
