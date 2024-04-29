#pragma once

#include "common/Duration.h"
#include <string>
#include <vector>
#include <optional>

enum class RequestType {
  press,
  release,
  toggle,
  is_pressed,
  is_released,
  wait_pressed,
  wait_released,
  wait_toggled,
  wait,
  set_instance_id,
  restart,
  stdout_result,
  set_config_file,
};

struct Request {
  RequestType type;
  std::string key;
  std::optional<Duration> timeout;
};

struct Settings {
  std::vector<Request> requests;
};

#if defined(_WIN32)
bool interpret_commandline(Settings& settings, int argc, wchar_t* argv[]);
#else
bool interpret_commandline(Settings& settings, int argc, char* argv[]);
#endif
void print_help_message();
