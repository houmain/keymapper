
#include "MultiStage.h"

namespace {
  bool is_server_event(const KeyEvent& event) {
    return (event.key == Key::timeout ||
      is_virtual_key(event.key) ||
      is_action_key(event.key));
  }
} // namespace

MultiStage::MultiStage(std::vector<StagePtr> stages) 
  : m_stages(std::move(stages)) {

  for (const auto& stage : m_stages)
    m_context_count += stage->contexts().size();
}

bool MultiStage::has_mouse_mappings() const {
  return std::any_of(begin(m_stages), end(m_stages), 
    [](const auto& stage) { return stage->has_mouse_mappings(); });
}

bool MultiStage::has_device_filters() const {
  return std::any_of(begin(m_stages), end(m_stages), 
    [](const auto& stage) { return stage->has_device_filters(); });
}

bool MultiStage::is_clear() const {
  return std::all_of(begin(m_stages), end(m_stages), 
    [](const auto& stage) { return stage->is_clear(); });
}

std::vector<Key> MultiStage::get_output_keys_down() const {
  if (m_stages.empty())
    return { };
  return m_stages.back()->get_output_keys_down();
}

void MultiStage::evaluate_device_filters(const std::vector<DeviceDesc>& device_descs) {
  for (auto& stage : m_stages)
    stage->evaluate_device_filters(device_descs);
}

KeySequence MultiStage::set_active_client_contexts(const std::vector<int>& indices) {
  m_active_client_contexts = indices;

  // set active contexts of each stage (translate so each starts at 0)
  auto context_offset = 0;
  for (auto& stage : m_stages) {
    // output of previous stage is input of current
    std::swap(m_context_active_buffer, m_output_buffer);
    m_output_buffer.clear();
    for (const auto& event : m_context_active_buffer) 
      if (is_server_event(event)) {
        // forward to server
        m_output_buffer.push_back(event);
      }
      else {
        auto output = stage->update(event, Stage::no_device_index);
        m_output_buffer.insert(m_output_buffer.end(), 
          output.begin(), output.end());
        stage->reuse_buffer(std::move(output));
      }

    const auto indices_begin = context_offset;
    const auto indices_end = context_offset + static_cast<int>(stage->contexts().size());
    context_offset = indices_end;
    m_indices_buffer.clear();
    for (auto index : indices)
      if (index >= indices_begin && index < indices_end)
        m_indices_buffer.push_back(index - indices_begin);
    auto output = stage->set_active_client_contexts(m_indices_buffer);
    m_output_buffer.insert(m_output_buffer.end(), 
      output.begin(), output.end());
    stage->reuse_buffer(std::move(output));
  }
  return m_output_buffer;
}

KeySequence MultiStage::update(KeyEvent event, int device_index) {  
  m_output_buffer.push_back(event);
  
  auto first_stage = true;
  for (const auto& stage : m_stages) {
    const auto update_stage = [&](const KeyEvent& event) {
      auto output = stage->update(event, device_index);
      m_output_buffer.insert(m_output_buffer.end(), 
        output.begin(), output.end());
      stage->reuse_buffer(std::move(output));
    };

    // output of previous stage is input of current
    std::swap(m_input_buffer, m_output_buffer);
    m_output_buffer.clear();

    // apply timeout in all stages
    if (event.key == Key::timeout && !first_stage)
      update_stage(event);

    // toggle virtual key in all stages
    if (!first_stage && is_virtual_key(event.key))
      update_stage(event);

    for (const auto& event : m_input_buffer)
      if (!first_stage && is_server_event(event)) {
        // forward to server
        m_output_buffer.push_back(event);
      }
      else {
        update_stage(event);
      }

    first_stage = false;
  }
  return std::move(m_output_buffer);
}

void MultiStage::reuse_buffer(KeySequence&& buffer) {
  m_output_buffer = std::move(buffer);
  m_output_buffer.clear();
}

void MultiStage::validate_state(const std::function<bool(Key)>& is_down) {
  if (!m_stages.empty())
    m_stages.front()->validate_state(is_down);
}

bool MultiStage::should_exit() const {
  if (m_stages.empty())
    return false;
  return m_stages.front()->should_exit();
}
