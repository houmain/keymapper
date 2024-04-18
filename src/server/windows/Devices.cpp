
#include "Devices.h"
#include "common/output.h"
#include <hidusage.h>
#include <hidsdi.h>
#include <initguid.h>
#include <Devpkey.h>
#include <Cfgmgr32.h>
#include <thread>
#include <atomic>
#include <map>
#include <optional>

#define INTERCEPTION_STATIC
#include "interception.h"

namespace {
  KeyEvent get_key_event(const InterceptionKeyStroke& stroke) {
    const auto key = Key{ stroke.code |
      (stroke.state & INTERCEPTION_KEY_E0 ? 0xE000u : 0u) };
    const auto state = ((stroke.state & INTERCEPTION_KEY_UP) ?
      KeyState::Up : KeyState::Down);
    return KeyEvent{ key, state };
  }

  InterceptionKeyStroke get_interception_stroke(const KeyEvent& event) {
    auto scan_code = static_cast<unsigned short>(*event.key);
    auto state = static_cast<unsigned short>(event.state == KeyState::Up ?
      INTERCEPTION_KEY_UP : INTERCEPTION_KEY_DOWN);
    if (scan_code & 0xE000) {
      scan_code &= ~0xE000;
      state |= INTERCEPTION_KEY_E0;
    }
    return InterceptionKeyStroke{ scan_code, state, 0 };
  }
} // namespace

class Interception {
private:
#define INIT_PROC(NAME) decltype(::NAME)* NAME = \
  reinterpret_cast<decltype(::NAME)*>(m_module ? GetProcAddress(m_module, #NAME) : nullptr)

  const HMODULE m_module;
  INIT_PROC(interception_create_context);
  INIT_PROC(interception_destroy_context);
  INIT_PROC(interception_set_filter);
  INIT_PROC(interception_is_keyboard);
  INIT_PROC(interception_is_mouse);
  INIT_PROC(interception_wait_with_timeout);
  INIT_PROC(interception_receive);
  INIT_PROC(interception_send);
  INIT_PROC(interception_get_hardware_id);
  InterceptionContext m_context{ };
  std::thread m_thread;
  std::atomic<bool> m_shutdown{ };
  std::vector<std::pair<HANDLE, std::wstring>> m_handle_with_hardware_ids;
  std::map<HANDLE, InterceptionDevice> m_device_by_handle;
  std::map<InterceptionDevice, HANDLE> m_handle_by_device;
  InterceptionDevice m_last_device{ };

public:
  Interception()
    : m_module(LoadLibraryA("interception.dll")) {
  }

  ~Interception() {
    m_shutdown.store(true);
    if (m_thread.joinable())
      m_thread.join();
    if (m_context)
      interception_destroy_context(m_context);
    if (m_module)
      FreeLibrary(m_module);
  }

  bool initialize(HWND window, UINT input_message, std::string* error_message) {
    if (!m_module || !interception_create_context) {
      *error_message = "Initializing Interception driver failed.\nPlease put the 'interception.dll' in installation directory and restart.";
      return false;
    }

    m_context = interception_create_context();
    if (!m_context) {
      *error_message = "Initializing Interception driver failed.\nPlease install and reboot.";
      return false;
    }

    interception_set_filter(m_context, interception_is_keyboard,
      INTERCEPTION_FILTER_KEY_DOWN | 
      INTERCEPTION_FILTER_KEY_UP | 
      INTERCEPTION_FILTER_KEY_E0);

    m_thread = std::thread(&Interception::thread_func, 
      this, window, input_message);
    return true;
  }

  void set_device_hardware_ids(HANDLE device, std::wstring hardware_ids) {
    m_handle_with_hardware_ids.emplace_back(device, std::move(hardware_ids));
  }

  void send_input(const KeyEvent& event) {
    InterceptionStroke stroke;
    auto* keystroke = reinterpret_cast<InterceptionKeyStroke*>(&stroke);
    *keystroke = get_interception_stroke(event);
    interception_send(m_context, m_last_device, &stroke, 1);
  }

private:
  std::wstring get_hardware_ids(InterceptionDevice device) {
    auto buffer = std::wstring();
    buffer.resize(256);
    const auto result = interception_get_hardware_id(m_context, device, 
      buffer.data(), static_cast<UINT>(buffer.size() * sizeof(wchar_t)));
    buffer.resize(result / sizeof(wchar_t));
    return buffer;
  }

  std::optional<std::tuple<int, int, int>> get_vid_pid_rev(const wchar_t* id) {
    int vid, pid, rev;
    if (swscanf_s(id, L"HID\\VID_%x&PID_%x&REV_%x", &vid, &pid, &rev) == 3)
      return std::make_tuple(vid, pid, rev);
    return std::nullopt;
  }

  bool match_hardware_ids(const std::wstring& list_a, const std::wstring& list_b) {
    for (auto a = std::wstring::size_type{ }; 
         a < list_a.size(); a = list_a.find(L'\0', a) + 1) {
      const auto entry_a = std::wstring_view(&list_a[a]);
      const auto vid_pid_rev_a = get_vid_pid_rev(entry_a.data());
      if (entry_a.find(L"\\") != std::wstring::npos)
        for (auto b = std::wstring::size_type{ }; 
              b < list_b.size(); b = list_b.find(L'\0', b) + 1) {
          const auto entry_b = std::wstring_view(&list_b[b]);
          if (entry_a == entry_b)
            return true;
          if (vid_pid_rev_a && 
              vid_pid_rev_a == get_vid_pid_rev(entry_b.data()))
            return true;
        }
    }
    return false;
  }

  HANDLE get_device_handle_by_hardware_id(const std::wstring& ids) {
    for (const auto& [handle, hardware_ids] : m_handle_with_hardware_ids)
      if (match_hardware_ids(hardware_ids, ids))
        return handle;
    return 0;
  }

  HANDLE get_device_handle(InterceptionDevice device) {
    auto handle = m_handle_by_device[device];
    if (!handle)
      if (handle = get_device_handle_by_hardware_id(get_hardware_ids(device))) {
        m_handle_by_device[device] = handle;
        m_device_by_handle[handle] = device;
      }
    return handle;
  }

  void thread_func(HWND window, UINT input_message) {
    InterceptionStroke stroke;
    while (!m_shutdown.load()) {
      const auto timeout_ms = 100;
      const auto device = interception_wait_with_timeout(m_context, timeout_ms);
      if (interception_receive(m_context, device, &stroke, 1) > 0) {
        const auto* keystroke = reinterpret_cast<InterceptionKeyStroke*>(&stroke);
        const auto event = get_key_event(*keystroke);
        if (auto device_handle = get_device_handle(device)) {
          m_last_device = device;

          if (::SendMessageA(window, input_message, MAKEWPARAM(event.key, event.state), 
              reinterpret_cast<LPARAM>(device_handle)) == 1)
            continue;
        }
        interception_send(m_context, device, &stroke, 1);
      }
    }
  }
};

//-------------------------------------------------------------------------

Devices::Devices() = default;
Devices::~Devices() = default;

bool Devices::initialize(HWND window, UINT input_message) {
  if (m_window)
    return initialized();
  m_window = window;

  verbose("Initializing Interception");
  auto interception = std::make_unique<Interception>();
  if (!interception->initialize(window, input_message, &m_error_message)) {
    verbose("%s", m_error_message.c_str());
    return false;
  }

  verbose("Requesting device messages");
  const auto flags = DWORD{ RIDEV_DEVNOTIFY };
  auto devices = std::vector<RAWINPUTDEVICE>{ };
  for (auto usage : { HID_USAGE_GENERIC_KEYBOARD })
    devices.push_back({ HID_USAGE_PAGE_GENERIC, usage, flags, window });
  if (::RegisterRawInputDevices(devices.data(), 
      static_cast<UINT>(devices.size()), sizeof(RAWINPUTDEVICE)) == FALSE)
    return false;

  m_interception = std::move(interception);
  return true;
}

bool Devices::initialized() {
  return static_cast<bool>(m_interception);
}

void Devices::shutdown() {
  m_interception.reset();
  m_window = nullptr;
}

// inspired by https://github.com/DJm00n/RawInputDemo
void Devices::on_device_attached(HANDLE device) {
  // get device info
  auto size = UINT{ sizeof(RID_DEVICE_INFO) };
  auto device_info = RID_DEVICE_INFO{ };
  auto read = ::GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &device_info, &size);
  if (read != size)
    return;
  
  // get interface
  ::GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &size);
  auto interface = std::wstring(size, ' ');
  read = ::GetRawInputDeviceInfoW(device, 
    RIDI_DEVICENAME, interface.data(), &size);
  if (read != size)
    return;

