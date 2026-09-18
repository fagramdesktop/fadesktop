
# This file is part of FAgram Desktop,
# the unofficial Telegram client based on Telegram Desktop.

# For license and copyright information please follow this link:
# https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL


cmake_minimum_required(VERSION 3.25...3.31)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/validate_special_target.cmake)
include(cmake/version.cmake)
desktop_app_parse_version(Telegram/build/version)

if (NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(configuration_types_init 1)
endif()

project(Telegram
    LANGUAGES C CXX
    VERSION ${desktop_app_version_cmake}
    DESCRIPTION "FAgram Desktop"
    HOMEPAGE_URL "https://t.me/FAgramDesktop"
)

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

if (configuration_types_init
    AND CMAKE_CONFIGURATION_TYPES
    AND NOT MinSizeRel IN_LIST CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES};MinSizeRel" CACHE STRING "" FORCE)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Telegram)

get_filename_component(third_party_loc "Telegram/ThirdParty" REALPATH)
get_filename_component(submodules_loc "Telegram" REALPATH)
get_filename_component(cmake_helpers_loc "cmake" REALPATH)

if (NOT DESKTOP_APP_USE_PACKAGED AND WIN32)
    set(Python3_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/python/Scripts/python)
endif()

include(cmake/variables.cmake)
include(cmake/nice_target_sources.cmake)
include(cmake/target_compile_options_if_exists.cmake)
include(cmake/target_link_frameworks.cmake)
include(cmake/target_link_options_if_exists.cmake)
include(cmake/init_target.cmake)
include(cmake/generate_target.cmake)
include(cmake/nuget.cmake)
include(cmake/validate_d3d_compiler.cmake)
include(cmake/target_prepare_qrc.cmake)

include(cmake/options.cmake)
include(cmake/external/qt/package.cmake)

# A prebuilt tlottie archive under extlibs/ is used by unpackaged builds on the
# platforms it can serve. These are the variables cmake/external/tlottie looks
# up, and a variable that is already set makes its find_library skip the search.
set(extlibs_loc "${CMAKE_CURRENT_SOURCE_DIR}/extlibs")
if (NOT DESKTOP_APP_USE_PACKAGED
    AND NOT WIN32
    AND NOT APPLE
    AND EXISTS "${extlibs_loc}/tlottie/libtlottie.a"
    AND EXISTS "${extlibs_loc}/tlottie/include/tlottie.h"
    AND NOT DEFINED DESKTOP_APP_TLOTTIE_LIBRARY)
    set(DESKTOP_APP_TLOTTIE_LIBRARY "${extlibs_loc}/tlottie/libtlottie.a")
    set(DESKTOP_APP_TLOTTIE_INCLUDE_DIR "${extlibs_loc}/tlottie/include")
endif()

set(desktop_app_skip_libs
    glibmm
    variant
)

add_subdirectory(cmake)
add_subdirectory(Telegram)
