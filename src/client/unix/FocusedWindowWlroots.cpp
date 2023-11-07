
#if defined(ENABLE_WAYLAND)

#include "FocusedWindowImpl.h"
#include <cstring>
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

static const auto WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION = 1;

class FocusedWindowWLRoots : public FocusedWindowSystem {
private:
  FocusedWindowData& m_data;
  wl_display* m_display{ };
  zwlr_foreign_toplevel_manager_v1* m_toplevel_manager{ };
  void* m_active_toplevel{ };
  bool m_updated{ };

public:
  explicit FocusedWindowWLRoots(FocusedWindowData *data)
    : m_data(*data) {
  }

  FocusedWindowWLRoots(const FocusedWindowWLRoots&) = delete;
  FocusedWindowWLRoots& operator=(const FocusedWindowWLRoots&) = delete;

  ~FocusedWindowWLRoots() {
    if (m_display)
      wl_display_disconnect(m_display);
  }

  bool initialize() {
    m_display = wl_display_connect(nullptr);
    if (!m_display)
      return false;

    auto registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(registry, &registry_listener_impl, this);
    wl_display_roundtrip(m_display);

    if (!m_toplevel_manager)
      return false;

    wl_display_flush(m_display);
    return true;
  }

  bool update() {
    wl_display_roundtrip(m_display);
    return std::exchange(m_updated, false);
  }

private:
  struct Toplevel {
    FocusedWindowWLRoots* self{ };
    std::string title;
    std::string app_id;
  };

  static const wl_registry_listener registry_listener_impl;
  static const zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_impl;
  static const zwlr_foreign_toplevel_handle_v1_listener toplevel_impl;

  void handle_global(wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
      toplevel_manager_init(static_cast<zwlr_foreign_toplevel_manager_v1*>(
        wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
          WLR_FOREIGN_TOPLEVEL_MANAGEMENT_VERSION)));
  }

  void toplevel_manager_init(zwlr_foreign_toplevel_manager_v1* toplevel_manager) {
    zwlr_foreign_toplevel_manager_v1_add_listener(
      toplevel_manager, &toplevel_manager_impl, this);
    m_toplevel_manager = toplevel_manager;
  }

  void toplevel_manager_finished(zwlr_foreign_toplevel_manager_v1* toplevel_manager) {
    zwlr_foreign_toplevel_manager_v1_destroy(toplevel_manager);
    m_toplevel_manager = nullptr;
  }

  void toplevel_manager_handle_toplevel(zwlr_foreign_toplevel_handle_v1* handle) {
    auto toplevel = new(std::nothrow) Toplevel();
    if (!toplevel)
      return;
    toplevel->self = this;
    zwlr_foreign_toplevel_handle_v1_add_listener(
      handle, &toplevel_impl, toplevel);
  }

  void toplevel_handle_title(Toplevel& toplevel, const char* title) {
    toplevel.title = title;
  }

  void toplevel_handle_appid(Toplevel& toplevel, const char* app_id) {
    toplevel.app_id = app_id;
  }

  void toplevel_handle_state(Toplevel& toplevel, wl_array* state) {
    const auto begin = static_cast<uint32_t*>(state->data);
    const auto end = begin + state->size / sizeof(uint32_t);
    for (auto it = begin; it != end; ++it)
      if (*it & ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
        m_active_toplevel = &toplevel;
  }

  void toplevel_handle_done(Toplevel& toplevel) {
    if (&toplevel == m_active_toplevel) {
      m_data.window_title = toplevel.title;
      m_data.window_class = toplevel.app_id;
      m_data.window_path = "";
      m_updated = true;
    }
  }

  void toplevel_handle_closed(Toplevel* toplevel, zwlr_foreign_toplevel_handle_v1 *handle) {
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    delete toplevel;
  }
};

const wl_registry_listener FocusedWindowWLRoots::registry_listener_impl {
  // global
  [](void* self, wl_registry* registry,
      uint32_t name, const char* interface, uint32_t version) {
    static_cast<FocusedWindowWLRoots*>(self)->handle_global(registry, name, interface, version);
  },
  // global_remove
  [](void* self, wl_registry* registry, uint32_t name) {
  }
};

const zwlr_foreign_toplevel_manager_v1_listener FocusedWindowWLRoots::toplevel_manager_impl{
  // toplevel
  [](void* self, zwlr_foreign_toplevel_manager_v1* toplevel_manager,
      zwlr_foreign_toplevel_handle_v1* toplevel) {
    static_cast<FocusedWindowWLRoots*>(self)->toplevel_manager_handle_toplevel(toplevel);
  },
  // finished
  [](void* self, zwlr_foreign_toplevel_manager_v1* toplevel_manager) {
    static_cast<FocusedWindowWLRoots*>(self)->toplevel_manager_finished(toplevel_manager);
  }
};

const zwlr_foreign_toplevel_handle_v1_listener FocusedWindowWLRoots::toplevel_impl{
  // title
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, const char* title) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_title(toplevel, title);
  },
  // app_id
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, const char* app_id) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_appid(toplevel, app_id);
  },
  // output_enter
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) {
  },
  // output_leave
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_output* output) {
  },
  // state
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle, wl_array* state) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_state(toplevel, state);
  },
  // done
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_done(toplevel);
  },
  // closed
  [](void* _toplevel, zwlr_foreign_toplevel_handle_v1* handle) {
    auto& toplevel = *static_cast<FocusedWindowWLRoots::Toplevel*>(_toplevel);
    toplevel.self->toplevel_handle_closed(&toplevel, handle);
  },
  // parent
  [](void* toplevel, zwlr_foreign_toplevel_handle_v1* handle,
      zwlr_foreign_toplevel_handle_v1* parent) {
  }
};

std::unique_ptr<FocusedWindowSystem> make_focused_window_wlroots(FocusedWindowData* data) {
  auto impl = std::make_unique<FocusedWindowWLRoots>(data);
  if (!impl->initialize())
    return { };
  return impl;
}

#endif // ENABLE_WAYLAND
