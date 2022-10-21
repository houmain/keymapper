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
if(NOT VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+")
    message(WARNING "Could not get version from git repository (defaulting to VERSION=0.0.0).")
    set(VERSION "0.0.0")
endif()
configure_file("src/common/version.h.in" "${CMAKE_SOURCE_DIR}/src/common/_version.h")

set(CMAKE_CXX_STANDARD 17)
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
  src/config/string_iteration.h
)

set(SOURCES_RUNTIME
  src/runtime/Key.h
  src/runtime/KeyEvent.h
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

if(NOT WIN32)
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/linux/FocusedWindow.cpp
    src/client/linux/main.cpp
  )
  set(SOURCES_SERVER ${SOURCES_SERVER}
    src/server/linux/GrabbedDevices.cpp
    src/server/linux/GrabbedDevices.h
    src/server/linux/main.cpp
    src/server/linux/UinputDevice.cpp
    src/server/linux/UinputDevice.h
  )
else() # WIN32
  set(SOURCES_CLIENT ${SOURCES_CLIENT}
    src/client/windows/FocusedWindow.cpp
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
endif() # WIN32

add_executable(keymapper WIN32 ${SOURCES_CLIENT} ${SOURCES_COMMON} ${SOURCES_CONFIG})

add_executable(keymapperd WIN32 ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_RUNTIME})

if(NOT WIN32)
  option(ENABLE_X11 "Enable X11 context awareness" TRUE)
  if(ENABLE_X11)
    add_compile_definitions(ENABLE_X11)
    target_link_libraries(keymapper X11)
  endif()

  option(ENABLE_DBUS "Enable D-Bus context update support" TRUE)
  if(ENABLE_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DBUS REQUIRED dbus-1)
    include_directories(${DBUS_INCLUDE_DIRS})
    link_directories(${DBUS_LIBRARY_DIRS})
    add_compile_definitions(ENABLE_DBUS)
    target_link_libraries(keymapper ${DBUS_LIBRARIES})
  endif()

  option(ENABLE_WLROOTS "Enable wlroots Wayland context update support" TRUE)
  if(ENABLE_WLROOTS)
    # source: https://github.com/foxcpp/ssr-wlroots/commit/e7144c94ed724c0d56bdde907b99e7c550c7ccaf
    include(WaylandScanner)
    wayland_generate_proto(${CMAKE_SOURCE_DIR}/src/client/linux/wlr-foreign-toplevel-management-unstable-v1.xml)
    wayland_proto_library(wayland-proto wlr-foreign-toplevel-management-unstable-v1)
    target_link_libraries(keymapper wayland-proto)
    add_compile_definitions(ENABLE_WLROOTS)
  endif()

  target_link_libraries(keymapperd usb-1.0 udev)
else() # WIN32
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

  add_executable(test-keymapper ${SOURCES_CONFIG} ${SOURCES_RUNTIME} ${SOURCES_TEST})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES 
  ${SOURCES_RUNTIME} ${SOURCES_CONFIG} ${SOURCES_CLIENT} ${SOURCES_SERVER} ${SOURCES_COMMON} ${SOURCES_TEST})

# install
if(NOT WIN32)
  install(TARGETS keymapper DESTINATION "bin" COMPONENT Application)
  install(TARGETS keymapperd DESTINATION "bin" COMPONENT Application)
  install(DIRECTORY extra/share DESTINATION . COMPONENT Application)
  install(DIRECTORY extra/lib DESTINATION . COMPONENT Application)
else()
  install(TARGETS keymapper DESTINATION . COMPONENT Application)
  install(TARGETS keymapperd DESTINATION . COMPONENT Application)
endif()

# package
set(CPACK_PACKAGE_NAME "keymapper")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "keymapper")
set(CPACK_PACKAGE_VENDOR "Albert Kalchmair")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Keymapper")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/houmain/keymapper")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/src/common/icon.ico")
 
set(CPACK_STRIP_FILES TRUE)
string(REGEX MATCHALL "[0-9]+" VERSION_LIST "${VERSION}")
list(GET VERSION_LIST 0 CPACK_PACKAGE_VERSION_MAJOR)
list(GET VERSION_LIST 1 CPACK_PACKAGE_VERSION_MINOR)
list(GET VERSION_LIST 2 CPACK_PACKAGE_VERSION_PATCH)

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
endif()

include(CPack)
