
#include "Settings.h"
#include "ConfigFile.h"
#include "FocusedWindow.h"
#include "runtime/Stage.h"
#include "LimitSingleInstance.h"
#include "common.h"

const auto config_filename = L".keymapper.conf";
const int update_interval_ms = 500;

namespace {
  Settings g_settings;
  ConfigFile g_config_file;
  FocusedWindowPtr g_focused_window;
  std::unique_ptr<Stage> g_stage;
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

void print(const char* message) {
#if !defined(NDEBUG)
  OutputDebugStringA(message);
#else
  static const auto has_console = [](){
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      FILE *stream;
      freopen_s(&stream, "CONOUT$", "w", stdout);
      return true;
    }
    return false;
  }();
  if (has_console) {
    std::fprintf(stdout, message);
  }
  else {
    MessageBoxA(nullptr, message, "Keymapper", MB_ICONINFORMATION);
  }
#endif
}

void update_configuration() {
  if (!g_stage->is_output_down())
    if (g_settings.auto_update_config)
      if (g_config_file.update())
        reset_state();
}

bool update_focused_window() {
  if (!update_focused_window(*g_focused_window))
    return false;

  g_stage->activate_override_set(
    find_context(g_config_file.config(),
      get_class(*g_focused_window),
      get_title(*g_focused_window)));
  return true;
}

KeySequence apply_input(KeyEvent event) {
  return g_stage->apply_input(event);
}

void reuse_buffer(KeySequence&& buffer) {
  g_stage->reuse_buffer(std::move(buffer));
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  LimitSingleInstance single_instance("Global\\{658914E7-CCA6-4425-89FF-EF4A13B75F31}");
  if (single_instance.is_another_instance_running())
    return 1;

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  g_settings.config_file_path = get_user_directory() + L'\\' + config_filename;

  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message(__wargv[0]);
    return 1;
  }

  // load initial configuration
  g_config_file = ConfigFile(g_settings.config_file_path);
  if (!g_config_file.update())
    return 1;
  reset_state();

  if (g_settings.run_interception)
    return run_interception();

  return run_hook(instance);
}
