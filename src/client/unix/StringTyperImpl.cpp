
#include "StringTyperImpl.h"
#include "common/output.h"

using MakeStringTyperImpl = std::unique_ptr<StringTyperImpl>();
MakeStringTyperImpl make_string_typer_x11;
MakeStringTyperImpl make_string_typer_generic;

StringTyper::StringTyper() {
  const auto systems = std::initializer_list<std::pair<const char*, MakeStringTyperImpl*>>{
#if defined(ENABLE_X11)
    { "X11", &make_string_typer_x11 },
#endif
    { "generic", &make_string_typer_generic },
  };

  for (auto [name, make_system] : systems)
    if (auto system = make_system()) {
      m_impl = std::move(system);
#if !defined(KEYMAPPER_TEST)
      verbose("  initialized string typing with %s layout", name);
#endif
      break;
    }
}

StringTyper::StringTyper(StringTyper&& rhs) noexcept = default;
StringTyper& StringTyper::operator=(StringTyper&& rhs) noexcept = default;
StringTyper::~StringTyper() = default;

void StringTyper::type(std::string_view string, const AddKey& add_key) const {
  if (m_impl)
    m_impl->type(string, add_key);
}
