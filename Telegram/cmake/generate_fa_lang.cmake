# This file is part of FAgram Desktop,
# the unofficial Telegram client based on tgd.

# For license and copyright information please follow this link:
# https://github.com/fagramdesktop/fadesktop/blob/dev/LEGAL

function(generate_fa_lang target_name lang_file)
    set(gen_dst ${CMAKE_CURRENT_BINARY_DIR}/gen)
    file(MAKE_DIRECTORY ${gen_dst})

    set(gen_timestamp ${gen_dst}/fa_lang_auto.timestamp)
    set(gen_files
        ${gen_dst}/fa_lang_auto.cpp
        ${gen_dst}/fa_lang_auto.h
    )

    add_custom_command(
    OUTPUT
        ${gen_timestamp}
    BYPRODUCTS
        ${gen_files}
    COMMAND
        codegen_fa_lang
        -o${gen_dst}
        ${lang_file}
    COMMENT "Generating fa_lang (${target_name})"
    DEPENDS
        codegen_fa_lang
        ${lang_file}
    )
    generate_target(${target_name} fa_lang ${gen_timestamp} "${gen_files}" ${gen_dst})
endfunction()
