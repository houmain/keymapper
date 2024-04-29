
#include "Stage.h"
#include "common/parse_regex.h"
#include <cassert>
#include <algorithm>
#include <array>
#include <iterator>
#include <limits>

namespace {
  const auto exit_sequence = std::array{ Key::ShiftLeft, Key::Escape, Key::K };

  KeySequence::const_iterator find_key(const KeySequence& sequence, Key key) {
    return std::find_if(begin(sequence), end(sequence),
      [&](const auto& ev) { return ev.key == key; });
  }

  KeySequence::const_iterator rfind_key(const KeySequence& sequence, Key key) {
    auto it = std::find_if(rbegin(sequence), rend(sequence),
      [&](const auto& ev) { return ev.key == key; });
    if (it != rend(sequence))
      return std::next(it).base();
    return end(sequence);
  }

  template<typename It, typename T>
  bool contains(It begin, It end, const T& v) {
    return std::find(begin, end, v) != end;
  }

  bool is_non_optional(const KeyEvent& e) {
    return (e.state == KeyState::Up || e.state == KeyState::Down);
  }

  bool has_non_optional(const KeySequence& sequence) {
    return std::any_of(begin(sequence), end(sequence), is_non_optional);
  }

  bool has_unmatched_down(ConstKeySequenceRange sequence) {
    return std::any_of(begin(sequence), end(sequence),
      [](const KeyEvent& e) { return (e.state == KeyState::Down); });
  }

  // sort outputs by growing negative index (to allow binary search)
  std::vector<Stage::Context> sort_command_outputs(
      std::vector<Stage::Context> contexts) {
    for (auto& context : contexts)
      std::sort(begin(context.command_outputs), end(context.command_outputs),
        [](const Stage::CommandOutput& a, const Stage::CommandOutput& b) { 
          return a.index > b.index; 
        });
    return contexts;
  }

  bool has_mouse_mappings(const KeySequence& sequence) {
    return std::any_of(begin(sequence), end(sequence),
      [](const KeyEvent& event) { return is_mouse_button(event.key); });
  }

  bool has_mouse_mappings(const std::vector<Stage::Context>& contexts) {
    for (const auto& context : contexts) {
      if (has_mouse_mappings(context.modifier_filter))
        return true;
      for (const auto& input : context.inputs)
        if (has_mouse_mappings(input.input))
          return true;
    }
    return false;
  }

  bool has_device_filter(const std::vector<Stage::Context>& contexts) {
    for (const auto& context : contexts)
      if (!context.device_filter.empty())
        return true;
    return false;
  }

  const KeyEvent* find_last_down_event(ConstKeySequenceRange sequence) {
    auto last = std::add_pointer_t<const KeyEvent>{ };
    for (const auto& event : sequence)
      if (event.state == KeyState::Down ||
          event.state == KeyState::DownMatched)
        last = &event;
    return last;
  }

  const KeyEvent* find_last_non_optional(ConstKeySequenceRange sequence) {
    auto last = std::add_pointer_t<const KeyEvent>{ };
    for (const auto& event : sequence)
      if (is_non_optional(event))
        last = &event;
    return last;
  }

  KeyEvent get_trigger_event(const Trigger& trigger) {
    if (const auto* event = std::get_if<KeyEvent>(&trigger))
      return *event;

    if (const auto* key = std::get_if<Key>(&trigger))
      return KeyEvent{ *key, KeyState::Down };

    const auto& input = *std::get<const KeySequence*>(trigger);
    if (auto event = find_last_non_optional(input))
      return *event;

    return input.back();
  }

  Key get_trigger_key(const Trigger& trigger) {
    return get_trigger_event(trigger).key;
  }
} // namespace

Stage::Stage(std::vector<Context> contexts)
  : m_contexts(sort_command_outputs(std::move(contexts))),
    m_has_mouse_mappings(::has_mouse_mappings(m_contexts)),
    m_has_device_filter(::has_device_filter(m_contexts)){
}

