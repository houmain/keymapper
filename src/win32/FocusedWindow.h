#pragma once

#include <memory>
#include <string>

class FocusedWindow;
struct FreeFocusedWindow { void operator()(FocusedWindow* window); };
using FocusedWindowPtr = std::unique_ptr<FocusedWindow, FreeFocusedWindow>;

FocusedWindowPtr create_focused_window();
bool update_focused_window(FocusedWindow& window);
const std::string& get_class(const FocusedWindow& window);
const std::string& get_title(const FocusedWindow& window);
bool is_inaccessible(const FocusedWindow& window);