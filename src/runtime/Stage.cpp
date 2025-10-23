
#include "Stage.h"
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

  size_t count_key_downs(const KeySequence& sequence, Key key) {
    return std::count_if(begin(sequence), end(sequence),
      [&](const KeyEvent& e) { return (e.key == key &&
        (e.state == KeyState::Down || e.state == KeyState::DownMatched));
      });
  }

  template<typename It, typename T>
  bool contains(It begin, It end, const T& v) {
    return std::find(begin, end, v) != end;
  }

  template<typename R, typename T>
  bool contains(const R& range, const T& v) {
    return contains(begin(range), end(range), v);
  }

  bool is_non_optional(const KeyEvent& e) {
    return (e.state == KeyState::Up || e.state == KeyState::Down);
  }

  bool has_non_optional(const KeySequence& sequence) {
    return std::any_of(begin(sequence), end(sequence), is_non_optional);
  }

  bool has_output_on_release(const KeySequence& sequence) {
    return std::any_of(begin(sequence), end(sequence), [](const KeyEvent& e) {
      return (e.state == KeyState::OutputOnRelease);
    });
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
      [](const KeyEvent& event) {
        return is_mouse_button(event.key) || is_mouse_wheel(event.key);
      });
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

  bool has_device_filter(const Stage::Context& context) {
    return (context.device_filter ||
            context.device_id_filter);
  }

  bool has_device_filter(const std::vector<Stage::Context>& contexts) {
    for (const auto& context : contexts)
      if (has_device_filter(context))
        return true;
    return false;
  }

  bool is_no_might_match_mapping(const KeySequence& sequence) {
    return (!sequence.empty() && 
      sequence.front().state == KeyState::NoMightMatch);
  }

  bool has_no_might_match_mapping(const std::vector<Stage::Context>& contexts) {
    for (const auto& context : contexts)
      for (const auto& input : context.inputs)
        if (is_no_might_match_mapping(input.input))
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

  ConstKeySequenceRange without_first(ConstKeySequenceRange sequence) {
    return { std::next(sequence.begin()), sequence.end() };
  }
} // namespace

Stage::Stage(std::vector<Context> contexts)
  : m_contexts(sort_command_outputs(std::move(contexts))),
    m_has_mouse_mappings(::has_mouse_mappings(m_contexts)),
    m_has_device_filter(::has_device_filter(m_contexts)),
    m_has_no_might_match_mapping(::has_no_might_match_mapping(m_contexts)) {
}

bool Stage::is_clear() const {
  return m_output_down.empty() &&
         m_output_on_release.empty() &&
         m_sequence.empty() &&
         m_last_repeat_device_index == no_device_index &&
         m_history.empty() &&
         !m_sequence_might_match &&
         !m_current_timeout;
}

std::vector<Key> Stage::get_output_keys_down() const {
  auto keys = std::vector<Key>{ };
  for (const auto& output : m_output_down)
    if (is_device_key(output.key))
      keys.push_back(output.key);
  return keys;
}

void Stage::evaluate_device_filters(const std::vector<DeviceDesc>& device_descs) {
  for (auto& context : m_contexts)
    if (has_device_filter(context)) {
      context.matching_device_bits = { };
      auto bit = decltype(context.matching_device_bits){ 1 };
      for (const auto& device_desc : device_descs) {
        if (context.device_filter.matches(device_desc.name, false) &&
            context.device_id_filter.matches(device_desc.id, false))
          context.matching_device_bits |= bit;
        bit <<= 1;
      }
    }
    else {
      context.matching_device_bits = all_device_bits;
    }
}

bool Stage::device_matches_filter(const Context& context, int device_index) const {
  if (device_index == any_device_index)
    return true;

  // no-device only matches contexts with default device
  if (device_index == no_device_index)
    return (context.matching_device_bits == all_device_bits);

  return ((context.matching_device_bits >> device_index) & 1);
}