bool Stage::is_clear() const {
  return m_output_down.empty() &&
         m_output_on_release.empty() &&
         m_sequence.empty() &&
         !m_sequence_might_match &&
         !m_current_timeout;
}

std::vector<Key> Stage::get_output_keys_down() const {
  auto keys = std::vector<Key>{ };
  for (const auto& output : m_output_down)
    if (is_keyboard_key(output.key) || is_mouse_button(output.key))
      keys.push_back(output.key);
  return keys;
}

void Stage::evaluate_device_filters(const std::vector<std::string>& device_names) {
  for (auto& context : m_contexts)
    if (!context.device_filter.empty()) {
      context.matching_device_bits = { };
      auto bit = decltype(context.matching_device_bits){ 1 };
      if (is_regex(context.device_filter)) {
        const auto regex = parse_regex(context.device_filter);
        for (const auto& device_name : device_names) {
          if (std::regex_search(device_name, regex) ^ context.invert_device_filter)
            context.matching_device_bits |= bit;
          bit <<= 1;
        }
      }
      else {
        for (const auto& device_name : device_names) {
          if ((device_name == context.device_filter) ^ context.invert_device_filter)
            context.matching_device_bits |= bit;
          bit <<= 1;
        }
      }
    }
}

bool Stage::device_matches_filter(const Context& context, int device_index) const {
  return (device_index == any_device_index ||
         ((context.matching_device_bits >> device_index) & 1));
}

KeySequence Stage::set_active_client_contexts(const std::vector<int> &indices) {
  // order of active contexts is relevant
  assert(std::is_sorted(begin(indices), end(indices)));
  for ([[maybe_unused]] auto i : indices)
    assert(i >= 0 && i < static_cast<int>(m_contexts.size()));

  m_active_client_contexts = indices;
  update_active_contexts();

  // cancel output on release when the focus changed
  cancel_inactive_output_on_release();

  // updating contexts can toggle ContextActive keys
  return std::move(m_output_buffer);
}

bool Stage::match_context_modifier_filter(const KeySequence& modifiers) {
  for (const auto& modifier : modifiers) {
    const auto pressed = (find_key(m_sequence, modifier.key) != m_sequence.end());
    const auto should_be_pressed = (modifier.state != KeyState::Not);
    if (pressed != should_be_pressed)
      return false;
  }
  return true;
}

void Stage::update_active_contexts() {
  std::swap(m_prev_active_contexts, m_active_contexts);

  // evaluate modifier and device filter of contexts which were set active by client
  m_active_contexts.clear();
  for (auto index : m_active_client_contexts) {
    const auto& context = m_contexts[index];
    if ((match_context_modifier_filter(context.modifier_filter) ^ context.invert_modifier_filter) &&
        (context.device_filter.empty() || context.matching_device_bits)) {
      index = fallthrough_context(index);
      if (m_active_contexts.empty() || m_active_contexts.back() != index)
        m_active_contexts.push_back(index);
    }
  }

  // compare current and previous active contexts indices
  // first toggle deactivated contexts' keys then activated
  for (auto toggle_activated : { false, true }) {
    auto prev_it = m_prev_active_contexts.begin();
    auto curr_it = m_active_contexts.begin();
    const auto prev_end = m_prev_active_contexts.end();
    const auto curr_end = m_active_contexts.end();
    while (prev_it != prev_end || curr_it != curr_end) {
      const auto max = std::numeric_limits<int>::max();
      const auto p = (prev_it != prev_end ? *prev_it : max);
      const auto c = (curr_it != curr_end ? *curr_it : max);
      if (p < c) {
        // context #p deactivated
        if (!toggle_activated)
          on_context_active_event({ Key::ContextActive, KeyState::Up }, p);
        ++prev_it;
      }
      else if (c < p) {
        // context #c activated
        if (toggle_activated)
          on_context_active_event({ Key::ContextActive, KeyState::Down }, c);
        ++curr_it;
      }
      else {
        ++prev_it;
        ++curr_it;
      }
    }
  }
}

