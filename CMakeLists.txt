cmake_minimum_required(VERSION 3.12.0)
project(keymapper LANGUAGES C CXX)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

find_package(Git)
if(NOT VERSION AND GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE VERSION
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
configure_file("src/common/version.h.in" "${CMAKE_SOURCE_DIR}/src/common/_version.h")

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  # Karabiner-DriverKit requires C++20
  set(CMAKE_CXX_STANDARD 20)
else()
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/GR- /Os /MP /W4 /permissive- /wd4100)
  add_link_options($<$<CONFIG:RELEASE>:/LTCG>)
  add_compile_options($<$<CONFIG:RELEASE>:/GL>)
  add_compile_options($<$<CONFIG:RELEASE>:/MT>)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
  add_compile_options(-Wall -Wpedantic -Wno-unknown-pragmas)

  option(ENABLE_ADDRESS_SANITIZER "Enable Address Sanitizer" FALSE)
  if(ENABLE_ADDRESS_SANITIZER)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
  endif()
endif()

include_directories(src)

set(SOURCES_CONFIG
  src/config/Config.h
  src/config/ParseConfig.cpp
  src/config/ParseConfig.h
  src/config/ParseKeySequence.cpp
  src/config/ParseKeySequence.h
  src/config/get_key_name.cpp
  src/config/get_key_name.h
  src/config/StringTyper.h
  src/config/string_iteration.h
)

set(SOURCES_RUNTIME
  src/runtime/Key.h
  src/runtime/KeyEvent.h
  src/runtime/Timeout.h
  src/runtime/MatchKeySequence.cpp
  src/runtime/MatchKeySequence.h
  src/runtime/Stage.cpp
  src/runtime/Stage.h
  src/runtime/MultiStage.cpp
  src/runtime/MultiStage.h
)

set(SOURCES_CLIENT
  src/client/ConfigFile.cpp
  src/client/ConfigFile.h
  src/client/FocusedWindow.h
  src/client/Settings.cpp
  src/client/Settings.h
  src/client/ServerPort.cpp
  src/client/ServerPort.h
  src/client/ControlPort.cpp
  src/client/ControlPort.h
  src/client/ClientState.cpp
  src/client/ClientState.h  
)

set(SOURCES_SERVER
  src/server/ClientPort.cpp
  src/server/ClientPort.h
  src/server/Settings.cpp
  src/server/Settings.h
  src/server/ServerState.cpp
  src/server/ServerState.h  
  src/server/verbose_debug_io.h
)

set(SOURCES_CONTROL
  src/control/ClientPort.cpp
  src/control/ClientPort.h
  src/control/Settings.cpp
  src/control/Settings.h
  src/control/main.cpp
)

