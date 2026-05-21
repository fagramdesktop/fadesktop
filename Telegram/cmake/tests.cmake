# This file is part of FAgram Desktop,
# the unofficial Telegram client based on tgd.

# For license and copyright information please follow this link:
# https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL

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

add_executable(test_markdown_iv)
init_target(test_markdown_iv "(tests)")

target_precompile_headers(test_markdown_iv PRIVATE ${src_loc}/iv/iv_pch.h)

target_include_directories(test_markdown_iv PRIVATE
    ${src_loc}
    ${CMAKE_BINARY_DIR}/Telegram/gen
    ${CMAKE_BINARY_DIR}/Telegram/lib_ui/gen
)

nice_target_sources(test_markdown_iv ${src_loc}
PRIVATE
    tests/test_markdown_iv.cpp
    iv/markdown/iv_markdown_common.cpp
    iv/markdown/iv_markdown_common.h
    iv/markdown/iv_markdown_article.cpp
    iv/markdown/iv_markdown_article.h
    iv/markdown/iv_markdown_article_layout_blocks.cpp
    iv/markdown/iv_markdown_article_layout_blocks.h
    iv/markdown/iv_markdown_article_layout_structure.cpp
    iv/markdown/iv_markdown_article_layout_structure.h
    iv/markdown/iv_markdown_article_paint.cpp
    iv/markdown/iv_markdown_article_paint.h
    iv/markdown/iv_markdown_article_selection.cpp
    iv/markdown/iv_markdown_article_selection.h
    iv/markdown/iv_markdown_article_text.cpp
    iv/markdown/iv_markdown_article_text.h
    iv/markdown/iv_markdown_document.cpp
    iv/markdown/iv_markdown_document.h
    iv/markdown/iv_markdown_embed_overlay.cpp
    iv/markdown/iv_markdown_embed_overlay.h
    iv/markdown/iv_markdown_math.cpp
    iv/markdown/iv_markdown_math.h
    iv/markdown/iv_markdown_math_renderer.cpp
    iv/markdown/iv_markdown_math_renderer.h
    iv/markdown/iv_markdown_microtex.cpp
    iv/markdown/iv_markdown_microtex.h
    iv/markdown/iv_markdown_parse.cpp
    iv/markdown/iv_markdown_parse.h
    iv/markdown/iv_markdown_parse_convert.cpp
    iv/markdown/iv_markdown_parse_convert.h
    iv/markdown/iv_markdown_parse_finalize.cpp
    iv/markdown/iv_markdown_parse_finalize.h
    iv/markdown/iv_markdown_parse_validate.cpp
    iv/markdown/iv_markdown_parse_validate.h
    iv/markdown/iv_markdown_media_block.cpp
    iv/markdown/iv_markdown_media_block.h
    iv/markdown/iv_markdown_prepare.cpp
    iv/markdown/iv_markdown_prepare.h
    iv/markdown/iv_markdown_prepare_blocks.cpp
    iv/markdown/iv_markdown_prepare_blocks.h
    iv/markdown/iv_markdown_prepare_formulas.cpp
    iv/markdown/iv_markdown_prepare_formulas.h
    iv/markdown/iv_markdown_prepare_inline.cpp
    iv/markdown/iv_markdown_prepare_inline.h
    iv/markdown/iv_markdown_prepare_links.cpp
    iv/markdown/iv_markdown_prepare_links.h
    iv/markdown/iv_markdown_prepare_native_blocks.cpp
    iv/markdown/iv_markdown_prepare_native_blocks.h
    iv/markdown/iv_markdown_prepare_native_richtext.cpp
    iv/markdown/iv_markdown_prepare_native_richtext.h
    iv/markdown/iv_markdown_prepare_serialize.cpp
    iv/markdown/iv_markdown_prepare_serialize.h
    iv/markdown/iv_markdown_prepare_state.cpp
    iv/markdown/iv_markdown_prepare_state.h
    iv/markdown/iv_markdown_view.cpp
    iv/markdown/iv_markdown_view.h
    iv/markdown/iv_markdown_view_widget.cpp
    iv/markdown/iv_markdown_view_widget.h
)

target_sources(test_markdown_iv PRIVATE
    ${CMAKE_BINARY_DIR}/Telegram/gen/styles/style_chat_helpers.cpp
    ${CMAKE_BINARY_DIR}/Telegram/gen/styles/style_iv.cpp
    ${CMAKE_BINARY_DIR}/Telegram/lib_ui/gen/styles/palette.cpp
    ${CMAKE_BINARY_DIR}/Telegram/lib_ui/gen/styles/style_basic.cpp
    ${CMAKE_BINARY_DIR}/Telegram/lib_ui/gen/styles/style_widgets.cpp
)

target_link_libraries(test_markdown_iv
PRIVATE
    desktop-app::external_cmark_gfm
    desktop-app::external_microtex
    desktop-app::external_qt
    desktop-app::external_qt_static_plugins
    desktop-app::lib_base
    desktop-app::lib_crl
    desktop-app::lib_spellcheck
    desktop-app::lib_tl
    desktop-app::lib_ui
    desktop-app::lib_webview
    tdesktop::td_scheme
    tdesktop::td_ui
)

set_target_properties(
    test_markdown_iv
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_command(TARGET test_markdown_iv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${src_loc}/tests/fixtures/markdown_iv/markdown-example.md
        ${src_loc}/tests/fixtures/markdown_iv/latex-markdown-test.md
        $<TARGET_FILE_DIR:test_markdown_iv>
)

add_dependencies(Telegram test_markdown_iv)
add_dependencies(test_markdown_iv lib_ui_styles td_scheme_scheme td_ui_styles)