void Stage::on_context_active_event(const KeyEvent& event, int context_index) {
  const auto& context = m_contexts[context_index];
  const auto& inputs = context.inputs;
  const auto it = std::find_if(inputs.begin(), inputs.end(),
    [&](const Input& input) { 
      return (input.input.front().key == Key::ContextActive); 
    });
  if (it != inputs.end()) {
    if (event.state == KeyState::Down) {
      if (auto output = find_output(context, it->output_index))
        apply_output(*output, event, context_index);
    }
    else {
      continue_output_on_release(event, context_index);
      release_triggered(event.key, context_index);
    }
  }
}

void Stage::cancel_inactive_output_on_release() {
  // only cancel output which was triggered in now inactive context
  m_output_on_release.erase(
    std::remove_if(begin(m_output_on_release), end(m_output_on_release),
      [&](const OutputOnRelease& output) {
        return !is_context_active(output.context_index);
      }),
    end(m_output_on_release));
}

int Stage::fallthrough_context(int context_index) const {
  while (m_contexts[context_index].fallthrough)
    ++context_index;
  return context_index;
}

bool Stage::is_context_active(int context_index) const {
  for (auto active_context_index : m_active_contexts)
    if (active_context_index == context_index ||
        fallthrough_context(active_context_index) == context_index)
      return true;
  return false;
}

void Stage::advance_exit_sequence(const KeyEvent& event) {
  if (!is_keyboard_key(event.key))
    return;

  if (event.state == KeyState::Down) {
    const auto p = m_exit_sequence_position;
    // ignore key repeat
    if (p > 0 && event.key == exit_sequence[p - 1])
      return;
    if (p < exit_sequence.size() && event.key == exit_sequence[p]) {
      ++m_exit_sequence_position;
      return;
    }
  }
  m_exit_sequence_position = 0;
}

bool Stage::should_exit() const {
  return (m_exit_sequence_position == exit_sequence.size());
}

KeySequence Stage::update(const KeyEvent event, int device_index) {
  advance_exit_sequence(event);
  apply_input(event, device_index);
  return std::move(m_output_buffer);
}

void Stage::reuse_buffer(KeySequence&& buffer) {
  m_output_buffer = std::move(buffer);
  m_output_buffer.clear();
}

void Stage::validate_state(const std::function<bool(Key)>& is_down) {
  m_sequence_might_match = false;

  m_sequence.erase(
    std::remove_if(begin(m_sequence), end(m_sequence),
      [&](const KeyEvent& event) { return !is_down(event.key); }),
    end(m_sequence));

  m_output_down.erase(
    std::remove_if(begin(m_output_down), end(m_output_down),
      [&](const OutputDown& output) {
        return !is_down(get_trigger_key(output.trigger));
      }),
    end(m_output_down));
}

const KeySequence* Stage::find_output(const Context& context, int output_index) const {
  if (output_index >= 0) {
    assert(output_index < static_cast<int>(context.outputs.size()));
    return &context.outputs[output_index];
  }

  // search for last override of command output
  for (auto i = static_cast<int>(m_active_contexts.size()) - 1; i >= 0; --i) {
    // binary search for command outputs of context
    const auto context_index = fallthrough_context(m_active_contexts[i]);
    const auto& command_outputs = m_contexts[context_index].command_outputs;
    const auto it = std::lower_bound(
      command_outputs.rbegin(), command_outputs.rend(), output_index,
      [](const CommandOutput& a, int index) { return a.index < index; });
    if (it != command_outputs.rend() && it->index == output_index)
      return &it->output;
  }
  return nullptr;
}

