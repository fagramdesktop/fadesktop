# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

if (DESKTOP_APP_TEST_APPS)
add_executable(test_text WIN32)
init_target(test_text "(tests)")

target_include_directories(test_text PRIVATE ${src_loc})

nice_target_sources(test_text ${src_loc}
PRIVATE
    tests/test_main.cpp
    tests/test_main.h
    tests/test_text.cpp
)

nice_target_sources(test_text ${res_loc}
PRIVATE
    qrc/emoji_1.qrc
    qrc/emoji_2.qrc
    qrc/emoji_3.qrc
    qrc/emoji_4.qrc
    qrc/emoji_5.qrc
    qrc/emoji_6.qrc
    qrc/emoji_7.qrc
    qrc/emoji_8.qrc
)

target_link_libraries(test_text
PRIVATE
    desktop-app::lib_base
    desktop-app::lib_crl
    desktop-app::lib_ui
    desktop-app::external_qt
    desktop-app::external_qt_static_plugins
)

set_target_properties(test_text PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_dependencies(Telegram test_text)

target_prepare_qrc(test_text)
endif()

if (WIN32)
    add_executable(test_uninstall EXCLUDE_FROM_ALL)
    init_target(test_uninstall "(tests)")

    target_include_directories(test_uninstall PRIVATE ${src_loc})

    nice_target_sources(test_uninstall ${src_loc}
    PRIVATE
        core/uninstall.cpp
        core/uninstall.h
        platform/win/uninstall_win.cpp
        platform/win/uninstall_win.h
        tests/test_uninstall.cpp
    )

    target_link_libraries(test_uninstall
    PRIVATE
        desktop-app::lib_base
        desktop-app::lib_crl
        desktop-app::external_qt
    )

    set_target_properties(test_uninstall PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