set(SOURCES_COMMON
  src/common/Connection.cpp
  src/common/Connection.h
  src/common/expand_path.h
  src/common/Filter.h
  src/common/Host.cpp
  src/common/Host.h
  src/common/Duration.h
  src/common/DeviceDesc.h
  src/common/KeyInfo.h
  src/common/output.cpp
  src/common/output.h
  src/common/parse_regex.h
  src/common/MessageType.h
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/unix/FocusedWindowImpl.cpp
    src/client/unix/FocusedWindowImpl.h
    src/client/unix/FocusedWindowX11.cpp
    src/client/unix/FocusedWindowWlroots.cpp
    src/client/unix/FocusedWindowDBus.cpp
    src/client/unix/TrayIcon.cpp
    src/client/unix/TrayIcon.h
    src/client/unix/TrayIconGtk.cpp
    src/client/unix/StringTyperImpl.cpp
    src/client/unix/StringTyperImpl.h
    src/client/unix/StringTyperGeneric.cpp
    src/client/unix/StringTyperWayland.cpp
    src/client/unix/StringTyperX11.cpp
    src/client/unix/StringTyperXKB.cpp
    src/client/unix/StringTyperXKB.h
    src/client/unix/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/unix/DeviceDescLinux.h
    src/server/unix/GrabbedDevicesLinux.cpp
    src/server/unix/GrabbedDevices.h
    src/server/unix/main.cpp
    src/server/unix/VirtualDevicesLinux.cpp
    src/server/unix/VirtualDevices.h
  )
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/windows/FocusedWindow.cpp
    src/client/windows/StringTyper.cpp
    src/client/windows/execute_terminal_command.cpp
    src/client/windows/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/windows/Devices.cpp
    src/server/windows/Devices.h
    src/server/windows/main.cpp
    src/server/windows/interception.h
  )
  set(SOURCES_CONTROL ${SOURCES_CONTROL}
    src/control/windows/expand_command.cpp
  )
  set(SOURCES_COMMON ${SOURCES_COMMON}
    src/common/windows/_resource.rc
    src/common/windows/LimitSingleInstance.h
    src/common/windows/win.cpp
    src/common/windows/win.h
  )
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/unix/FocusedWindowImpl.cpp
    src/client/unix/FocusedWindowImpl.h
    src/client/unix/FocusedWindowCarbon.cpp
    src/client/unix/StringTyperImpl.cpp
    src/client/unix/StringTyperImpl.h
    src/client/unix/StringTyperGeneric.cpp
    src/client/unix/StringTyperCarbon.cpp
    src/client/unix/TrayIcon.cpp
    src/client/unix/TrayIcon.h
    src/client/unix/TrayIconCocoa.mm
    src/client/unix/MessageBoxCocoa.mm
    src/client/unix/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/unix/GrabbedDevicesMacOS.cpp
    src/server/unix/GrabbedDevices.h
    src/server/unix/main.cpp
    src/server/unix/VirtualDevicesMacOS.cpp
    src/server/unix/VirtualDevices.h
  )
endif()

add_executable(keymapper WIN32 ${SOURCES_CLIENT} ${SOURCES_COMMON} ${SOURCES_CONFIG})

add_executable(keymapperd WIN32 ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_RUNTIME}
 $<$<NOT:$<CONFIG:Release>>:src/config/get_key_name.cpp>
)

