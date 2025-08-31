
#include "ParseConfig.h"
#include "string_iteration.h"
#include "runtime/Key.h"
#include "common/parse_regex.h"
#include "common/expand_path.h"
#include <cassert>
#include <cctype>
#include <istream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <utility>
#include <charconv>

#if defined(__linux)
const char* current_system = "Linux";
#elif defined(_WIN32)
const char* current_system = "Windows";
#elif defined(__APPLE__)
const char* current_system = "MacOS";
#else
#  error unknown system
#endif

namespace {
  using namespace std::placeholders;

  template<typename It>
  std::string_view make_string_view(It begin, It end) {
    return (begin == end ? "" : 
      std::string_view(&*begin, std::distance(begin, end)));
  }

  bool is_alpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c));
  }

  char to_lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  bool starts_with(std::string_view str, std::string_view with) {
    return (str.size() >= with.size() && str.substr(0, with.size()) == with);
  }

  bool starts_with_lower_case(std::string_view str) {
    return (!str.empty() && 
      is_alpha(str.front()) &&
      to_lower(str.front()) == str.front());
  }

  bool equal_case_insensitive(std::string_view a, std::string_view b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
      [](char a, char b) { return to_lower(a) == to_lower(b); });
  }

  bool contains(const KeySequence& sequence, Key key) {
    return std::find_if(cbegin(sequence), cend(sequence),
      [&](const KeyEvent& event) {
        return event.key == key;
      }) != cend(sequence);
  }

  template<typename T>
  bool contains(const std::vector<T>& vector, const T& item) {
    return std::find(cbegin(vector), cend(vector), item) != cend(vector);
  }

  void replace_key(KeySequence& sequence, Key both, Key key) {
    std::for_each(begin(sequence), end(sequence),
      [&](KeyEvent& event) {
        if (event.key == both)
          event.key = key;
      });
  }

  void replace_not_key(KeySequence& sequence, Key both, Key left,
                       Key right) {
    for (auto it = begin(sequence); it != end(sequence); ++it)
      if (it->state == KeyState::Not && it->key == both) {
        it->key = right;
        it = sequence.insert(it, { left, KeyState::Not });
        ++it;
      }
  }

  std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
      s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
      s.remove_suffix(1);
    return s;
  }

  std::string_view unquote(std::string_view s) {
    if (s.size() >= 2 &&
        s.front() == s.back() &&
        (s.front() == '\'' || s.front() == '"'))
      s = s.substr(1, s.size() - 2);
    return s;
  }

  std::vector<std::string> get_argument_list(const std::string& list) {
    assert(!list.empty() && list.front() == '[' && list.back() == ']');
    auto it = std::next(list.begin());
    const auto end = list.end();
    auto begin = it;
    auto arguments = std::vector<std::string>();
    while (begin != end) {
      auto next_open = it;
      skip_until_not_in_string(&it, end, ",");
      if (it > begin) {
        // check if the comma is within a sub argument list
        if (skip_until_not_in_string(&next_open, end, "[") &&
            next_open < it) {
          it = std::prev(next_open);
          skip_arglist(&it, end);
          continue;
        }
        auto argument = make_string_view(begin, std::prev(it));
        arguments.emplace_back(trim(argument));
      }
      begin = it;
    }
    return arguments;
  }

  std::string substitute_arguments(std::string_view text,
      const std::vector<std::string>& arguments) {
    auto it = text.begin();
    const auto end = text.end();
    auto result = std::string();
    for (auto begin = it; it != end; begin = it) {
      if (!skip_until(&it, end, '$')) {
        result.append(begin, end);
        break;
      }
      // argument or beginning of terminal command
      result.append(begin, it - 1);
      begin = it - 1;
      if (auto number = try_read_number(&it, end)) {
        const auto index = static_cast<size_t>(*number);
        // ignore missing arguments
        if (index < arguments.size())
          result.append(unquote(arguments[index]));
        continue;
      }
      else if (skip(&it, end, '$')) {
        // substitute $$ with argument count
        result.append(std::to_string(arguments.size()));
        continue;
      }
      result.append(begin, it);
    }
    return result;
  }

  std::optional<int> parse_int(std::string_view string) {
    auto result = 0;
    const auto [p, ec] = std::from_chars(string.data(), 
      string.data() + string.size(), result);
    if (ec != std::errc())
      return std::nullopt;
    return result;
  }

  bool contains_variable(std::string_view string) {
    auto it = string.begin();
    const auto end = string.end();
    while (skip_until_not_in_string(&it, end, "$")) {
      auto begin = it;
      if (skip_ident(&it, end) && 
          parse_int(make_string_view(begin, it)))
        return true;
    }
    return false;
  }

  int get_formal_argument_count(std::string_view expression) {
    auto max = 0;
    auto it = expression.begin();
    const auto end = expression.end();
    while (skip_until(&it, end, '$'))
      if (auto number = try_read_number(&it, end))
        max = std::max(*number + 1, max);
    return max;
  }
} // namespace

