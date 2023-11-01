cmake_minimum_required(VERSION 3.12.0)
project(keymapper LANGUAGES C CXX)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

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
)

set(SOURCES_CLIENT
  src/client/ConfigFile.cpp
  src/client/ConfigFile.h
  src/client/FocusedWindow.h
  src/client/Settings.cpp
  src/client/Settings.h
  src/client/ServerPort.cpp
  src/client/ServerPort.h
)

set(SOURCES_SERVER
  src/server/ButtonDebouncer.cpp
  src/server/ButtonDebouncer.h
  src/server/ClientPort.cpp
  src/server/ClientPort.h
  src/server/Settings.cpp
  src/server/Settings.h
  src/server/verbose_debug_io.h
)

set(SOURCES_COMMON
  src/common/Connection.cpp
  src/common/Connection.h
  src/common/Duration.h
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
    src/client/unix/StringTyperImpl.cpp
    src/client/unix/StringTyperImpl.h
    src/client/unix/StringTyperGeneric.cpp
    src/client/unix/StringTyperX11.cpp
    src/client/unix/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/unix/GrabbedDevicesLinux.cpp
    src/server/unix/GrabbedDevices.h
    src/server/unix/main.cpp
    src/server/unix/VirtualDeviceLinux.cpp
    src/server/unix/VirtualDevice.h
  )
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/windows/FocusedWindow.cpp
    src/client/windows/StringTyper.cpp
    src/client/windows/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/windows/main.cpp
  )
  set(SOURCES_COMMON ${SOURCES_COMMON}
    src/common/windows/_resource.rc
    src/common/windows/LimitSingleInstance.h
    src/common/windows/win.h
  )
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/unix/FocusedWindowImpl.cpp
    src/client/unix/FocusedWindowImpl.h
    src/client/unix/FocusedWindowCarbon.cpp
    src/client/unix/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/unix/GrabbedDevicesMacOS.cpp
    src/server/unix/GrabbedDevices.h
    src/server/unix/main.cpp
    src/server/unix/VirtualDeviceMacOS.cpp
    src/server/unix/VirtualDevice.h
  )
endif()

add_executable(keymapper WIN32 ${SOURCES_CLIENT} ${SOURCES_COMMON} ${SOURCES_CONFIG})

add_executable(keymapperd WIN32 ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_RUNTIME})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  option(ENABLE_X11 "Enable X11 context awareness" TRUE)
  if(ENABLE_X11)
    target_compile_definitions(keymapper PRIVATE ENABLE_X11)
    target_link_libraries(keymapper X11)
  endif()

  option(ENABLE_DBUS "Enable D-Bus context update support" TRUE)
  if(ENABLE_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    include_directories(${DBUS_INCLUDE_DIRS})
    link_directories(${DBUS_LIBRARY_DIRS})
    target_compile_definitions(keymapper PRIVATE ENABLE_DBUS)
    target_link_libraries(keymapper ${DBUS_LIBRARIES})
  endif()

  option(ENABLE_WLROOTS "Enable wlroots Wayland context update support" TRUE)
  if(ENABLE_WLROOTS)
    # source: https://github.com/foxcpp/ssr-wlroots/commit/e7144c94ed724c0d56bdde907b99e7c550c7ccaf
    include(WaylandScanner)
    wayland_generate_proto(${CMAKE_SOURCE_DIR}/src/client/unix/wlr-foreign-toplevel-management-unstable-v1.xml)
    wayland_proto_library(wayland-proto wlr-foreign-toplevel-management-unstable-v1)
    target_compile_definitions(keymapper PRIVATE ENABLE_WLROOTS)
    target_link_libraries(keymapper wayland-proto)
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

  if (MSVC)
    add_compile_options(/GR- /GL /Os /W1)
    add_link_options(/LTCG)
    add_compile_options($<$<CONFIG:RELEASE>:/MT>)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
  endif()

  target_link_libraries(keymapper wtsapi32.lib ws2_32.lib)
  target_link_libraries(keymapperd ws2_32.lib)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  option(ENABLE_CARBON "Enable Carbon context update support" TRUE)
  if(ENABLE_CARBON)
    target_compile_definitions(keymapper PRIVATE ENABLE_CARBON)
    target_link_libraries(keymapper "-framework Carbon")
  endif()

  include(FetchContent)
  FetchContent_Declare(asio GIT_REPOSITORY "https://github.com/chriskohlhoff/asio")
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
    src/test/test4_Fuzz.cpp
  )

  if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(SOURCES_TEST ${SOURCES_TEST} src/client/windows/StringTyper.cpp)
  else()
    set(SOURCES_TEST ${SOURCES_TEST}
      src/client/unix/StringTyperImpl.cpp
      src/client/unix/StringTyperGeneric.cpp)
  endif()

  add_executable(test-keymapper ${SOURCES_CONFIG} ${SOURCES_RUNTIME} ${SOURCES_TEST})
  target_compile_definitions(test-keymapper PRIVATE KEYMAPPER_TEST)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES
  ${SOURCES_RUNTIME} ${SOURCES_CONFIG} ${SOURCES_CLIENT} ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_TEST})

# install
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  install(TARGETS keymapper DESTINATION "bin" COMPONENT Application)
  install(TARGETS keymapperd DESTINATION "bin" COMPONENT Application)
  install(DIRECTORY extra/share DESTINATION . COMPONENT Application)
  install(DIRECTORY extra/lib DESTINATION . COMPONENT Application)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  install(TARGETS keymapper DESTINATION "bin" COMPONENT Application)
  install(TARGETS keymapperd DESTINATION "bin" COMPONENT Application)
  install(FILES extra/keymapper-launchd PERMISSIONS WORLD_EXECUTE OWNER_WRITE WORLD_READ DESTINATION "bin" COMPONENT Application)
else()
  install(TARGETS keymapper DESTINATION . COMPONENT Application)
  install(TARGETS keymapperd DESTINATION . COMPONENT Application)
endif()

# package
set(CPACK_PACKAGE_NAME "keymapper")
set(CPACK_PACKAGE_DESCRIPTION "A cross-platform context-aware key remapper")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "keymapper")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Keymapper")
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
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libudev1, libusb-1.0-0")
  set(CPACK_DEBIAN_PACKAGE_SECTION utils)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  set(CPACK_GENERATOR "DragNDrop")
  set(CPACK_DMG_VOLUME_NAME "${PROJECT_NAME}")
endif()

include(CPack)