add_executable(keymapperctl ${SOURCES_CONTROL} ${SOURCES_COMMON})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_package(PkgConfig REQUIRED)

  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libudev1, libusb-1.0-0")
  set(CPACK_RPM_PACKAGE_REQUIRES "libusb1")

  pkg_check_modules(XKBCOMMON xkbcommon)
  if(XKBCOMMON_VERSION VERSION_GREATER_EQUAL 1.0)
    set(ENABLE_XKBCOMMON_DEFAULT TRUE)
  endif()
  option(ENABLE_XKBCOMMON "Enable xkbcommon string typing support" ${ENABLE_XKBCOMMON_DEFAULT})
  if(ENABLE_XKBCOMMON)
    target_compile_definitions(keymapper PRIVATE ENABLE_XKBCOMMON)
    if(XKBCOMMON_VERSION VERSION_GREATER_EQUAL 1.6)
      target_compile_definitions(keymapper PRIVATE ENABLE_XKBCOMMON_COMPOSE)
    endif()
  endif()

  find_package(X11)
  if(X11_FOUND)
    set(ENABLE_X11_DEFAULT TRUE)
  endif()
  option(ENABLE_X11 "Enable X11 context update support" ${ENABLE_X11_DEFAULT})
  if(ENABLE_X11)
    target_compile_definitions(keymapper PRIVATE ENABLE_X11)
    target_link_libraries(keymapper X11)

    if(ENABLE_XKBCOMMON)
      target_link_libraries(keymapper X11-xcb xkbcommon-x11)
    endif()
  endif()

  option(ENABLE_DBUS "Enable D-Bus context update support" TRUE)
  if(ENABLE_DBUS)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    include_directories(${DBUS_INCLUDE_DIRS})
    link_directories(${DBUS_LIBRARY_DIRS})
    target_compile_definitions(keymapper PRIVATE ENABLE_DBUS)
    target_link_libraries(keymapper ${DBUS_LIBRARIES})
  endif()

  option(ENABLE_WAYLAND "Enable Wayland context update support" TRUE)
  if(ENABLE_WAYLAND)
    # source: https://github.com/foxcpp/ssr-wlroots/commit/e7144c94ed724c0d56bdde907b99e7c550c7ccaf
    include(WaylandScanner)
    wayland_generate_proto(${CMAKE_SOURCE_DIR}/src/client/unix/wlr-foreign-toplevel-management-unstable-v1.xml)
    wayland_proto_library(wayland-proto wlr-foreign-toplevel-management-unstable-v1)
    target_compile_definitions(keymapper PRIVATE ENABLE_WAYLAND)
    target_link_libraries(keymapper wayland-proto ${XKBCOMMON_LIBRARIES})

    set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, libxkbcommon")
  endif()

  pkg_check_modules(AYATANA_APPINDICATOR ayatana-appindicator3-0.1)
  if(AYATANA_APPINDICATOR_FOUND)
    # link libayatana-appindicator instead of libappindicator when available
    set(APPINDICATOR_FOUND ${AYATANA_APPINDICATOR_FOUND})
    set(APPINDICATOR_LIBRARIES ${AYATANA_APPINDICATOR_LIBRARIES})
    set(APPINDICATOR_INCLUDE_DIRS ${AYATANA_APPINDICATOR_INCLUDE_DIRS})
  else()
    pkg_check_modules(APPINDICATOR appindicator3-0.1)
  endif()

  pkg_check_modules(GTK3 gtk+-3.0)
  if(APPINDICATOR_FOUND AND GTK3_FOUND)
    set(ENABLE_APPINDICATOR_DEFAULT TRUE)
  endif()

  option(ENABLE_APPINDICATOR "Enable tray icon" ${ENABLE_APPINDICATOR_DEFAULT})
  if(ENABLE_APPINDICATOR)
    target_compile_definitions(keymapper PRIVATE ENABLE_APPINDICATOR)
    target_link_libraries(keymapper ${GTK3_LIBRARIES} ${APPINDICATOR_LIBRARIES})
    target_include_directories(keymapper PRIVATE ${GTK3_INCLUDE_DIRS} ${APPINDICATOR_INCLUDE_DIRS})

    if(AYATANA_APPINDICATOR_FOUND)
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libayatana-appindicator3-1")
      set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, libayatana-appindicator-gtk3")
    else()
      set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libappindicator3-1")
      set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, libappindicator-gtk3")
    endif()
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1")
      target_link_libraries(keymapper stdc++fs)
    endif()
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
      target_link_libraries(keymapper c++fs)
    endif()
  endif()

  target_link_libraries(keymapperd usb-1.0 udev)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  string(REPLACE "." "," FILE_VERSION "${VERSION}")
  string(REGEX REPLACE "-.*" "" FILE_VERSION "${FILE_VERSION}")
  configure_file("src/common/windows/resource.rc.in" "${CMAKE_SOURCE_DIR}/src/common/windows/_resource.rc")

  target_link_libraries(keymapper wtsapi32.lib ws2_32.lib)
  target_link_libraries(keymapperd ws2_32.lib cfgmgr32.lib hid.lib)
  target_link_libraries(keymapperctl ws2_32.lib)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  option(ENABLE_CARBON "Enable Carbon context update support" TRUE)
  if(ENABLE_CARBON)
    target_compile_definitions(keymapper PRIVATE ENABLE_CARBON)
    target_link_libraries(keymapper "-framework Carbon")
  endif()

  option(ENABLE_COCOA "Enable Cocoa tray icon support" TRUE)
  if(ENABLE_COCOA)
    target_compile_definitions(keymapper PRIVATE ENABLE_COCOA)
    target_link_libraries(keymapper "-framework Cocoa")
  endif()

  include(FetchContent)
  FetchContent_Declare(asio
    GIT_REPOSITORY "https://github.com/chriskohlhoff/asio"
    GIT_TAG        1f53428 # 1.31.0
  )
  FetchContent_MakeAvailable(asio)
  include_directories(${asio_SOURCE_DIR}/asio/include)

  include_directories(src/libs/Karabiner-DriverKit-VirtualHIDDevice/include)
  target_link_libraries(keymapperd "-framework CoreFoundation" "-framework IOKit")