Config ParseConfig::operator()(std::istream& is,
    const std::filesystem::path& base_path) try {
  m_parsing_done = false;
  m_base_path = base_path;
  m_filename = { };
  m_line_no = 0;
  m_include_level = 0;
  m_preprocess_level = 0;
  m_config = { };
  m_commands.clear();
  m_macros.clear();
  m_logical_keys.clear();
  m_system_filter_matched = true;
  m_after_empty_context_block = false;
  m_enforce_lowercase_commands = { };
  m_allow_unmapped_commands = { };
  m_forward_modifiers.clear();
  m_next_auto_virtual = { Key::first_auto_virtual };
  m_line_auto_virtual.reset();
  set_string_typer_compose_key(Key::none, { });

  // add default context
  auto& default_context = m_config.contexts.emplace_back();
  default_context.begin_stage = true;
  default_context.system_filter_matched = true;

  // register common logical keys
  add_logical_key("Shift", Key::ShiftLeft, Key::ShiftRight);
  add_logical_key("Control", Key::ControlLeft, Key::ControlRight);
  add_logical_key("Alt", Key::AltLeft, Key::AltRight);
  add_logical_key("Meta", Key::MetaLeft, Key::MetaRight);
  
  parse_file(is);

  // check if there is a mapping for each command (to reduce typing errors)
  if (!m_allow_unmapped_commands)
    for (const auto& command : m_commands)
      if (!command.mapped)
        throw ConfigError("Command '" + command.name + "' was not mapped");

  // prepend forward-modifier mappings in each stage. e.g.: ShiftLeft >> ShiftLeft
  prepend_forward_modifier_mappings();

  // remove contexts of other systems or which are empty
  optimize_contexts();

  // replace logical keys (in reverse order of registration)
  for (auto it = m_logical_keys.rbegin(); it != m_logical_keys.rend(); ++it) {
    const auto& [name, both, left, right] = *it;
    replace_logical_key(both, left, right);
  }

  suppress_forwarded_modifiers_in_outputs();

  // collect virtual key aliases
  for (const auto& [name, value] : m_macros)
    if (auto key = get_key_by_name(value); is_virtual_key(key))
      m_config.virtual_key_aliases.emplace_back(name, key);

  return std::move(m_config);
}
catch (const ConfigError&) {
  throw;
}
catch (const std::exception& ex) {
  error(ex.what());
}

void ParseConfig::error(std::string message) const {
  if (!m_filename.empty()) {
    message += " in file '";
    message += m_filename;
    message += "'";
  }
  if (m_line_no) {
    message += " in line ";
    message += std::to_string(m_line_no);
  }
  throw ConfigError(std::move(message));
}

void ParseConfig::parse_file(std::istream& is, std::string filename) {
  auto prev_filename = std::exchange(m_filename, std::move(filename));
  const auto prev_line_no = std::exchange(m_line_no, 0);

  auto line = std::string();
  auto prev_line = std::string();
  while (is.good() && !m_parsing_done) {
    std::getline(is, line);

    // skip UTF-8 BOM
    if (m_line_no == 0 && starts_with(line, "\xEF\xBB\xBF"))
      line.erase(0, 3);

    ++m_line_no;

    // allow to join lines with '\'
    auto end = line.end();
    trim_space(line.begin(), &end);
    if (end != line.begin() && *std::prev(end) == '\\') {
      prev_line += std::string(line.begin(), std::prev(end));
      continue;
    }
    if (!prev_line.empty()) {
      prev_line += line;
      line = std::move(prev_line);
    }
    parse_line(std::move(line));
  }

  m_line_no = prev_line_no;
  m_filename = std::move(prev_filename);
}

