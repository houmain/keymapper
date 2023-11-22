#pragma once

#include "MatchKeySequence.h"
#include <functional>
#include <optional>
#include <variant>

using Trigger = std::variant<const KeySequence*, KeyEvent, Key>;

class Stage {
public:
  static const int no_device_index = -1;
  static const int any_device_index = -2;

  struct Input {
    KeySequence input;
    // positive for direct-, negative for command output
    int output_index;
  };

  struct CommandOutput {
    KeySequence output;
    int index;
  };

  struct Context {
    std::vector<Input> inputs;
    std::vector<KeySequence> outputs;
    std::vector<CommandOutput> command_outputs;
    std::string device_filter;
    KeySequence modifier_filter;
    uint64_t matching_device_bits = ~uint64_t{ };
    bool invert_device_filter{ };
    bool invert_modifier_filter{ };
    bool fallthrough{ };
  };

  explicit Stage(std::vector<Context> contexts = { });

  const std::vector<Context>& contexts() const { return m_contexts; }
  const std::vector<int>& active_client_contexts() const { return m_active_client_contexts; }
  bool has_mouse_mappings() const { return m_has_mouse_mappings; }
  bool has_device_filters() const { return m_has_device_filter; }

  bool is_clear() const;
  const KeySequence& sequence() const { return m_sequence; }
  std::vector<Key> get_output_keys_down() const;
  void evaluate_device_filters(const std::vector<std::string>& device_names);
  KeySequence set_active_client_contexts(const std::vector<int>& indices);
  KeySequence update(KeyEvent event, int device_index);
  void reuse_buffer(KeySequence&& buffer);
  void validate_state(const std::function<bool(Key)>& is_down);
  bool should_exit() const;

private:
  using MatchInputResult = std::tuple<MatchResult, const KeySequence*, Trigger, int>;

  void advance_exit_sequence(const KeyEvent& event);
  const KeySequence* find_output(const Context& context, int output_index) const;
  bool device_matches_filter(const Context& context, int device_index) const;
  MatchInputResult match_input(ConstKeySequenceRange sequence, int device_index,
    bool accept_might_match, bool is_key_up_event);
  void apply_input(KeyEvent event, int device_index);
  void release_triggered(Key key, int context_index = -1);
  void forward_from_sequence();
  void apply_output(ConstKeySequenceRange sequence,
    const Trigger& trigger, int context_index);
  void update_output(const KeyEvent& event, const Trigger& trigger, int context_index = -1);
  void finish_sequence(ConstKeySequenceRange sequence);
  bool match_context_modifier_filter(const KeySequence& modifiers);
  void update_active_contexts();
  bool continue_output_on_release(const KeyEvent& event, int context_index = -1);
  void cancel_inactive_output_on_release();
  int fallthrough_context(int context_index) const;
  bool is_context_active(int context_index) const;
  void on_context_active_event(const KeyEvent& event, int context_index);

  std::vector<Context> m_contexts;
  bool m_has_mouse_mappings{ };
  bool m_has_device_filter{ };
  std::vector<int> m_active_client_contexts;
  std::vector<int> m_active_contexts;
  std::vector<int> m_prev_active_contexts;
  MatchKeySequence m_match;
  size_t m_exit_sequence_position{ };

  // the input since the last match (or already matched but still hold)
  KeySequence m_sequence;
  bool m_sequence_might_match{ };

  struct OutputOnRelease {
    Key trigger;
    ConstKeySequenceRange sequence;
    int context_index;
  };
  std::vector<OutputOnRelease> m_output_on_release;

  // the keys which were output and are still down
  struct OutputDown {
    Key key;
    Trigger trigger;
    bool suppressed;           // by KeyState::Not event
    bool temporarily_released; // by KeyState::Not event
    bool pressed_twice;
    int context_index;
  };
  std::vector<OutputDown> m_output_down;

  struct CurrentTimeout : KeyEvent {
    Key trigger;
    const KeySequence* matched_output;
    bool not_exceeded;
  };
  std::optional<CurrentTimeout> m_current_timeout;

  // temporary buffer
  KeySequence m_output_buffer;
  bool m_temporary_reapplied{ };
  std::vector<Key> m_any_key_matches;
};
