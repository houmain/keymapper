#pragma once

#include "MatchKeySequence.h"
#include <functional>

class Stage {
public:
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
    std::vector<int> device_indces;
  };

  explicit Stage(std::vector<Context> contexts);

  const std::vector<Context>& contexts() const { return m_contexts; }
  bool has_mouse_mappings() const { return m_has_mouse_mappings; }

  const KeySequence& sequence() const { return m_sequence; }
  bool is_output_down() const { return !m_output_down.empty(); }
  void set_device_indices(const std::vector<std::string>& device_names);
  void set_active_contexts(const std::vector<int>& indices);
  KeySequence update(KeyEvent event, int device_index);
  void reuse_buffer(KeySequence&& buffer);
  void validate_state(const std::function<bool(Key)>& is_down);
  bool should_exit() const;

private:
  void advance_exit_sequence(const KeyEvent& event);
  const KeySequence* find_output(const Context& context, int output_index) const;
  std::pair<MatchResult, const KeySequence*> match_input(
    ConstKeySequenceRange sequence, int device_index, 
    bool accept_might_match);
  void apply_input(KeyEvent event, int device_index);
  void release_triggered(Key key);
  void forward_from_sequence();
  void apply_output(const KeySequence& expression, Key trigger);
  void update_output(const KeyEvent& event, Key trigger);
  void finish_sequence(ConstKeySequenceRange sequence);

  std::vector<Context> m_contexts;
  bool m_has_mouse_mappings{ };
  std::vector<int> m_active_contexts;
  MatchKeySequence m_match;
  size_t m_exit_sequence_position{ };

  // the input since the last match (or already matched but still hold)
  KeySequence m_sequence;
  bool m_sequence_might_match{ };

  // the keys which were output and are still down
  struct OutputDown {
    Key key;
    Key trigger;
    bool suppressed;           // by KeyState::Not event
    bool temporarily_released; // by KeyState::Not event
    bool pressed_twice;
  };
  std::vector<OutputDown> m_output_down;

  // temporary buffer
  KeySequence m_output_buffer;
  std::vector<Key> m_toggle_virtual_keys;
  bool m_temporary_reapplied{ };
  std::vector<Key> m_any_key_matches;
};