void ParseConfig::parse_line(std::string line) {
  reset_line_auto_virtual();

  auto it = line.begin();
  auto end = line.end();
  skip_space(&it, end);

  auto first_ident = read_ident(&it, end);
  skip_space(&it, end);
  if (skip(&it, end, "=")) {
    skip_space(&it, end);
    if (!parse_logical_key_definition(first_ident, it, end))
      parse_macro(std::move(first_ident), it, end);
    return;
  }

  line = preprocess(std::move(line));
  it = line.begin();
  end = line.end();
  skip_space(&it, end);

  if (skip(&it, end, "@")) {
    parse_directive(it, end);
  }
  else if (skip(&it, end, "[")) {
    parse_context(it, end);

    // no fallthrough of stage blocks
    if (m_config.contexts.back().begin_stage) {
      m_after_empty_context_block = false;
    }
    else {
      if (m_after_empty_context_block)
        std::prev(m_config.contexts.end(), 2)->fallthrough = true;

      m_after_empty_context_block = true;
    }
  }
  else {
    m_after_empty_context_block = false;

    // allow to separate multiple mappings with ;
    while (it != end) {
      auto begin = it;
      if (skip_until_not_in_string(&it, end, ';', true)) {
        parse_mapping(begin, it - 1);
      }
      else {
        parse_mapping(begin, end);
      }
    }
  }
}

void ParseConfig::parse_mapping(It it, It end) {
  const auto begin = it;
  auto first_ident = read_ident(&it, end);
  skip_space(&it, end);

  if (!first_ident.empty() && skip(&it, end, ">>")) {
    if (find_command(first_ident)) {
      // mapping command
      parse_mapping(first_ident, it, end);
    }
    else if (starts_with_lower_case(first_ident)) {
      // mapping undefined command
      if (!m_allow_unmapped_commands)
        error("Unknown command '" + first_ident + "'");

      // still validate output
      parse_output(it, end);
    }
    else {
      // directly mapping single key
      parse_command_and_mapping(
        cbegin(first_ident), cend(first_ident), it, end);
    }
  }
  else if (skip_until(&it, end, ">>")) {
    // directly mapping key sequence
    parse_command_and_mapping(begin, it - 2, it, end);
  }
  else {
    error("Missing '>>'");
  }
}

void ParseConfig::add_toggle_active_context(KeySequence input) {
  auto& contexts = m_config.contexts;
  contexts.emplace_back();

  // automatically prepend ? to input
  if (input.at(0).state != KeyState::NoMightMatch)
    input.insert(input.begin(), { Key::none, KeyState::NoMightMatch });

  add_mapping(std::move(input), { 
    { add_action(Config::ActionType::toggle_active), KeyState::Down },
    { Key::none, KeyState::OutputOnRelease },
  });
  // rotate new context to the front
  std::rotate(contexts.rbegin(), contexts.rbegin() + 1, contexts.rend());
}

void ParseConfig::parse_directive(It it, const It end) {
  if (!m_system_filter_matched)
    return;

  const auto add_grab_device_filter = [&](bool invert, bool by_id) {
    auto& filter = m_config.grab_device_filters.emplace_back();
    static_cast<Filter&>(filter) = read_filter(&it, end, invert);
    filter.by_id = by_id;
  };

  const auto read_optional_bool = [&]() {
    auto value = read_ident(&it, end);
    if (value.empty() || value == "true")
      return true;
    if (value == "false")
      return false;
    error("Unexpected '" + value + "'");
  };

  const auto ident = read_ident(&it, end);
  skip_space(&it, end);
  if (ident == "include" || ident == "include-optional") {
    if (++m_include_level > 10)
      error("Recursive includes detected");

    auto filename =
      m_config.include_filenames.emplace_back(m_base_path / 
        expand_path(read_string(&it, end))).string();

    auto is = std::ifstream(filename);
    if (is.good()) {
      parse_file(is, std::move(filename));
    }
    else if (ident == "include") {
      error("Opening include file '" + filename + "' failed");
    }
    --m_include_level;
  }
  else if (ident == "grab-device") {
    add_grab_device_filter(false, false);
  }
  else if (ident == "skip-device") {
    add_grab_device_filter(true, false);
  }
  else if (ident == "grab-device-id") {
    add_grab_device_filter(false, true);
  }
  else if (ident == "skip-device-id") {
    add_grab_device_filter(true, true);
  }
  else if (ident == "enforce-lowercase-commands") {
    m_enforce_lowercase_commands = read_optional_bool();
  }
  else if (ident == "allow-unmapped-commands") {
    m_allow_unmapped_commands = read_optional_bool();
  }
  else if (ident == "forward-modifiers") {
    m_forward_modifiers = parse_forward_modifiers_list(&it, end);
  }
  else if (ident == "done") {
    m_parsing_done = true;
  }
  else if (ident == "toggle-active") {
    add_toggle_active_context(parse_input(it, end));
    it = end;
  }
  else if (ident == "linux-compose-key" ||
           ident == "compose-key") {
    auto compose_key = parse_input(it, end);
    it = end;
    if (compose_key.size() == 2 && compose_key[0].key != Key::AltRight) {
      set_string_typer_compose_key(compose_key[0].key, { });
    }
    else if (compose_key.size() == 4 && compose_key[0].key == Key::AltRight) {
      set_string_typer_compose_key(compose_key[1].key, StringTyper::Modifier::AltGr);
    }
    else {
      error("Invalid compose key");
    }
  }
  else if (ident == "linux-highres-wheel-events" ||
           ident == "macos-iso-keyboard" ||
           ident == "macos-toggle-fn") {
    if (read_optional_bool())
      m_config.server_directives.push_back(ident);
  }
  else if (ident == "options") {
    const auto add_option = [&](const std::string& name) {
      using Option = std::pair<const char*, Config::Option>;
      for (auto [option_name, option] : std::initializer_list<Option> {
          { "update", Config::Option::auto_update_config },
          { "no-update", Config::Option::no_auto_update_config },
          { "verbose", Config::Option::verbose },
          { "no-tray", Config::Option::no_tray_icon },
          { "no-notify", Config::Option::no_notify },
        })
        if (name == option_name)
          return m_config.options.push_back(option);

      error("Unknown option '" + name + "'");
    };
    auto ss = std::istringstream(std::string(it, end));
    auto name = std::string();
    while (std::getline(ss, name, ' '))
      add_option(name);
    it = end;
  }
  else {
    error("Unknown directive '" + ident + "'");
  }

  skip_space(&it, end);
  if (it != end)
    error("Unexpected '" + std::string(it, end) + "'");
}

