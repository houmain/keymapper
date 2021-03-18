
#include "Settings.h"
#include "ConfigFile.h"
#include "FocusedWindow.h"
#include "runtime/Stage.h"
#include "LimitSingleInstance.h"
#include "common.h"
#include <array>
#include <cstdarg>

const auto config_filename = L"keymapper.conf";
const int update_interval_ms = 50;

namespace {
  Settings g_settings;
  ConfigFile g_config_file;
  FocusedWindowPtr g_focused_window;
  std::unique_ptr<Stage> g_stage;
  bool g_was_inaccessible;
  
  void vprint(bool notify, const char* format, va_list args) {
#if defined(NDEBUG)
    static const auto s_has_console = [](){
      if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE *stream;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        std::fputc('\n', stdout);
        return true;
      }
      return false;
    }();
    if (s_has_console) {
      std::vfprintf(stdout, format, args);
      std::fputc('\n', stdout);
      std::fflush(stdout);
      return;
    }
#endif

    auto buffer = std::array<char, 1024>();
    std::vsnprintf(buffer.data(), buffer.size(), format, args);

#if !defined(NDEBUG)
    OutputDebugStringA(buffer.data());
    OutputDebugStringA("\n");
#else
    if (notify)
      MessageBoxA(nullptr, buffer.data(), "Keymapper", MB_ICONINFORMATION);
#endif
  }
} // namespace

void reset_state() {
  const auto& config = g_config_file.config();

  // mappings
  auto mappings = std::vector<Mapping>();
  for (const auto& command : config.commands)
    mappings.push_back({ command.input, command.default_mapping });

  // mapping overrides
  auto override_sets = std::vector<MappingOverrideSet>();
  for (auto i = 0u; i < config.contexts.size(); ++i) {
    override_sets.emplace_back();
    auto& override_set = override_sets.back();
    for (auto j = 0u; j < config.commands.size(); ++j) {
      const auto& command = config.commands[j];
      for (const auto& context_mapping : command.context_mappings)
        if (context_mapping.context_index == static_cast<int>(i))
          override_set.push_back(
            { static_cast<int>(j), context_mapping.output });
    }
  }
  g_stage = std::make_unique<Stage>(std::move(mappings), std::move(override_sets));
  g_focused_window = create_focused_window();
}

void error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprint(true, format, args);
  va_end(args);
}

void verbose(const char* format, ...) {
  if (g_settings.verbose) {
    va_list args;
    va_start(args, format);
    vprint(false, format, args);
    va_end(args);
  }
}

void update_configuration() {
  if (!g_stage->is_output_down())
    if (g_settings.auto_update_config)
      if (g_config_file.update()) {
        verbose("configuration updated");
        reset_state();
      }
}

void validate_state(bool check_accessibility) {

  // validate internal state when a window of another user was focused
  if (check_accessibility) {
    if (!is_inaccessible(*g_focused_window)) {
      g_was_inaccessible = true;
      return;
    }
    if (!std::exchange(g_was_inaccessible, false))
      return;
  }

  g_stage->validate_state([](KeyCode keycode) {
    const auto vk = MapVirtualKeyA(keycode, MAPVK_VSC_TO_VK_EX);
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
  });
}

bool update_focused_window() {
  if (!update_focused_window(*g_focused_window))
    return false;

  verbose("detected focused window changed:");
  verbose("  class = '%s'", get_class(*g_focused_window).c_str());
  verbose("  title = '%s'", get_title(*g_focused_window).c_str());

  const auto override_set = find_context(g_config_file.config(),
      get_class(*g_focused_window),
      get_title(*g_focused_window));

  if (override_set >= 0) {
    verbose("setting active context #%i:", override_set + 1);
    const auto& context = g_config_file.config().contexts[override_set];
    if (const auto& filter = context.window_class_filter)
      verbose("  class filter = '%s'", filter.string.c_str());
    if (const auto& filter = context.window_title_filter)
      verbose("  title filter = '%s'", filter.string.c_str());
  }
  else {
    verbose("setting no active context");
  }

  g_stage->activate_override_set(override_set);
  return true;
}

KeySequence apply_input(KeyEvent event) {
  return g_stage->apply_input(event);
}

void reuse_buffer(KeySequence&& buffer) {
  g_stage->reuse_buffer(std::move(buffer));
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  g_settings.config_file_path = config_filename;

  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message(__wargv[0]);
    return 1;
  }

  LimitSingleInstance single_instance("Global\\{658914E7-CCA6-4425-89FF-EF4A13B75F31}");
  if (single_instance.is_another_instance_running()) {
    error("another instance is already running");
    return 1;
  }

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  verbose("loading configuration file '%ws'", g_settings.config_file_path.c_str());
  g_config_file = ConfigFile(g_settings.config_file_path);
  if (!g_config_file.update())
    return 1;
  reset_state();

  if (g_settings.run_interception)
    return run_interception();

  return run_hook(instance);
}