endif()

option(ENABLE_TEST "Enable tests")
if(ENABLE_TEST)
  set(SOURCES_TEST
    src/test/catch.hpp
    src/test/test.cpp
    src/test/test.h
    src/test/test0_ParseKeySequence.cpp
    src/test/test1_ParseConfig.cpp
    src/test/test2_MatchKeySequence.cpp
    src/test/test3_Stage.cpp
    src/test/test4_Server.cpp
    src/test/test5_Fuzz.cpp
    src/server/ServerState.cpp
  )

  if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(SOURCES_TEST ${SOURCES_TEST} 
      src/client/windows/StringTyper.cpp
      src/common/windows/win.cpp)
  else()
    set(SOURCES_TEST ${SOURCES_TEST}
      src/client/unix/StringTyperImpl.cpp
      src/client/unix/StringTyperGeneric.cpp)
  endif()

  add_executable(test-keymapper ${SOURCES_CONFIG} ${SOURCES_RUNTIME} ${SOURCES_TEST})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES
  ${SOURCES_RUNTIME} ${SOURCES_CONFIG} ${SOURCES_CLIENT} ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_TEST})

# install
set(TARGETS keymapper keymapperd keymapperctl)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  SET(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

  install(TARGETS ${TARGETS} DESTINATION bin COMPONENT Application)
  install(DIRECTORY extra/share/ DESTINATION share COMPONENT Application)
  install(FILES extra/icon.svg DESTINATION share/icons/hicolor/scalable/apps
          RENAME "io.github.houmain.keymapper.svg" COMPONENT Application)
  install(DIRECTORY extra/lib/ DESTINATION lib COMPONENT Application)
  install(DIRECTORY extra/xdg DESTINATION ../etc COMPONENT Application)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  install(TARGETS ${TARGETS} DESTINATION "bin" COMPONENT Application)
  install(FILES extra/keymapper-launchd DESTINATION "bin" COMPONENT Application)
else()
  install(TARGETS ${TARGETS} DESTINATION . COMPONENT Application)
endif()

# package
set(CPACK_PACKAGE_NAME "keymapper")
set(CPACK_PACKAGE_DESCRIPTION "A cross-platform context-aware key remapper")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "keymapper")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "keymapper")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/keymapper")
set(CPACK_PACKAGE_CONTACT ${CPACK_PACKAGE_HOMEPAGE_URL})
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/src/common/icon.ico")

set(CPACK_STRIP_FILES TRUE)
if(VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+")
  string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${VERSION}")
  list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
  list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
  list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CPACK_GENERATOR WIX)
  set(CPACK_WIX_EXTENSIONS WixUtilExtension)
  set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/extra/wix-patch-file.xml")
  set(CPACK_WIX_UPGRADE_GUID "D3B4DADB-1A7E-40B3-A6F0-EEB022317FC8")

  set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/src/common/icon.ico")
  set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/extra/wix-dialog.png")
  set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/extra/wix-banner.png")

  file(READ  "${CMAKE_SOURCE_DIR}/LICENSE" LICENSE_RTF)
  string(REPLACE "\n" "\\par" LICENSE_RTF "${LICENSE_RTF}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf" "{\\rtf1\\ansi\\deff0\\nouicompat{\\fonttbl{\\f0\\fnil\\fcharset0 Sans Serif;}}\\fs18 ${LICENSE_RTF} }")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/LICENSE.rtf")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CPACK_DEBIAN_PACKAGE_SECTION utils)
  set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
  set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
endif()

include(CPack)