std::string ParseConfig::read_filter_string(It* it, const It end) {
  const auto begin = *it;
  if (skip_regular_expression(it, end)) {
    return std::string(begin, *it);
  }
  else if (skip_string(it, end)) {
    return std::string(begin + 1, *it - 1);
  }
  else {
    skip_ident(it, end);
    auto ident = preprocess(std::string(begin, *it));

    // trim quotes after macro substitution
    if (!ident.empty() && (ident.front() == '\'' || ident.front() == '\"')) {
      if (ident.size() < 2 || ident.back() != ident.front())
        error("Unterminated string");
      ident = ident.substr(1, ident.size() - 2);
    }
    return ident;
  }
}

Filter ParseConfig::read_filter(It* it, const It end, bool invert) {
  auto string = read_filter_string(it, end);
  if (string.empty())
    error("String expected");
  if (is_regex(string)) {
    auto regex = parse_regex(string);
    return { std::move(string), std::move(regex), invert };
  }
  return { std::move(string), { }, invert };
}

KeySequence ParseConfig::parse_modifier_list(std::string_view string) {
  auto it = string.begin();
  const auto end = string.end();
  auto sequence = KeySequence();
  for (;;) {
    skip_space(&it, end);
    if (it == end)
      break;
    const auto is_not = skip(&it, end, "!");
    const auto name = read_ident(&it, end);
    const auto key = get_key_by_name(name);
    if (!*key)
      error("Invalid key name '" + name + "'");
    if (key == Key::ContextActive)
      error("Not allowed key ContextActive");
    if (!contains(sequence, key))
      sequence.emplace_back(key, (is_not ? KeyState::Not : KeyState::Down));
  }
  return sequence;
}

std::vector<Key> ParseConfig::parse_forward_modifiers_list(It* it, const It end) {
  auto modifiers = std::vector<Key>();
  while (*it != end) {
    const auto name = read_ident(it, end);
    if (name.empty())
      error("Key name expected");

    const auto it2 = std::find_if(m_logical_keys.begin(), m_logical_keys.end(),
      [&](const LogicalKey& key) { return key.name == name; });
    if (it2 != m_logical_keys.end()) {
      for (auto key : { it2->left, it2->right })
        if (!contains(modifiers, key))
          modifiers.push_back(key);
    }
    else if (const auto key = ::get_key_by_name(name); is_device_key(key)) {
      if (!contains(modifiers, key))
        modifiers.push_back(key);
    }
    else {
      error("Invalid key '" + name + "'");
    }
    skip_space(it, end);
  }
  return modifiers;
}

