
#if defined(ENABLE_CARBON)

#include "FocusedWindowImpl.h"
#include <Carbon/Carbon.h>
#include <libproc.h>

namespace {
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated"

  std::string to_string(CFStringRef string) {
    const auto length = CFStringGetLength(string);
    const auto max_size= CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);  
    auto buffer = std::vector<char>(max_size);
    if (CFStringGetCString(string, buffer.data(), max_size, kCFStringEncodingUTF8))
      return buffer.data();
    return { };
  }  

  std::string get_process_name(const ProcessSerialNumber& psn) {
    auto name = CFStringRef{ };
    if (CopyProcessName(&psn, &name) != noErr)
      return { };
    auto string = to_string(name);
    CFRelease(name);
    return string;
  }

  std::string get_process_path(const ProcessSerialNumber& psn) {
    auto pid = pid_t{ };
    GetProcessPID(&psn, &pid);
    char buffer[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, buffer, sizeof(buffer)) > 0)
      return buffer;
    return { };
  }

  ProcessSerialNumber get_front_process() {
    auto psn = ProcessSerialNumber{ };
    GetFrontProcess(&psn);
    return psn;
  }
  #pragma clang diagnostic pop
} // namespace

class FocusedWindowCarbon : public FocusedWindowSystem {
private:
  FocusedWindowData& m_data;
  EventHandlerUPP m_event_handler_upp{ };
  EventHandlerRef m_event_handler_ref{ };
  bool m_front_app_changed{ };

public:
  explicit FocusedWindowCarbon(FocusedWindowData* data)
    : m_data(*data) {
  }

  FocusedWindowCarbon(const FocusedWindowCarbon&) = delete;
  FocusedWindowCarbon& operator=(const FocusedWindowCarbon&) = delete;

  ~FocusedWindowCarbon() {
    RemoveEventHandler(m_event_handler_ref);
    DisposeEventHandlerUPP(m_event_handler_upp);    
  }

  bool initialize() {
    auto event_target = GetApplicationEventTarget();
    m_event_handler_upp = NewEventHandlerUPP(&FocusedWindowCarbon::event_handler_callback);
    auto event_type = EventTypeSpec{ kEventClassApplication, kEventAppFrontSwitched };
    if (InstallEventHandler(event_target, m_event_handler_upp, 1, &event_type, 
          this, &m_event_handler_ref) != noErr)
      return false;

    handle_front_app_changed(get_front_process());
    return true;
  }

  bool update() override {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    return std::exchange(m_front_app_changed, false);
  }

private:
  static OSStatus event_handler_callback(EventHandlerCallRef handler_call, 
      EventRef event, void* context) {
    return static_cast<FocusedWindowCarbon*>(context)->handle_event(event);
  }

  OSStatus handle_event(EventRef event) {
    auto psn = ProcessSerialNumber{ };
    if (GetEventParameter(event, kEventParamProcessID, typeProcessSerialNumber, 
        nullptr, sizeof(psn), nullptr, &psn) != noErr)
      return -1;
    handle_front_app_changed(psn);
    return noErr;
  }

  void handle_front_app_changed(const ProcessSerialNumber& psn) {
    m_data.window_class = get_process_name(psn);
    m_data.window_path = get_process_path(psn);
    m_front_app_changed = true;
  }
};

std::unique_ptr<FocusedWindowSystem> make_focused_window_carbon(FocusedWindowData* data) {
  auto impl = std::make_unique<FocusedWindowCarbon>(data);
  if (!impl->initialize())
    return { };
  return impl;
}

#endif // ENABLE_CARBON
