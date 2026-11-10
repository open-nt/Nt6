# MIT License - see LICENSE
#
# Embeds a file in a program as a byte array, in place of a Qt resource (.qrc) file.
#
#   include(embed.cmake)
#   nt_example_embed(myapp banner_txt data/banner.txt)
#
# compiles data/banner.txt into `myapp` as
#
#   extern const unsigned char banner_txt[];     // the bytes, followed by a 0
#   extern const std::size_t banner_txt_size;    // the number of bytes, without the 0
#
# The array is regenerated when the file changes. The same file runs as a script
# (cmake -P embed.cmake) to write the generated source.

if(CMAKE_SCRIPT_MODE_FILE)
    # Script mode: INPUT, OUTPUT and SYMBOL are passed with -D.
    file(READ "${INPUT}" hex HEX)
    string(LENGTH "${hex}" digits)
    math(EXPR size "${digits} / 2")
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    file(WRITE "${OUTPUT}"
        "// Generated from ${INPUT} by embed.cmake; do not edit.\n"
        "#include <cstddef>\n"
        "extern const unsigned char ${SYMBOL}[] = {${bytes}0x00};\n"
        "extern const std::size_t ${SYMBOL}_size = ${size};\n")
    return()
endif()

set(NT_EXAMPLE_EMBED_SCRIPT "${CMAKE_CURRENT_LIST_FILE}")

function(nt_example_embed target symbol file)
    get_filename_component(input "${file}" ABSOLUTE)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/embedded_${symbol}.cpp")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -DINPUT=${input} -DOUTPUT=${output} -DSYMBOL=${symbol}
                -P "${NT_EXAMPLE_EMBED_SCRIPT}"
        DEPENDS "${input}" "${NT_EXAMPLE_EMBED_SCRIPT}"
        COMMENT "Embedding ${file} as ${symbol}"
        VERBATIM)
    target_sources(${target} PRIVATE "${output}")
endfunction()