void ParseConfig::parse_context(It it, const It end) {
  auto& context = m_config.contexts.emplace_back();

  skip_space(&it, end);
  if (skip(&it, end, "default")) {
    skip_space(&it, end);
    if (!skip(&it, end, "]"))
      error("Missing ']'");
  }
  else if (skip(&it, end, "stage")) {
    skip_space(&it, end);
    if (!skip(&it, end, "]"))
      error("Missing ']'");
    context.begin_stage = true;
  }
  else {
    for (;;) {
      const auto attrib = read_ident(&it, end);
      if (attrib.empty())
        error("Identifier expected");

      skip_space(&it, end);
      const auto invert = skip(&it, end, "!");
      if (!skip(&it, end, "="))
        error("Missing '='");

      skip_space(&it, end);
      if (attrib == "class") {
        context.window_class_filter = read_filter(&it, end, invert);
      }
      else if (attrib == "title") {
        context.window_title_filter = read_filter(&it, end, invert);
      }
      else if (attrib == "path") {
        context.window_path_filter = read_filter(&it, end, invert);
      }
      else if (attrib == "system") {
        const auto system = read_value(&it, end);
        context.system_filter_matched = equal_case_insensitive(
          system, current_system) ^ invert;
      }
      else if (attrib == "device") {
        context.device_filter = read_filter(&it, end, invert);
      }
      else if (attrib == "device-id" ||
               attrib == "device_id") { // deprecated
        context.device_id_filter = read_filter(&it, end, invert);
      }
      else if (attrib == "modifier") {
        const auto modifier = preprocess(read_value(&it, end));
        context.modifier_filter = parse_modifier_list(modifier);
        context.invert_modifier_filter = invert;
      }
      else {
        error("Unexpected '" + attrib + "'");
      }

      skip_space(&it, end);
      if (skip(&it, end, "]"))
        break;

      // allow to separate with commas
      skip(&it, end, ',');

      skip_space(&it, end);
      if (it == end)
        error("Missing ']'");
    }
  }
  m_system_filter_matched = context.system_filter_matched;

  skip_space(&it, end);
  if (it != end)
    error("Unexpected '" + std::string(it, end) + "'");
}

void ParseConfig::parse_mapping(const std::string& name, It begin, It end) {
  add_mapping(name, parse_output(begin, end));
}

bool is_ident(const std::string& string) {
  auto it = string.begin();
  const auto end = string.end();
  skip_ident(&it, end);
  return (it == end);
}

std::string ParseConfig::parse_command_name(It it, It end) const {
  skip_space(&it, end);
  auto ident = read_ident(&it, end);
  skip_space(&it, end);
  if (it != end ||
      !is_ident(ident) ||
      *get_key_by_name(ident) ||
      (m_enforce_lowercase_commands && !starts_with_lower_case(ident)))
    return { };

  return ident;
}

void ParseConfig::parse_command_and_mapping(const It in_begin, const It in_end,
    const It out_begin, const It out_end) {
  auto input = parse_input(in_begin, in_end);
  auto command_name = parse_command_name(out_begin, out_end);
  if (!command_name.empty())
    add_command(std::move(input), std::move(command_name));
  else
    add_mapping(std::move(input), parse_output(out_begin, out_end));
}

KeySequence ParseConfig::parse_input(It it, It end) {
  skip_space(&it, end);
  return m_parse_sequence(std::string(it, end), true,
    std::bind(&ParseConfig::get_key_by_name, this, _1));
}

Key ParseConfig::get_key_by_name(std::string_view name) const {
  if (const auto key = ::get_key_by_name(name); key != Key::none)
    return key;

  const auto it = std::find_if(m_logical_keys.begin(), m_logical_keys.end(),
    [&](const LogicalKey& key) { return key.name == name; });
  if (it != m_logical_keys.end())
    return it->both;

  return { };
}

Key ParseConfig::add_action(Config::ActionType type, std::string_view value) {
  const auto action_key_code =
    static_cast<Key>(*Key::first_action + m_config.actions.size());
  m_config.actions.push_back({ type, std::string(value) });
  return action_key_code;
}

KeySequence ParseConfig::parse_output(It it, It end) {
  skip_space(&it, end);
  auto sequence = m_parse_sequence(std::string(it, end), false,
    std::bind(&ParseConfig::get_key_by_name, this, _1),
    std::bind(&ParseConfig::add_action, this, Config::ActionType::terminal_command, _1));
  if (contains(sequence, Key::ContextActive))
    error("Not allowed key ContextActive");
  return sequence;
}

void ParseConfig::parse_macro(std::string name, It it, It end) {
  auto preprocessed = preprocess(it, end, false);
  if (m_system_filter_matched || !m_macros.count(name))
    m_macros[std::move(name)] = std::move(preprocessed);
}

bool ParseConfig::parse_logical_key_definition(
    const std::string& logical_name, It it, const It end) {
  if (*get_key_by_name(logical_name))
    return false;

  auto left = get_key_by_name(preprocess(read_ident(&it, end)));
  skip_space(&it, end);
  if (!*left || !skip(&it, end, "|"))
    return false;

  for (;;) {
    skip_space(&it, end);
    const auto name = preprocess(read_ident(&it, end));
    const auto right = get_key_by_name(name);
    if (!*right)
      error("Invalid key '" + name + "'");
    skip_space(&it, end);
    if (skip(&it, end, "|")) {
      left = add_logical_key("$", left, right);
      continue;
    }
    add_logical_key(logical_name, left, right);
    skip_space(&it, end);
    if (it != end)
      error("Unexpected '" + std::string(it, end) + "'");
    break;
  }
  return true;
}

