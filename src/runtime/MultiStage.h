#pragma once

#include "Stage.h"

using StagePtr = std::unique_ptr<Stage>;
using MultiStagePtr = std::unique_ptr<class MultiStage>;

class MultiStage {
public:
  explicit MultiStage(std::vector<StagePtr> stages = { });

  const size_t context_count() const { return m_context_count; }
  const std::vector<StagePtr>& stages() const { return m_stages; }
  const std::vector<int>& active_client_contexts() const { return m_active_client_contexts; }
  bool has_mouse_mappings() const;
  bool has_device_filters() const;

  bool is_clear() const;
  std::vector<Key> get_output_keys_down() const;
  void evaluate_device_filters(const std::vector<DeviceDesc>& device_descs);
  KeySequence set_active_client_contexts(const std::vector<int>& indices);
  KeySequence update(KeyEvent event, int device_index);
  void reuse_buffer(KeySequence&& buffer);
  void validate_state(const std::function<bool(Key)>& is_down);
  bool should_exit() const;

private:
  size_t m_context_count{ };
  std::vector<StagePtr> m_stages;
  std::vector<int> m_active_client_contexts;

  // temporary buffer
  KeySequence m_output_buffer;
  KeySequence m_input_buffer;
  KeySequence m_context_active_buffer;
  std::vector<int> m_indices_buffer;
};