auto Stage::match_input(ConstKeySequenceRange sequence,
    int device_index, bool accept_might_match, bool is_key_up_event) -> MatchInputResult {
  for (auto context_index : m_active_contexts) {
    const auto& context = m_contexts[context_index];
    if (!device_matches_filter(context, device_index))
      continue;

    for (const auto& input : context.inputs) {
      auto input_timeout_event = KeyEvent{ };
      const auto result = m_match(input.input, sequence,
        &m_any_key_matches, &input_timeout_event);

      if (accept_might_match && result == MatchResult::might_match) {
        
        if (input_timeout_event.key == Key::timeout) {
          // request client to inject timeout event
          m_output_buffer.push_back(input_timeout_event);

          // track timeout - use last key Down as trigger
          if (auto trigger = find_last_down_event(sequence)) {
            if (!m_current_timeout || 
                *m_current_timeout != input_timeout_event ||
                m_current_timeout->trigger != trigger->key) {
              m_current_timeout = { input_timeout_event, trigger->key };
            }
            else if (is_key_up_event) {
              // timeout did not change, undo adding to output buffer
              m_output_buffer.pop_back();
            }
          }
        }
        return { MatchResult::might_match, nullptr, &input.input, context_index };
      }

      if (result == MatchResult::match)
        if (auto output = find_output(context, input.output_index))
          return { MatchResult::match, output, &input.input, context_index };
    }
  }
  return { MatchResult::no_match, nullptr, nullptr, 0 };
}

void Stage::apply_input(const KeyEvent event, int device_index) {
  assert(event.state == KeyState::Down ||
         event.state == KeyState::Up);

  // check if key triggers an output on release
  if (!continue_output_on_release(event))
    return;

  // suppress short timeout after not-timeout was exceeded
  if (m_current_timeout && is_not_timeout(m_current_timeout->state)) {
    if (event.key == Key::timeout) {
      if (m_current_timeout->timeout == event.timeout) {
        m_current_timeout->not_exceeded = true;
      }
      else if (m_current_timeout->not_exceeded) {
        return;
      }
    }
    else if (event.state == KeyState::Up &&
             m_current_timeout->trigger == event.key) {
      m_current_timeout->not_exceeded = false;
    }
  }

  if (event.state == KeyState::Down) {
    // merge key repeats
    const auto it = rfind_key(m_sequence, event.key);
    if (it != end(m_sequence) && it->state != KeyState::Up) {
      // ignore key repeat while sequence might match
      if (m_sequence_might_match)
        return;

      m_sequence.erase(it);
    }
  }

  m_sequence.push_back(event);

  // update contexts with modifier filter
  update_active_contexts();

  if (event.state == KeyState::Up) {
    // release output when triggering input was released
    release_triggered(event.key);

    // remove from sequence
    // except when it was already used for a might match
    if (!m_sequence_might_match) {
      const auto it = find_key(m_sequence, event.key);
      assert(it != end(m_sequence));
      if (it->state == KeyState::DownMatched)
        m_sequence.erase(it);
    }
  }

  for (auto& output : m_output_down)
    output.suppressed = false;

  const auto is_key_up_event = (event.state == KeyState::Up && event.key != Key::timeout);
  while (has_non_optional(m_sequence)) {
    // find first mapping which matches or might match sequence
    auto sequence = ConstKeySequenceRange(m_sequence);
    auto [result, output, trigger, context_index] = match_input(
      sequence, device_index, true, is_key_up_event);

    // virtual key events need to match directly or never
    if (is_virtual_key(event.key) &&
        result != MatchResult::match) {
      finish_sequence(sequence);
      break;
    }

    // hold back sequence when something might match
    if (result == MatchResult::might_match) {
      m_sequence_might_match = true;
      break;
    }

    // when might match failed, look for exact match in sequence start
    auto matched_start_only = false;
    if (result == MatchResult::no_match &&
        m_sequence_might_match) {

      while (sequence.size() > 1) {
        sequence.pop_back();
        if (!has_unmatched_down(sequence))
          break;

        std::tie(result, output, trigger, context_index) = 
          match_input(sequence, device_index, false, is_key_up_event);
        if (result == MatchResult::match) {
          matched_start_only = true;
          break;
        }
      }
    }

    // when a timeout matched once, prevent following timeout
    // cancellation from matching another input
    if (m_current_timeout && !is_not_timeout(m_current_timeout->state)) {
      if (event.key == Key::timeout) {
        if (result == MatchResult::match) {
          if (!m_current_timeout->matched_output) {
            if (!matched_start_only)
              m_current_timeout->matched_output = output;
          }
          else if (m_current_timeout->matched_output != output) {
            result = MatchResult::no_match;
          }
        }
      }
      else if (result == MatchResult::no_match) {
        m_current_timeout->matched_output = nullptr;
      }
    }

    // prevent match after not-timeout did not match once
    if (m_current_timeout && is_not_timeout(m_current_timeout->state)) {
      if (result == MatchResult::no_match)
        m_current_timeout->not_exceeded = true;
    }

    if (result == MatchResult::match) {
      // optimize trigger
      if (get_trigger_key(trigger) == Key::any ||
          event.key == Key::timeout)
        trigger = event;

      // for timeouts use last key press as trigger, if it is still down
      if (get_trigger_key(trigger) == Key::timeout && m_current_timeout)
        if (auto it = rfind_key(m_sequence, m_current_timeout->trigger);
            it != cend(m_sequence) && it->state != KeyState::Up)
          trigger = m_current_timeout->trigger;

      apply_output(*output, trigger, context_index);

      // release new output when triggering input was released
      if (event.state == KeyState::Up) {
        continue_output_on_release(event, context_index);
        release_triggered(event.key);
      }

      finish_sequence(sequence);

      // continue when only the start of the sequence matched
      m_sequence_might_match = false;
    }
    else {
      // when still no match was found, forward beginning of sequence
      forward_from_sequence();

      if (m_sequence_might_match && !has_non_optional(m_sequence))
        m_sequence_might_match = false;
    }
  }

  // update contexts with modifier filter
  update_active_contexts();

  if (m_sequence.empty())
    m_current_timeout.reset();
}