std::string ParseConfig::apply_builtin_macro(const std::string& ident,
    const std::vector<std::string>& arguments) const {
  
  if (ident == "length") {
    if (arguments.size() != 1)
      error("Invalid argument count");
    auto string = std::string_view(arguments[0]);
    if (string.size() < 2 || 
        (string.front() != '"' && string.back() != '\'') ||
        string.front() != string.back())
      error("String literal expected");
    return std::to_string(string.size() - 2);
  }

  if (ident == "add" ||
      ident == "sub" ||
      ident == "mul" ||
      ident == "div" ||
      ident == "mod" ||
      ident == "min" ||
      ident == "max") {
    if (arguments.size() != 2)
      error("Invalid argument count");
    const auto a = parse_int(arguments[0]);
    const auto b = parse_int(arguments[1]);
    if (!a.has_value() || !b.has_value())
      error("Number expected");
    if (ident == "add") return std::to_string(*a + *b);
    if (ident == "sub") return std::to_string(*a - *b);
    if (ident == "mul") return std::to_string(*a * *b);
    if (ident == "div") return (*b ? std::to_string(*a / *b) : "");
    if (ident == "mod") return (*b ? std::to_string(*a % *b) : "");
    if (ident == "min") return std::to_string(std::min(*a, *b));
    if (ident == "max") return std::to_string(std::max(*a, *b));
  }

  if (ident == "repeat") {
    if (arguments.size() != 2)
      error("Invalid argument count");
    const auto count = parse_int(arguments[1]);
    if (!count.has_value())
      error("Number expected");
    auto result = std::string();
    for (auto i = 0; i < *count; ++i) {
      result.append(arguments[0]);
      result.append(" ");
    }
    return result;
  }

  if (ident == "default") {
    if (arguments.size() != 2)
      error("Invalid argument count");
    const auto a = arguments[0];
    const auto b = arguments[1];
    return (!a.empty() ? a : b);
  }

  if (ident == "apply") {
    if (arguments.size() < 1)
      error("Invalid argument count");
    const auto arg_count = get_formal_argument_count(arguments[0]);
    if (!arg_count)
      return "";
    auto result = std::ostringstream();
    const auto is_mapping = (arguments[0].find(">>") != std::string::npos);
    const auto end = static_cast<int>(arguments.size()) - arg_count + 1;
    for (auto i = 1; i < end; i += arg_count) {
      if (is_mapping && i > 1)
        result << ";\n";
      const auto args_begin = std::next(arguments.begin(), i);
      const auto args_end = std::next(args_begin, arg_count);
      assert(args_end <= arguments.end());
      result << substitute_arguments(unquote(arguments[0]),
        { args_begin, args_end });
    }
    return result.str();
  }

  error("Unknown macro '" + ident + "'");
}

std::string ParseConfig::substitute_variables(std::string string) const {
  auto it = string.begin();
  const auto end = string.end();
  auto result = std::string();
  for (;;) {
    const auto begin = it;
    if (!skip_until(&it, end, '$')) {
      result.append(begin, end);
      break;
    }

    const auto var_begin = std::prev(it);
    const auto skipped_brace = skip(&it, end, '{');
    auto ident = read_ident(&it, end);
    if ((skipped_brace && !skip(&it, end, '}')) ||
        m_macros.find(ident) == m_macros.end()) {
      result.append(begin, it);
      continue;
    }
    result.append(begin, var_begin);
    result.append(unquote(preprocess(std::move(ident))));
  }
  return result;
}

std::string ParseConfig::preprocess(std::string expression) const {
  if (++m_preprocess_level > 30)
    error("Recursive macro instantiation");

  const struct Guard {
    int& level;
    ~Guard() { --level; }
  } decrement_level{ m_preprocess_level };

  // simply substitute when expression is a single identifier
  auto it = expression.begin();
  const auto end = expression.end();
  if (skip_ident(&it, end) && 
      it == end) {

    if (!m_prevent_auto_virtual_substitution &&
        expression == "Virtual")
      return get_auto_virtual_name(get_line_auto_virtual());

    const auto macro = m_macros.find(expression);
    if (macro != cend(m_macros))
      return preprocess(macro->second);
    return expression;
  }
  return preprocess(expression.begin(), expression.end());
}

