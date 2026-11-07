# MIT License - see LICENSE
#
# Runs ndoc over the fixture and checks the generated pages.

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(COMMAND "${NDOC}" -o "${OUTPUT_DIR}/html" "${SOURCE_DIR}/ndoc.conf"
                RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ndoc failed: ${out}${err}")
endif()
# The only expected diagnostic is the missing comment on notDocumented().
string(REGEX REPLACE "[^\n]*notDocumented\\(\\) const' is not documented\n" "" unexpected "${err}")
if(NOT unexpected STREQUAL "")
    message(FATAL_ERROR "unexpected warnings:\n${unexpected}")
endif()
string(REGEX MATCH "widget\\.h:[0-9]+: warning: 'Widget::notDocumented\\(\\) const' is not documented" found "${err}")
if(found STREQUAL "")
    message(FATAL_ERROR "no missing-comment warning for notDocumented():\n${err}")
endif()

function(expect file text)
    file(READ "${OUTPUT_DIR}/html/${file}" content)
    string(FIND "${content}" "${text}" at)
    if(at EQUAL -1)
        message(SEND_ERROR "${file}: missing: ${text}")
    endif()
endfunction()

function(reject file text)
    file(READ "${OUTPUT_DIR}/html/${file}" content)
    string(FIND "${content}" "${text}" at)
    if(NOT at EQUAL -1)
        message(SEND_ERROR "${file}: must not contain: ${text}")
    endif()
endfunction()

# Comment placement: header-only, source file, \fn and \enum topics.
expect(widget.html "The Widget class is documented entirely in its header.")
expect(base.html "The Base class is documented from a source file.")
expect(base.html "Handles <i>code</i>.")
expect(fx.html "Selects a mode.")
expect(fx.html "<code>Fx::Fast</code>")

# Qt's page structure.
foreach(section "Public Types" "Public Functions" "Public Slots" "Signals"
                "Reimplemented Protected Functions" "Detailed Description"
                "Differences from Qt" "Member Type Documentation"
                "Member Function Documentation")
    expect(widget.html ">${section}</h2>")
endforeach()
expect(widget.html "[signal]")
expect(widget.html "[slot]")
expect(widget.html "[explicit]")
expect(widget.html "[override, protected]")
expect(widget.html "Reimplements <a href=\"base.html#handle\">Base::handle()</a>")
expect(widget.html "This is an overloaded function.")
expect(widget.html "template &lt;typename T&gt;")
expect(widget.html "Inherits</th><td><a href=\"base.html\">Base</a>")
expect(base.html "Inherited by</th><td><a href=\"widget.html\">Widget</a>")
expect(widget.html "target_link_libraries(app PRIVATE Fx::Core)")

# Signatures survive literals, operators and function pointers.
expect(widget.html "const char *<i>title</i> = &quot;a, b&quot;")
expect(widget.html "Widget::operator==(")
expect(widget.html "char32_t <i>fill</i> = U' ', int <i>limit</i> = 1'000")
expect(widget.html "Widget::afterLiterals()")
expect(widget.html "void (*<i>callback</i>)(int)")

# Links, snippets and markup.
expect(widget.html "href=\"guide.html\">the guide</a>")
expect(widget.html "w.show();")
reject(widget.html "int before")
expect(guide.html "id=\"first-section\"")
expect(guide.html "id=\"custom-anchor\"")
expect(guide.html "<img src=\"images/pic.svg\" alt=\"A picture\">")
if(NOT EXISTS "${OUTPUT_DIR}/html/images/pic.svg")
    message(SEND_ERROR "images/pic.svg was not copied")
endif()
expect(guide.html "<a href=\"widget.html\">Widget</a>")

# Hidden API and the site index.
reject(widget.html "m_hidden")
reject(widget.html "notDocumented")
reject(classes.html "Detail")
expect(index.html "Fixture Core Classes")
expect(index.html "Guide Page")

# The header links to the source, with the GitHub mark.
expect(index.html "<a class=\"source\" href=\"https://github.com/example/fixture\"")
expect(index.html "aria-label=\"Source code on GitHub\"")
expect(base.html "Font Awesome Free")
expect(widget-members.html "from <a href=\"base.html\">Base</a>")

# Coverage reports what is undocumented, and nothing that was excused.
file(READ "${OUTPUT_DIR}/html/coverage.txt" coverage)
string(FIND "${coverage}" "notDocumented() const" at)
if(at EQUAL -1)
    message(SEND_ERROR "coverage misses notDocumented()")
endif()
string(FIND "${coverage}" "hidden" at)
if(NOT at EQUAL -1)
    message(SEND_ERROR "coverage reports an internal class")
endif()
execute_process(COMMAND "${NDOC}" --require-complete -o "${OUTPUT_DIR}/html" "${SOURCE_DIR}/ndoc.conf"
                RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result EQUAL 0)
    message(SEND_ERROR "--require-complete passed with undocumented API")
endif()

# Missing comments warn but do not fail --strict.
execute_process(COMMAND "${NDOC}" --strict -o "${OUTPUT_DIR}/html" "${SOURCE_DIR}/ndoc.conf"
                RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(NOT result EQUAL 0)
    message(SEND_ERROR "--strict failed on a missing comment alone")
endif()

# Strict mode fails on broken links and bad parameter names.
file(MAKE_DIRECTORY "${OUTPUT_DIR}/broken")
configure_file("${SOURCE_DIR}/src/broken.h.in" "${OUTPUT_DIR}/broken/src/broken.h" COPYONLY)
file(WRITE "${OUTPUT_DIR}/broken/ndoc.conf" "project = Broken\nsources = src\nmodule B = src\n")
execute_process(COMMAND "${NDOC}" --strict -o "${OUTPUT_DIR}/broken/html" "${OUTPUT_DIR}/broken/ndoc.conf"
                RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE err)
if(result EQUAL 0)
    message(SEND_ERROR "--strict passed with warnings")
endif()
foreach(text "can't link to 'No Such Page'" "unknown parameter 'wrong'"
             "undocumented parameter 'count'")
    string(FIND "${err}" "${text}" at)
    if(at EQUAL -1)
        message(SEND_ERROR "missing diagnostic: ${text}\n${err}")
    endif()
endforeach()