bool Stage::continue_output_on_release(const KeyEvent& event, int context_index) {
  const auto it = std::find_if(begin(m_output_on_release), end(m_output_on_release),
    [&](const OutputOnRelease& o) {
      return (o.trigger == event.key &&
              (o.trigger != Key::ContextActive || o.context_index == context_index));
    });
  if (it != m_output_on_release.end()) {
    // ignore key repeat
    if (event.state == KeyState::Down)
      return false;

    // trigger released - output rest of sequence
    apply_output(it->sequence, event, it->context_index);
    m_output_on_release.erase(it);
  }
  return true;
}

void Stage::release_triggered(Key key, int context_index) {
  // sort output to release to the right
  const auto it = std::stable_partition(begin(m_output_down), end(m_output_down),
    [&](const OutputDown& k) { 
      if (get_trigger_key(k.trigger) == key) {
        if (key == Key::ContextActive)
          return (k.context_index != context_index);
        return false;
      }
      return true;
    });
  std::for_each(
    std::make_reverse_iterator(end(m_output_down)),
    std::make_reverse_iterator(it),
    [&](const OutputDown& k) {
      if (!k.temporarily_released)
        m_output_buffer.push_back({ k.key, KeyState::Up });
    });
  m_output_down.erase(it, end(m_output_down));
}

void Stage::apply_output(ConstKeySequenceRange sequence,
    const Trigger& trigger, int context_index) {
  for (const auto& event : sequence)
    if (event.key == Key::any) {
      for (auto key : m_any_key_matches)
        update_output({ key, event.state }, trigger, context_index);
    }
    else if (event.state == KeyState::OutputOnRelease) {
      // do not split output when input matched when trigger was released
      const auto trigger_event = get_trigger_event(trigger);
      if (trigger_event.state == KeyState::Down) {
        // send rest of sequence when trigger is released
        const auto it = sequence.begin() + std::distance(&sequence[0], &event);
        const auto rest = ConstKeySequenceRange(std::next(it), sequence.end());
        m_output_on_release.push_back({ trigger_event.key, rest, context_index });
        break;
      }
    }
    else {
      update_output(event, trigger, context_index);
    }
}