std::string ParseConfig::preprocess(It it, const It end, 
    bool apply_arguments) const {
  auto result = std::string();
  // remove comments
  skip_space(&it, end);

  for (;;) {
    if (it == end)
      break;

    auto begin = it;
    if (skip_ident(&it, end)) {
      // an ident
      auto ident = std::string(begin, it);
      begin = it;
      if (!skip_arglist(&it, end)) {
        result.append(preprocess(std::move(ident)));
      }
      else if (!apply_arguments) {
        // do not apply arguments during macro definition
        result.append(std::move(ident));
        result.append(std::string(begin, it));
      }
      else {
        // apply macro arguments
        // keep Virtual unsubstituted until after apply
        m_prevent_auto_virtual_substitution = (ident == "apply");
        auto arguments = get_argument_list(preprocess(begin, it));
        m_prevent_auto_virtual_substitution = false;

        const auto macro = m_macros.find(ident);
        if (macro != cend(m_macros)) {
          ident = substitute_arguments(macro->second, arguments);
        }
        else {
          ident = apply_builtin_macro(ident, arguments);
        }
        
        // preprocess result again only if it does not contain new variables
        if (!contains_variable(ident)) {
          begin = it;
          skip_arglists(&it, end);
          ident = preprocess(ident + std::string(begin, it));
        }
        result.append(std::move(ident));
      }
    }
    else if (skip_string(&it, end) ||
             skip_terminal_command(&it, end) ||
             skip_regular_expression(&it, end)) {
      result.append(substitute_variables(std::string(begin, it)));
    }
    else if (*it == '#') {
      break;
    }
    else if (*it == '\n') {
      // reset line's auto virtual when line was generated with apply
      reset_line_auto_virtual();
      ++it;
    }
    else {
      // single character
      result.append(begin, ++it);
    }
  }
  return result;
}

void ParseConfig::reset_line_auto_virtual() const {
  m_line_auto_virtual.reset();
}

Key ParseConfig::get_line_auto_virtual() const {
  if (!m_line_auto_virtual) {
    if (m_next_auto_virtual == Key::last_virtual)
      error("Too many virtual keys");
    m_line_auto_virtual.emplace(m_next_auto_virtual);
    m_next_auto_virtual = static_cast<Key>(*m_next_auto_virtual + 1);
  }
  return *m_line_auto_virtual;
}

Config::Context& ParseConfig::current_context() {
  assert(!m_config.contexts.empty());
  return m_config.contexts.back();
}

auto ParseConfig::find_command(const std::string& name) -> Command* {
  const auto it = std::find_if(
    begin(m_commands), end(m_commands),
    [&](const Command& c) { return c.name == name; });
  return (it != cend(m_commands) ? &*it : nullptr);
}

void ParseConfig::add_command(KeySequence input, std::string name) {
  assert(!name.empty());
  assert(!m_enforce_lowercase_commands || starts_with_lower_case(name));
  auto& context = current_context();
  auto command = find_command(name);
  if (!command) {
    // command outputs have a negative index
    const auto output_index = -static_cast<int>(m_commands.size() + 1);
    m_commands.push_back({ std::move(name), output_index, false });
    command = &m_commands.back();
  }
  context.inputs.push_back({ std::move(input), command->index });
}

void ParseConfig::add_mapping(KeySequence input, KeySequence output) {
  assert(!input.empty());
  auto& context = current_context();
  context.inputs.push_back({
    std::move(input),
    static_cast<int>(context.outputs.size())
  });
  context.outputs.push_back(std::move(output));
}

void ParseConfig::add_mapping(const std::string& name, KeySequence output) {
  assert(!name.empty());
  auto& context = current_context();
  auto command = find_command(name);
  if (!command)
    error("Unknown command '" + name + "'");

  if (std::count_if(begin(context.command_outputs), end(context.command_outputs),
      [&](const Config::CommandOutput& output) { return output.index == command->index; }))
    error("Duplicate mapping of '" + name + "'");

  context.command_outputs.push_back({
    std::move(output),
    command->index
  });
  command->mapped = true;
}

Key ParseConfig::add_logical_key(std::string name, Key left, Key right) {
  const auto both = static_cast<Key>(*Key::first_logical + m_logical_keys.size());
  m_logical_keys.push_back({ std::move(name), both, left, right });
  return both;
}

