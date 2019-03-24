cmake_minimum_required(VERSION 3.0.0)
project(keymapper LANGUAGES CXX)

option(ENABLE_TEST "Enable tests")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

include_directories(src)

set(SOURCES_CONFIG
  src/config/Config.h
  src/config/ParseConfig.cpp
  src/config/ParseConfig.h
  src/config/ParseKeySequence.cpp
  src/config/ParseKeySequence.h
  src/config/key_names.cpp
  src/config/key_names.h
  src/config/string_iteration.h
)

set(SOURCES_RUNTIME
  src/runtime/Key.h
  src/runtime/MatchKeySequence.cpp
  src/runtime/MatchKeySequence.h
  src/runtime/Stage.cpp
  src/runtime/Stage.h
)

if(NOT WIN32)
  add_executable(keymapper
    ${SOURCES_CONFIG}
    src/linux/client/ConfigFile.cpp
    src/linux/client/ConfigFile.h
    src/linux/client/FocusedWindow.cpp
    src/linux/client/FocusedWindow.h
    src/linux/client/Settings.cpp
    src/linux/client/Settings.h
    src/linux/client/ipc.cpp
    src/linux/client/ipc.h
    src/linux/client/main.cpp
  )
  target_link_libraries(keymapper X11)

  add_executable(keymapperd
    ${SOURCES_RUNTIME}
    src/linux/server/ipc.cpp
    src/linux/server/ipc.h
    src/linux/server/keyboard.cpp
    src/linux/server/keyboard.h
    src/linux/server/main.cpp
    src/linux/server/uinput_keyboard.cpp
    src/linux/server/uinput_keyboard.h
  )
  target_link_libraries(keymapperd usb-1.0 udev)
else(NOT WIN32)
  add_compile_definitions(ENABLE_INTERCEPTION)
  add_executable(keymapper
    ${SOURCES_CONFIG}
    ${SOURCES_RUNTIME}
    src/win32/ConfigFile.cpp
    src/win32/ConfigFile.h
    src/win32/FocusedWindow.cpp
    src/win32/FocusedWindow.h
    src/win32/LimitSingleInstance.h
    src/win32/Settings.cpp
    src/win32/Settings.h
    src/win32/common.h
    src/win32/interception.h
    src/win32/key_scan_code.cpp
    src/win32/key_scan_code.h
    src/win32/main.cpp
    src/win32/run_hook.cpp
    src/win32/run_interception.cpp
    src/win32/win.h
  )
endif(NOT WIN32)

if(ENABLE_TEST)
  add_executable(test-keymapper
    ${SOURCES_CONFIG}
    ${SOURCES_RUNTIME}
    src/test/catch.hpp
    src/test/test.cpp
    src/test/test.h
    src/test/test0_ParseKeySequence.cpp
    src/test/test1_ParseConfig.cpp
    src/test/test2_MatchKeySequence.cpp
    src/test/test3_Stage.cpp
  )
endif(ENABLE_TEST)
