
#if defined(ENABLE_WAYLAND)

#include "StringTyperImpl.h"
#include <cstring>
#include <wayland-client.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include <unistd.h>

class StringTyperWayland : public StringTyperImpl {
private:
  wl_display* m_display{ };
  wl_seat* m_seat{ };
  wl_keyboard* m_keyboard{ };

public:
  ~StringTyperWayland() {
    if (m_display)
      wl_display_disconnect(m_display);
  }

  bool initialize() {
    m_display = wl_display_connect(nullptr);
    if (!m_display)
      return false;

    auto registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(registry, &registry_listener_impl, this);
    for (auto i = 0; i < 5; ++i)
      wl_display_roundtrip(m_display);

    if (m_dictionary.empty())
      return false;

    return true;
  }

private:
  static const wl_registry_listener registry_listener_impl;
  static const wl_seat_listener wl_seat_listener_impl;
  static const wl_keyboard_listener wl_keyboard_listener_impl;

  void handle_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::strcmp(interface, wl_seat_interface.name) == 0) {
      const auto version = 5;
      m_seat = static_cast<wl_seat*>(wl_registry_bind(
        registry, name, &wl_seat_interface, version));
      wl_seat_add_listener(m_seat, &wl_seat_listener_impl, this);
    }
  }

  void handle_seat_capabilities(uint32_t capabilities) {
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
      if (!m_keyboard) {
        m_keyboard = wl_seat_get_keyboard(m_seat);
        wl_keyboard_add_listener(m_keyboard, &wl_keyboard_listener_impl, this);
      }
    }
    else if (m_keyboard) {
      wl_keyboard_release(m_keyboard);
      m_keyboard = nullptr;
    }
  }

  void handle_keyboard_keymap(uint32_t format, int32_t fd, uint32_t size) {
    if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
      auto map_shm = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
      if (map_shm != MAP_FAILED) {
        set_keymap(static_cast<const char*>(map_shm));
        ::munmap(map_shm, size);
      }
      ::close(fd);
    }
  }

  void set_keymap(const char* string) {
    auto context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    auto keymap = xkb_keymap_new_from_string(context, string,
      XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    const auto min = xkb_keymap_min_keycode(keymap);
    const auto max = xkb_keymap_max_keycode(keymap);
    auto symbols = std::add_pointer_t<const xkb_keysym_t>{ };
    auto masks = std::array<xkb_mod_mask_t, 8>{ };

    m_dictionary.clear();
    for (auto keycode = min; keycode < max; ++keycode)
      if (auto name = xkb_keymap_key_get_name(keymap, keycode))
        if (auto key = xkb_keyname_to_key(name); key != Key::none) {
          const auto layouts = xkb_keymap_num_layouts_for_key(keymap, keycode);
          for (auto layout = 0u; layout < layouts; ++layout) {
            const auto levels = xkb_keymap_num_levels_for_key(keymap, keycode, layout);
            for (auto level = 0u; level < levels; ++level) {
              const auto num_symbols = xkb_keymap_key_get_syms_by_level(keymap, keycode,
                layout, level, &symbols);
              const auto num_masks = xkb_keymap_key_get_mods_for_level(keymap, keycode,
                layout, level, masks.data(), masks.size());
              if (num_symbols > 0 && num_masks > 0)
                if (auto character = xkb_keysym_to_utf32(symbols[0]))
                  if (m_dictionary.find(character) == m_dictionary.end())
                    m_dictionary[character] = { key, get_xkb_modifiers(masks[0]) };
            }
          }
        }
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
  }
};

const wl_registry_listener StringTyperWayland::registry_listener_impl {
  // global
  [](void* self, wl_registry* registry,
      uint32_t name, const char* interface, uint32_t version) {
    static_cast<StringTyperWayland*>(self)->handle_global(registry, name, interface, version);
  },
  // global_remove
  [](void* self, wl_registry* registry, uint32_t name) {
  }
};

const wl_seat_listener StringTyperWayland::wl_seat_listener_impl = {
  // capabilities
  [](void* self, wl_seat* seat, uint32_t capabilities) {
    static_cast<StringTyperWayland*>(self)->handle_seat_capabilities(capabilities);
  },
  // name
  [](void* self, wl_seat* wl_seat, const char *name) {
  },
};

const wl_keyboard_listener StringTyperWayland::wl_keyboard_listener_impl = {
  // keymap
  [](void* self, wl_keyboard* wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
    static_cast<StringTyperWayland*>(self)->handle_keyboard_keymap(format, fd, size);
  },
  // enter
  [](void* self, wl_keyboard* wl_keyboard, uint32_t serial, wl_surface* surface,
      wl_array* keys) {
  },
  // leave
  [](void* self, wl_keyboard* wl_keyboard, uint32_t serial, wl_surface* surface) {
  },
  // key
  [](void* self, wl_keyboard* wl_keyboard, uint32_t serial, uint32_t time,
      uint32_t key, uint32_t state) {
  },
  // modifiers
  [](void* self, wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed,
      uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
  },
  // repeat_info
  [](void* self, wl_keyboard* wl_keyboard, int32_t rate, int32_t delay) {
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_wayland() {
  auto impl = std::make_unique<StringTyperWayland>();
  if (!impl->initialize())
    return { };
  return impl;
}

#endif // ENABLE_WAYLAND