void ParseConfig::replace_logical_key(Key both, Key left, Key right) {
  auto& contexts = m_config.contexts;
  for (auto& context : contexts) {
    // replace !<both> with !<left> !<right>
    for (auto& input : context.inputs)
      replace_not_key(input.input, both, left, right);
    for (auto& output : context.outputs)
      replace_not_key(output, both, left, right);
    for (auto& command : context.command_outputs)
      replace_not_key(command.output, both, left, right);

    // duplicate command and replace the logical with a physical key
    for (auto it = begin(context.inputs); it != end(context.inputs); ++it)
      if (contains(it->input, both)) {
        // duplicate and replace with <left> and <right>
        it = context.inputs.insert(it, *it);
        replace_key(it->input, both, left);
        ++it;
        replace_key(it->input, both, right);

        // when directly mapped output also contains logical key,
        // then duplicate output and replace with <right>
        if (it->output_index >= 0) {
          auto& output = context.outputs[it->output_index];
          if (contains(output, both)) {
            it->output_index = static_cast<int>(context.outputs.size());
            context.outputs.push_back(output);
            replace_key(context.outputs.back(), both, right);
          }
        }
      }

    // replace logical key with <left>
    for (auto& output : context.outputs)
      replace_key(output, both, left);
    for (auto& command : context.command_outputs)
      replace_key(command.output, both, left);

    // replace !<both> with !<left> !<right> in context modifier
    replace_not_key(context.modifier_filter, both, left, right);
  }

  // insert fallthrough context and replace the logical with a physical key
  for (auto it = begin(contexts); it != end(contexts); ++it) {
    if (contains(it->modifier_filter, both)) {
      // duplicate and replace with <left> and <right>
      it = contexts.insert(it, *it);
      it->inputs.clear();
      it->outputs.clear();
      it->command_outputs.clear();
      it->fallthrough = true;
      replace_key(it->modifier_filter, both, left);
      ++it;
      replace_key(it->modifier_filter, both, right);
    }
  }
}

void ParseConfig::optimize_contexts() {
  auto& contexts = m_config.contexts;
  auto before_context = false;
  for (auto i = static_cast<int>(contexts.size()) - 1; i >= 0; --i) {
    auto& context = contexts[i];
    const auto can_not_match = !context.system_filter_matched;
    if (context.fallthrough) {
      // remove fallthrough contexts which are not before a context
      if (can_not_match || !before_context)
        contexts.erase(std::next(contexts.begin(), i));
    }
    else {
      const auto has_no_effect = (
        context.inputs.empty() && 
        context.command_outputs.empty() &&
        (i == 0 || !context.begin_stage));

      if (can_not_match || has_no_effect) {
        // convert fallthrough context when removing the non-fallthrough context
        if (i > 0 && contexts[i - 1].fallthrough) {
          auto& before = contexts[i - 1];
          before.inputs = std::move(context.inputs);
          before.outputs = std::move(context.outputs);
          before.command_outputs = std::move(context.command_outputs);
          before.fallthrough = false;
        }
        contexts.erase(std::next(contexts.begin(), i));

        // no longer before a context when removing a non-fallthrough context
        before_context = false;
      }
      else {
        before_context = true;
      }
    }
  }
  // ensure there is always at least one context
  if (contexts.empty())
    contexts.emplace_back();
}

void ParseConfig::prepend_forward_modifier_mappings() {
  for (auto& context : m_config.contexts)
    if (context.begin_stage)
      for (auto it = m_forward_modifiers.rbegin(); 
           it != m_forward_modifiers.rend(); ++it) {
        const auto key = *it;
        context.inputs.insert(context.inputs.begin(), {
          KeySequence{ 
            KeyEvent(key, KeyState::Down),
            KeyEvent(key, KeyState::UpAsync)
          },
          static_cast<int>(context.outputs.size())
        });
        context.outputs.push_back({
          KeyEvent(key, KeyState::Down)
        });
      }
}

void ParseConfig::suppress_forwarded_modifiers_in_outputs() {
  if (m_forward_modifiers.empty())
    return;

  for (auto& context : m_config.contexts) {
    for (auto key : m_forward_modifiers) {
      const auto down_event = KeyEvent(key, KeyState::Down);
      const auto not_event = KeyEvent(key, KeyState::Not);
      const auto prepend_not_event = [&](KeySequence& output) {
        if (!contains(output, down_event) &&
            !contains(output, not_event))
          output.insert(output.begin(), not_event);
      };

      for (auto& input : context.inputs)
        if (contains(input.input, down_event)) {
          if (input.output_index >= 0) {
            prepend_not_event(context.outputs[input.output_index]);
          }
          else {
            for (auto& following_context : m_config.contexts) {
              for (auto& command : following_context.command_outputs)
                if (command.index == input.output_index)
                  prepend_not_event(command.output);
            }
          }
        }
    }
  }
}
