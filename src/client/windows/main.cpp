
#include "FocusedWindow.h"
#include "client/Settings.h"
#include "client/ConfigFile.h"
#include "runtime/Stage.h"
#include "common/windows/LimitSingleInstance.h"
#include "common/common.h"
#include "main.h"
#include <array>
#include <optional>
#include <cstdarg>

const auto config_filename = L"keymapper.conf";
const int  update_interval_ms = 50;
const int  update_configuration_rate = 10;

namespace {
  Settings g_settings;
  std::optional<ConfigFile> g_config_file;
  FocusedWindowPtr g_focused_window;
  std::unique_ptr<Stage> g_stage;
  std::vector<int> g_new_active_contexts;
  std::vector<int> g_current_active_contexts;
  bool g_was_inaccessible;
  unsigned int g_update_configuration_count;
  
  std::wstring utf8_to_wide(const std::string& str) {
    auto result = std::wstring();
    result.resize(MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      NULL, 0));
    MultiByteToWideChar(CP_UTF8, 0, 
      str.data(), static_cast<int>(str.size()), 
      result.data(), static_cast<int>(result.size()));
    return result;
  }

  bool starts_with_case_insensitive(const std::string& string, const char* value) {
    for (const auto* a = string.c_str(), *b = value; *b != '\0'; ++a, ++b)
      if (*a == '\0' || 
          std::tolower(static_cast<int>(*a)) != std::tolower(static_cast<int>(*b)))
        return false;
    return true;
  }

  bool execute_terminal_command(const std::string& command) {
    auto cmd = std::wstring(MAX_PATH, ' ');
    cmd.resize(GetSystemDirectoryW(cmd.data(), static_cast<UINT>(cmd.size())));
    cmd += L"\\CMD.EXE";

    auto args = L"/C " + utf8_to_wide(command);

    auto flags = DWORD{ };
    if (!starts_with_case_insensitive(command, "cmd") && 
        !starts_with_case_insensitive(command, "powershell"))
      flags |= CREATE_NO_WINDOW;

    auto startup_info = STARTUPINFOW{ sizeof(STARTUPINFOW) };
    auto process_info = PROCESS_INFORMATION{ };
    if (!CreateProcessW(cmd.data(), args.data(), nullptr, nullptr, FALSE, 
        flags, nullptr, nullptr, &startup_info, &process_info)) 
      return false;
    
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return true;
  }

  void update_active_contexts() {
    const auto& contexts = g_config_file->config().contexts;
    const auto& window_class = get_class(*g_focused_window);
    const auto& window_title = get_title(*g_focused_window);

    g_new_active_contexts.clear();
    for (auto i = 0; i < static_cast<int>(contexts.size()); ++i)
      if (contexts[i].matches(window_class, window_title))
        g_new_active_contexts.push_back(i);

    if (g_new_active_contexts != g_current_active_contexts) {
      verbose("Active contexts updated (%u)", g_new_active_contexts.size());
      g_stage->set_active_contexts(g_new_active_contexts);
      g_current_active_contexts.swap(g_new_active_contexts);
    }
  }
} // namespace

void reset_state() {
  const auto& config = g_config_file->config();

  auto contexts = std::vector<Stage::Context>();
  for (auto& config_context : config.contexts) {
    auto& context = contexts.emplace_back();
    for (const auto& input : config_context.inputs)
      context.inputs.push_back({ std::move(input.input), input.output_index });
    context.outputs = std::move(config_context.outputs);
    for (const auto& output : config_context.command_outputs)
      context.command_outputs.push_back({ std::move(output.output), output.index });
  }
  g_stage = std::make_unique<Stage>(std::move(contexts));
  g_focused_window = create_focused_window();
  update_active_contexts();
}

void execute_action(int triggered_action) {
  if (triggered_action >= 0 &&
      triggered_action < static_cast<int>(g_config_file->config().actions.size())) {
    const auto& action = g_config_file->config().actions[triggered_action];
    const auto& command = action.terminal_command;
    verbose("Executing terminal command '%s'", command.c_str());
    execute_terminal_command(command);
  }
}

void update_configuration() {
  if (!g_settings.auto_update_config)
    return;
  if (g_stage->is_output_down())
    return;
  if (g_update_configuration_count++ % update_configuration_rate)
    return;

  if (g_config_file->update()) {
    verbose("Configuration updated");
    reset_state();

    g_current_active_contexts.clear();
    update_active_contexts();
  }
}

void validate_state(bool check_accessibility) {

  // validate internal state when a window of another user was focused
  if (check_accessibility) {
    if (is_inaccessible(*g_focused_window)) {
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

  verbose("Detected focused window changed:");
  verbose("  class = '%s'", get_class(*g_focused_window).c_str());
  verbose("  title = '%s'", get_title(*g_focused_window).c_str());
  update_active_contexts();
  return true;
}

KeySequence apply_input(KeyEvent event) {
  auto output = g_stage->update(event);
  if (g_stage->should_exit()) {
    verbose("Read exit sequence");
    ::PostQuitMessage(0);
  }
  return output;
}

void reuse_buffer(KeySequence&& buffer) {
  g_stage->reuse_buffer(std::move(buffer));
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  g_settings.config_file_path = config_filename;

  if (!interpret_commandline(g_settings, __argc, __wargv)) {
    print_help_message();
    return 1;
  }

  g_verbose_output = g_settings.verbose;
  g_output_color = !g_settings.no_color;

  LimitSingleInstance single_instance("Global\\{658914E7-CCA6-4425-89FF-EF4A13B75F31}");
  if (single_instance.is_another_instance_running()) {
    error("Another instance is already running");
    return 1;
  }

  SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

  verbose("Loading configuration file '%ws'", g_settings.config_file_path.c_str());
  g_config_file.emplace(g_settings.config_file_path);
  if (!g_config_file->update())
    return 1;
  reset_state();

  if (g_settings.run_interception)
    return run_interception();

  return run_hook(instance);
}
