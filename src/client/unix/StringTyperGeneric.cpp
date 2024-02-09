
#include "StringTyperImpl.h"

namespace {
  Key get_key(char c) {
    switch (std::toupper(static_cast<int>(c))) {
      case 'A': return Key::A;
      case 'B': return Key::B;
      case 'C': return Key::C;
      case 'D': return Key::D;
      case 'E': return Key::E;
      case 'F': return Key::F;
      case 'G': return Key::G;
      case 'H': return Key::H;
      case 'I': return Key::I;
      case 'J': return Key::J;
      case 'K': return Key::K;
      case 'L': return Key::L;
      case 'M': return Key::M;
      case 'N': return Key::N;
      case 'O': return Key::O;
      case 'P': return Key::P;
      case 'Q': return Key::Q;
      case 'R': return Key::R;
      case 'S': return Key::S;
      case 'T': return Key::T;
      case 'U': return Key::U;
      case 'V': return Key::V;
      case 'W': return Key::W;
      case 'X': return Key::X;
      case 'Y': return Key::Y;
      case 'Z': return Key::Z;
      case '0': return Key::Digit0;
      case '1': return Key::Digit1;
      case '2': return Key::Digit2;
      case '3': return Key::Digit3;
      case '4': return Key::Digit4;
      case '5': return Key::Digit5;
      case '6': return Key::Digit6;
      case '7': return Key::Digit7;
      case '8': return Key::Digit8;
      case '9': return Key::Digit9;
      case ' ': return Key::Space;
      case '\t': return Key::Tab;
      case '\r': return Key::Enter;
    }
    return Key::none;
  }
} // namespace

class StringTyperGeneric : public StringTyperImpl {
public:
  StringTyperGeneric() {
    for (auto c = '0'; c <= '9'; ++c)
      m_dictionary[c] = { get_key(c) };
    for (auto c = 'a'; c <= 'z'; ++c)
      m_dictionary[c] = { get_key(c) };
    for (auto c = 'A'; c <= 'Z'; ++c)
      m_dictionary[c] = { get_key(c), StringTyper::Shift };
    for (auto c : { ' ', '\t', '\r' })
      m_dictionary[c] = { get_key(c) };
  }
};

std::unique_ptr<StringTyperImpl> make_string_typer_generic() {
  return std::make_unique<StringTyperGeneric>();
}
