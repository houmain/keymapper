
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

  bool has_non_optional(const KeySequence& sequence) {
    return std::any_of(begin(sequence), end(sequence),
      [](const KeyEvent& e) {
        return (e.state == KeyState::Up || e.state == KeyState::Down);
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
      [](const KeyEvent& event) { return is_mouse_button(event.key); });
  }

  bool has_mouse_mappings(const std::vector<Stage::Context>& contexts) {
    for (const auto& context : contexts)
      for (const auto& input : context.inputs)
        if (has_mouse_mappings(input.input))
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
} // namespace

Stage::Stage(std::vector<Context> contexts)
  : m_contexts(sort_command_outputs(std::move(contexts))),
    m_has_mouse_mappings(::has_mouse_mappings(m_contexts)) {
}

bool Stage::is_clear() const {
  return m_output_down.empty() && 
         m_sequence.empty() &&
         !m_sequence_might_match &&
         !m_current_timeout;
}

void Stage::evaluate_device_filters(const std::vector<std::string>& device_names) {
  for (auto& context : m_contexts)
    if (!context.device_filter.empty()) {
      context.matching_device_bits = { };
      auto bit = decltype(context.matching_device_bits){ 1 };
      if (is_regex(context.device_filter)) {
        const auto regex = parse_regex(context.device_filter);
        for (const auto& device_name : device_names) {
          if (std::regex_search(device_name, regex))
            context.matching_device_bits |= bit;
          bit <<= 1;
        }
      }
      else {
        for (const auto& device_name : device_names) {
          if (device_name == context.device_filter)
            context.matching_device_bits |= bit;
          bit <<= 1;
        }
      }
    }
}

bool Stage::device_matches_filter(const Context& context, int device_index) const {
  return (context.matching_device_bits >> device_index) & 1;
}

void Stage::set_active_contexts(const std::vector<int> &indices) {
  // order of active contexts is relevant
  assert(std::is_sorted(begin(indices), end(indices)));
  for (auto i : indices)
    assert(i >= 0 && i < static_cast<int>(m_contexts.size()));

  m_active_contexts = indices;
}

void Stage::advance_exit_sequence(const KeyEvent& event) {
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
        return !is_virtual_key(event.key) && !is_down(event.key);
      }),
    end(m_sequence));

  m_output_down.erase(
    std::remove_if(begin(m_output_down), end(m_output_down),
      [&](const OutputDown& output) {
        return !is_down(output.trigger);
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
    const auto& command_outputs =
      m_contexts[m_active_contexts[i]].command_outputs;
    const auto it = std::lower_bound(
      command_outputs.rbegin(), command_outputs.rend(), output_index,
      [](const CommandOutput& a, int index) { return a.index < index; });
    if (it != command_outputs.rend() && it->index == output_index)
      return &it->output;
  }
  return nullptr;
}

std::pair<MatchResult, const KeySequence*> Stage::match_input(
    ConstKeySequenceRange sequence, int device_index, bool accept_might_match) {
  for (auto i : m_active_contexts) {
    const auto& context = m_contexts[i];
    if (!device_matches_filter(context, device_index))
      continue;

    for (const auto& input : context.inputs) {
      auto input_timeout_event = KeyEvent{ };
      const auto result = m_match(input.input, sequence,
        &m_any_key_matches, &input_timeout_event);

      if (accept_might_match && result == MatchResult::might_match) {
        
        if (input_timeout_event.key == Key::timeout) {
          // next apply_input should reply timeout Up event
          m_output_buffer.emplace_back(Key::timeout, 
            KeyState::Up, +input_timeout_event.timeout);

          // track timeout - use last key Down as trigger
          if (auto trigger = find_last_down_event(sequence))
            if (!m_current_timeout || 
                m_current_timeout->state != input_timeout_event.state ||
                m_current_timeout->trigger != trigger->key)
              m_current_timeout = {
                input_timeout_event,
                trigger->key
              };
        }
        return { MatchResult::might_match, nullptr };
      }

      if (result == MatchResult::match)
        if (auto output = find_output(context, input.output_index))
          return { MatchResult::match, output };
    }
  }
  return { MatchResult::no_match, nullptr };
}

void Stage::apply_input(const KeyEvent event, int device_index) {
  assert(event.state == KeyState::Down ||
         event.state == KeyState::Up);

  // suppress short timeout after not-timeout was exceeded
  if (m_current_timeout && m_current_timeout->state == KeyState::Not) {
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

  while (has_non_optional(m_sequence)) {
    // find first mapping which matches or might match sequence
    auto sequence = ConstKeySequenceRange(m_sequence);
    auto [result, output] = match_input(sequence, device_index, true);

    // hold back sequence when something might match
    if (result == MatchResult::might_match) {
      m_sequence_might_match = true;
      break;
    }

    // when might match failed, look for exact match in sequence start
    if (result == MatchResult::no_match &&
        m_sequence_might_match) {

      while (sequence.size() > 1) {
        sequence.pop_back();
        if (!has_unmatched_down(sequence))
          break;

        std::tie(result, output) = match_input(sequence, device_index, false);
        if (result == MatchResult::match)
          break;
      }
    }
    m_sequence_might_match = false;

    // when a timeout matched once, prevent following timeout
    // cancellation from matching another input
    if (m_current_timeout && m_current_timeout->state == KeyState::Up) {
      if (event.key == Key::timeout) {
        if (result == MatchResult::match) {
          if (!m_current_timeout->matched_output) {
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
    if (m_current_timeout && m_current_timeout->state == KeyState::Not) {
      if (result == MatchResult::no_match)
        m_current_timeout->not_exceeded = true;
    }

    if (result == MatchResult::match) {
      auto trigger = event.key;

      // for timeouts use last key press as trigger, if it is still down
      if (trigger == Key::timeout && m_current_timeout)
        if (auto it = rfind_key(m_sequence, m_current_timeout->trigger);
            it != cend(m_sequence) && it->state != KeyState::Up)
          trigger = m_current_timeout->trigger;

      apply_output(*output, trigger);

      // release new output when triggering input was released
      if (event.state == KeyState::Up)
        release_triggered(event.key);

      finish_sequence(sequence);

      // continue when only the start of the sequence matched
    }
    else {
      // when still no match was found, forward beginning of sequence
      forward_from_sequence();
    }
  }

  if (m_sequence.empty())
    m_current_timeout.reset();
}

void Stage::release_triggered(Key key) {
  const auto it = std::stable_partition(begin(m_output_down), end(m_output_down),
    [&](const auto& k) { return k.trigger != key; });
  std::for_each(
    std::make_reverse_iterator(end(m_output_down)),
    std::make_reverse_iterator(it),
    [&](const OutputDown& k) {
      if (!k.temporarily_released)
        m_output_buffer.push_back({ k.key, KeyState::Up });
    });
  m_output_down.erase(it, end(m_output_down));
}

void Stage::apply_output(const KeySequence& expression, Key trigger) {
  for (const auto& event : expression)
    if (is_virtual_key(event.key)) {
      if (event.state == KeyState::Down)
        m_toggle_virtual_keys.push_back(event.key);
    }
    else if (event.key == Key::any) {
      for (auto key : m_any_key_matches)
        update_output({ key, event.state }, trigger);
    }
    else {
      update_output(event, trigger);
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

void Stage::update_output(const KeyEvent& event, Key trigger) {
  const auto it = std::find_if(begin(m_output_down), end(m_output_down),
    [&](const OutputDown& down_key) { return down_key.key == event.key; });

  switch (event.state) {
    case KeyState::Up: {
      if (it != end(m_output_down)) {
        if (it->pressed_twice) {
          // try to remove current down
          auto it2 = find_key(m_output_buffer, event.key);
          if (it2 != m_output_buffer.end())
            m_output_buffer.erase(it2);

          it->pressed_twice = false;
        }
        else {
          // only releasing trigger can permanently release
          if (it->trigger == trigger)
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
        if (output.temporarily_released && !output.suppressed) {
          output.temporarily_released = false;
          m_output_buffer.emplace_back(output.key, KeyState::Down);
          m_temporary_reapplied = true;

          if (output.key == event.key)
            return;
        }

      if (it == end(m_output_down)) {
        if (event.key != Key::timeout)
          m_output_down.push_back({ event.key, trigger, false, false });
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
      m_output_buffer.emplace_back(event.key, KeyState::Down, event.timeout);
      break;
    }

    case KeyState::OutputOnRelease: {
      m_output_buffer.emplace_back(event.key, event.state);
      break;
    }

    case KeyState::DownMatched:
      // ignored
      break;

    case KeyState::UpAsync:
    case KeyState::DownAsync:
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

  // toggle virtual keys
  for (auto key : m_toggle_virtual_keys) {
    auto it = find_key(m_sequence, key);
    if (it != cend(m_sequence))
      m_sequence.erase(it);
    else
      m_sequence.emplace_back(key, KeyState::DownMatched);
  }
  m_toggle_virtual_keys.clear();
  m_temporary_reapplied = false;
}