void Stage::forward_from_sequence() {
  for (auto it = begin(m_sequence); it != end(m_sequence); ++it) {
    auto& event = *it;
    if (event.state == KeyState::Down || event.state == KeyState::DownMatched) {
      const auto up = std::find(it, end(m_sequence),
        KeyEvent{ event.key, KeyState::Up });
      if (up != end(m_sequence)) {
        // erase Down and Up
        update_output(event, event.key);
        release_triggered(event.key);
        m_sequence.erase(up);
        m_sequence.erase(it);
        return;
      }
      else if (event.state == KeyState::Down) {
        // no Up yet, convert to DownMatched
        // suppress forwarding when a timeout already matched
        if (!m_current_timeout || !m_current_timeout->matched_output)
          update_output(event, event.key);
        event.state = KeyState::DownMatched;
        return;
      }
    }
    else if (event.state == KeyState::Up) {
      // remove remaining Up
      release_triggered(event.key);
      m_sequence.erase(it);
      return;
    }
  }
}

void Stage::update_output(const KeyEvent& event, const Trigger& trigger, int context_index) {
  const auto it = std::find_if(begin(m_output_down), end(m_output_down),
    [&](const OutputDown& down_key) { return down_key.key == event.key; });

  switch (event.state) {
    case KeyState::Up: {
      if (it != end(m_output_down)) {
        if (it->pressed_twice) {
          // try to remove current down
          auto it2 = rfind_key(m_output_buffer, event.key);
          if (it2 != m_output_buffer.end())
            m_output_buffer.erase(it2);

          it->pressed_twice = false;
        }
        else {
          // only releasing trigger can permanently release
          if (get_trigger_key(it->trigger) == get_trigger_key(trigger))
            m_output_down.erase(it);
          else
            it->temporarily_released = true;

          m_output_buffer.push_back(event);
        }
      }
      break;
    }

    case KeyState::Not: {
      // make sure it is released in output
      if (it != end(m_output_down)) {
        if (!it->temporarily_released) {
          m_output_buffer.emplace_back(event.key, KeyState::Up);
          it->temporarily_released = true;
        }
        it->suppressed = true;
      }
      break;
    }

    case KeyState::Down: {
      // reapply temporarily released
      for (auto& output : m_output_down)
        if (output.temporarily_released && 
            (!output.suppressed || output.key == event.key)) {
          output.temporarily_released = false;
          m_output_buffer.emplace_back(output.key, KeyState::Down);
          m_temporary_reapplied = true;

          if (output.key == event.key)
            return;
        }

      if (it == end(m_output_down)) {
        if (event.key != Key::timeout)
          m_output_down.push_back({ event.key, trigger, 
            false, false, false, context_index });
      }
      else {
        // already pressed before
        it->temporarily_released = false;
        it->pressed_twice = true;

        // up/down when something was reapplied in the meantime
        if (m_temporary_reapplied) {
          m_output_buffer.emplace_back(event.key, KeyState::Up);
          it->pressed_twice = false;
        }
      }
      m_output_buffer.push_back(event);
      break;
    }

    case KeyState::DownMatched:
      // ignored
      break;

    case KeyState::UpAsync:
    case KeyState::DownAsync:
    case KeyState::OutputOnRelease:
    case KeyState::NotTimeout_cancel_on_up_down:
      assert(!"unreachable");
      break;
  }
}

void Stage::finish_sequence(ConstKeySequenceRange sequence) {
  // erase Down and DownMatchen when an Up follows, convert to DownMatched otherwise
  assert(sequence.begin() == m_sequence.begin());
  assert(sequence.size() <= m_sequence.size());
  auto length = sequence.size();
  for (auto i = size_t{ }; i < length; ) {
    const auto it = begin(m_sequence) + i;
    if (it->state == KeyState::Down || it->state == KeyState::DownMatched) {
      if (!contains(it, end(m_sequence), KeyEvent{ it->key, KeyState::Up })) {
        it->state = KeyState::DownMatched;
        ++i;
        continue;
      }
    }
    m_sequence.erase(it);
    --length;
  }
  m_temporary_reapplied = false;
}