  const auto get_string = [](auto CM_getter, auto subject, const DEVPROPKEY* property_key) -> std::wstring {
    auto property_type = DEVPROPTYPE{ };
    auto property_size = ULONG{ };
    CM_getter(subject, property_key, &property_type, nullptr, &property_size, 0);
    auto result = std::wstring(property_size / sizeof(wchar_t), 0);
    if (CM_getter(subject, property_key, &property_type, 
        reinterpret_cast<BYTE*>(result.data()), &property_size, 0) != CR_SUCCESS)
      return { };
    // pop null terminator
    result.pop_back();
    return result;
  };

  // get instance id
  auto instance_id = get_string(&CM_Get_Device_Interface_PropertyW, 
    interface.c_str(), &DEVPKEY_Device_InstanceId);

  // get node handle
  auto node_handle = DEVINST{ };
  if (::CM_Locate_DevNodeW(&node_handle, instance_id.data(),
      CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
    return;

  auto device_name = get_string(CM_Get_DevNode_PropertyW, node_handle, &DEVPKEY_NAME);
  auto hardware_ids = get_string(CM_Get_DevNode_PropertyW, node_handle, &DEVPKEY_Device_HardwareIds);

  // try to get product name
  if (auto file = CreateFileW(interface.c_str(), 0, FILE_SHARE_READ, 
      nullptr, OPEN_EXISTING, NULL, nullptr); file != INVALID_HANDLE_VALUE) {
    auto product = std::vector<wchar_t>(256);
    if (::HidD_GetProductString(file, product.data(), 
        static_cast<ULONG>(product.size())) && product[0])
      device_name = product.data();
    ::CloseHandle(file);
  }
  m_device_handles.push_back(device);
  m_device_names.push_back(wide_to_utf8(device_name));

  if (m_interception)
    m_interception->set_device_hardware_ids(device, std::move(hardware_ids));
}

void Devices::on_device_removed(HANDLE device) {
  if (auto index = get_device_index(device); index >= 0) {
    m_device_handles.erase(m_device_handles.begin() + index);
    m_device_names.erase(m_device_names.begin() + index);
  }
}

int Devices::get_device_index(HANDLE device) const {
  if (!device)
    return -1;
  const auto begin = m_device_handles.begin();
  const auto end = m_device_handles.end();
  const auto it = std::find(begin, end, device);
  return (it != end ? static_cast<int>(std::distance(begin, it)) : -1);
}

const std::string& Devices::get_device_name(HANDLE device) const {
  if (auto index = get_device_index(device); index >= 0)
    return m_device_names[static_cast<size_t>(index)];

  static const std::string s_empty;
  return s_empty;
}

void Devices::send_input(const KeyEvent& event) {
  if (m_interception)
    m_interception->send_input(event);
}