KeySequence Stage::set_active_client_contexts(const std::vector<int> &indices) {
  // order of active contexts is relevant
  assert(std::is_sorted(begin(indices), end(indices)));
  for ([[maybe_unused]] auto i : indices)
    assert(i >= 0 && i < static_cast<int>(m_contexts.size()));

  m_active_client_contexts = indices;
  update_active_contexts(any_device_index);

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

void Stage::update_active_contexts(int device_index) {
  std::swap(m_prev_active_contexts, m_active_contexts);

  // evaluate modifier and device filter of contexts which were set active by client
  m_active_contexts.clear();
  for (auto index : m_active_client_contexts) {
    const auto& context = m_contexts[index];
    if ((match_context_modifier_filter(context.modifier_filter) ^ context.invert_modifier_filter) &&
         device_matches_filter(context, device_index)) {
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
      [&](const KeyEvent& event) { 
        return is_device_key(event.key) && 
          !is_down(event.key); 
      }),
    end(m_sequence));

  auto release_events = KeySequence();
  for (const auto& output : m_output_down)
    if (is_device_key(output.key) && !is_down(get_trigger_key(output.trigger)))
      release_events.emplace_back(output.key, KeyState::Up);
  for (const auto& event : release_events)
    apply_input(event, any_device_index);
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

auto Stage::match_input(bool first_iteration, bool matched_are_optional,
    ConstKeySequenceRange sequence, int device_index, 
    bool is_key_up_event) -> MatchInputResult {

  for (auto context_index : m_active_contexts) {
    const auto& context = m_contexts[context_index];
    for (const auto& context_input : context.inputs) {
      const auto& input = context_input.input;
      const auto no_might_match_mapping = 
        is_no_might_match_mapping(input);

      // no-might-match mappings are matched with
      // history and only in first iteration
      if (no_might_match_mapping && (!first_iteration || m_history.empty()))
        continue;

      // no-might-match mappings are matched only
      // when current event comes from a device
      if (no_might_match_mapping && (device_index == any_device_index))
        continue;

      // might match is only accepted in first iteration (whole sequence)
      const auto accept_might_match = 
        (first_iteration && !no_might_match_mapping);

      auto input_timeout_event = KeyEvent{ };
      const auto result = m_match(input,
        (no_might_match_mapping ? m_history : sequence),
        matched_are_optional, &m_any_key_matches, &input_timeout_event);

      if (accept_might_match && result == MatchResult::might_match)
        return { MatchResult::might_match, nullptr, &input, context_index, input_timeout_event };

      if (result == MatchResult::match)
        if (auto output = find_output(context, context_input.output_index))
          return { MatchResult::match, output, &input, context_index, {} };
    }
  }
  return { MatchResult::no_match, nullptr, nullptr, 0, {} };
}

bool Stage::is_physically_pressed(Key key) const {
  const auto it = rfind_key(m_sequence, key);
  return (it != cend(m_sequence) && it->state != KeyState::Up);
}

void Stage::apply_input(const KeyEvent event, int device_index) {
  assert(event.state == KeyState::Down ||
         event.state == KeyState::Up);
  assert(is_device_key(event.key) ||
         is_virtual_key(event.key) ||
         event.key == Key::timeout ||
         event.key == Key::unicode_output);

  // check if key triggers an output on release
  if (!continue_output_on_release(event))
    return;

  // suppress short timeout after not-timeout was exceeded
  if (m_current_timeout && is_not_timeout(m_current_timeout->state)) {
    if (event.key == Key::timeout) {
      if (m_current_timeout->value == event.value) {
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

      // suppress key repeat of all but last repeating device
      if (device_index != no_device_index) {
        if (device_index == m_last_pressed_device_index)
          m_last_repeat_device_index = device_index;

        if (device_index != m_last_repeat_device_index)
          return;
      }
      m_sequence.erase(it);
    }
    else {
      // not a repeat, store pressed device index
      if (device_index >= 0) {
        if (device_index == m_last_repeat_device_index)
          m_last_repeat_device_index = no_device_index;
        m_last_pressed_device_index = device_index;
      }
    }
  }
  else {
    // reset repeat device index on Up
    if (device_index == m_last_repeat_device_index)
      m_last_repeat_device_index = no_device_index;
  }

  // add to sequence
  m_sequence.push_back(event);

  // add to history
  if (m_has_no_might_match_mapping && 
      !is_virtual_key(event.key) &&
      event.key != Key::timeout) {
    const auto it = rfind_key(m_history, event.key);
    if (event.state == KeyState::Down) {
      if (it == end(m_history) || it->state != KeyState::Down)
        m_history.push_back(event);
    }
    else {
      if (it != end(m_history) && it->state == KeyState::Down)
        m_history.push_back(event);
    }
  }

  // update contexts with modifier filter
  update_active_contexts(device_index);

  if (event.state == KeyState::Up) {

    // suppress forwarding when a timeout already matched
    if (m_current_timeout && m_current_timeout->matched_output)
      for (auto& ev : m_sequence)
        if (ev.state == KeyState::Down)
          ev.state = KeyState::DownMatched;

    // remove from sequence
    // except when it was already used for a might match
    if (!m_sequence_might_match) {
      const auto it = find_key(m_sequence, event.key);
      assert(it != end(m_sequence));
      if (it->state == KeyState::DownMatched)
        m_sequence.erase(it);
    }
  }

  const auto is_unsuppressed_common_modifier_down =
    std::any_of(m_output_down.begin(), m_output_down.end(),
      [](const OutputDown& output) {
        return (is_common_modifier(output.key) && !output.suppressed);
      });

  for (auto& output : m_output_down) {
    // keep suppressing common modifiers as long as a common modifier is still output
    if (is_common_modifier(output.key) && is_unsuppressed_common_modifier_down)
      continue;

    output.suppressed = false;
  }

  auto matched_are_optional = false;

  const auto is_key_up_event = (event.state == KeyState::Up && event.key != Key::timeout);
  while (has_non_optional(m_sequence)) {
    // find first mapping which matches or might match sequence
    auto sequence = ConstKeySequenceRange(m_sequence);

    auto [result, output, trigger, context_index, input_timeout_event] = match_input(
      true, matched_are_optional, sequence, device_index, is_key_up_event);

    // initially try to find a match also including all already matched events
    // if none is found then retry with making them optional
    if (result != MatchResult::match && !matched_are_optional) {
      matched_are_optional = true;
      continue;
    }

    // virtual key events need to match directly or never
    if (is_virtual_key(event.key) &&
        result != MatchResult::match) {
      finish_sequence(sequence);
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

        std::tie(result, output, trigger, context_index, input_timeout_event) = 
          match_input(false, true, sequence, device_index, is_key_up_event);
        if (result == MatchResult::match) {
          matched_start_only = true;
          break;
        }
      }
    }

    if (input_timeout_event.key == Key::timeout) {
      // request client to inject timeout event
      m_output_buffer.push_back(input_timeout_event);

      // track timeout - use last key Down as trigger
      if (auto down = find_last_down_event(sequence)) {
        if (!m_current_timeout ||
            *m_current_timeout != input_timeout_event ||
            m_current_timeout->trigger != down->key) {
          m_current_timeout = { input_timeout_event, down->key };
        }
        else if (is_key_up_event) {
          // timeout did not change, undo adding to output buffer
          m_output_buffer.pop_back();
        }
      }
    }

    // hold back sequence when something might match
    if (result == MatchResult::might_match) {
      m_sequence_might_match = true;
      break;
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

      // use last event as trigger when output has on release part
      if (has_output_on_release(*output))
        trigger = event;

      // for timeouts use last key press as trigger
      if (get_trigger_key(trigger) == Key::timeout && m_current_timeout)
        trigger = m_current_timeout->trigger;

      // ensure that trigger is still down
      if (!is_physically_pressed(get_trigger_key(trigger))) {

        // do not change trigger of hold back output
        // when trigger is also released (might need some more work)
        const auto keep_trivial_trigger = (output->size() == 1 && 
            output->front() == get_trigger_event(trigger) &&
            contains(m_sequence, KeyEvent(get_trigger_key(trigger), KeyState::Up)));

        if (!keep_trivial_trigger)
          trigger = event;
      }

      apply_output(*output, trigger, context_index);

      finish_sequence(sequence);

      // continue when only the start of the sequence matched
      if (!matched_start_only)
        m_sequence_might_match = false;
    }
    else {
      // when still no match was found, forward beginning of sequence
      forward_from_sequence();

      if (m_sequence_might_match && !has_non_optional(m_sequence))
        m_sequence_might_match = false;
    }
  }

  // release output when triggering input was released
  if (event.state == KeyState::Up)
    release_triggered(event.key);

  // update contexts with modifier filter
  update_active_contexts(device_index);

  // remove matched timeout events
  while (!m_sequence.empty() && 
         m_sequence.front().state == KeyState::UpMatched)
    m_sequence.erase(m_sequence.begin());

  if (m_sequence.empty())
    m_current_timeout.reset();

  m_temporary_reapplied = false;

  clean_up_history();
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

  // also reset current timeout
  if (m_current_timeout && m_current_timeout->trigger == key)
    m_current_timeout.reset();
}

void Stage::apply_output(ConstKeySequenceRange sequence,
    const Trigger& trigger, int context_index) {
  for (auto it = begin(sequence); it != end(sequence); ++it) {
    const auto& event = *it;
    if (event.key == Key::any) {
      if (event.state == KeyState::Not) {
        // release all keys
        for (const auto& output : m_output_down)
          update_output({ output.key, KeyState::Not }, trigger, context_index);
      }
      else {
        // output keys matched by Any in input
        for (auto key : m_any_key_matches)
          update_output({ key, event.state }, trigger, context_index);
      }
    }
    else if (event.state == KeyState::OutputOnRelease) {
      // do not split output when input matched when trigger was released
      const auto trigger_event = get_trigger_event(trigger);
      if (trigger_event.state == KeyState::Down) {
        // send rest of sequence when trigger is released
        const auto rest = ConstKeySequenceRange(std::next(it), sequence.end());
        m_output_on_release.push_back({ trigger_event.key, rest, context_index });
        break;
      }
    }
    else if (is_virtual_key(event.key)) {
      update_virtual_key(event, trigger, context_index);
    }
    else{
      update_output(event, trigger, context_index);
    }
  }
}

void Stage::update_virtual_key(const KeyEvent& event, 
    const Trigger& trigger, int context_index) {
  // inserting a Virtual Down to toggle
  const auto times_down = count_key_downs(m_sequence, event.key) +
                          count_key_downs(m_output_buffer, event.key); 
  const auto pressed = (times_down % 2 == 1);
  if (event.state == KeyState::Not) {
    // Not only toggles when already pressed
    if (pressed) {
      update_output({ event.key, KeyState::Down }, trigger, context_index);
      update_output({ event.key, KeyState::Up }, trigger, context_index);
    }
  }
  else {
    // Down toggles when not already pressed or unconditionally
    if (!pressed || m_virtual_keys_toggle)
      update_output(event, trigger, context_index);
  }
}

void Stage::forward_from_sequence() {
  // TODO: this function likely needs a refactoring
  for (auto it = begin(m_sequence); it != end(m_sequence); ++it) {
    auto& event = *it;
    if (event.state == KeyState::Down || event.state == KeyState::DownMatched) {
      const auto up = std::find(it, end(m_sequence),
        KeyEvent{ event.key, KeyState::Up });
      if (up != end(m_sequence)) {
        // erase Down when Up is following
        update_output(event, event.key);
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
    else {
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
        if (it->pressed_twice && is_virtual_key(event.key)) {
          // allow to toggle virtual key which is still hold by ContextActive
          it->pressed_twice = false;

          m_output_buffer.push_back(event);
        }
        else if (it->pressed_twice && !it->suppressed) {
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
      if (it != end(m_output_down) && 
          !is_virtual_key(event.key) &&
          !is_action_key(event.key)) {
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
        if (output.temporarily_released && !output.suppressed) {
          output.temporarily_released = false;
          m_output_buffer.emplace_back(output.key, KeyState::Down);
          m_temporary_reapplied = true;

          if (output.key == event.key)
            return;
        }
        else if (output.temporarily_released &&
                 output.key == event.key) {
          // when it is a common modifier and 
          // was the last output, simply undo releasing
          if (is_common_modifier(event.key) &&
              !m_output_buffer.empty() && 
              m_output_buffer.back() == KeyEvent(event.key, KeyState::Up)) {
            m_output_buffer.pop_back();
            output.temporarily_released = false;
            return;
          }
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
    case KeyState::UpMatched:
      // ignored
      break;

    case KeyState::NoMightMatch:
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
    else if (it->key == Key::timeout) {
      // convert all timeout events to UpMatched
      it->state = KeyState::UpMatched;
      ++i;
      continue;
    }

    m_sequence.erase(it);
    --length;
  }
}

void Stage::clean_up_history() {
  // remove all events from beginning of history which
  // prevent all no-might-match mappings from matching
  auto input_timeout_event = KeyEvent{ };
  auto any_key_matches = std::vector<Key>{ };
  while (!m_history.empty()) {
    const auto event = m_history.front();

    // Ups of common modifiers are removed when everything before was removed
    if (event.state == KeyState::Up) {
      assert(is_common_modifier(event.key));
      m_history.erase(m_history.begin());
      continue;
    }
    assert(event.state == KeyState::Down);

    // do not remove Down without Up
    const auto up_event = KeyEvent{ event.key, KeyState::Up, event.value };
    if (!contains(m_history, up_event))
      return;

    for (auto context_index : m_active_contexts)
      for (const auto& context_input : m_contexts[context_index].inputs)
        if (is_no_might_match_mapping(context_input.input)) {
          // pass without NoMightMatch, so it does not skip events at the front
          const auto& input = without_first(context_input.input);
          if (m_match(input, m_history, true, &any_key_matches,
                &input_timeout_event) == MatchResult::might_match)
            return;
        }

    m_history.erase(m_history.begin());

    // keep Up of common modifiers, immediately remove others
    if (!is_common_modifier(event.key))
      m_history.erase(std::find(m_history.begin(), m_history.end(), up_event));
  }
}
