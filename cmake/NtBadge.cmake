# MIT License - see LICENSE
#
# Writes a version badge for the README, published with the documentation.
#
#   cmake -DVERSION_FILE=VERSION -DOUTPUT_DIR=<dir> [-DLABEL=version] [-DCOLOR=13977f] -P NtBadge.cmake
#
# Writes <dir>/version.svg, and <dir>/version.json for shields.io endpoint badges.

if(NOT VERSION_FILE OR NOT OUTPUT_DIR)
    message(FATAL_ERROR "NtBadge.cmake needs VERSION_FILE and OUTPUT_DIR")
endif()
if(NOT LABEL)
    set(LABEL "version")
endif()
if(NOT COLOR)
    set(COLOR "13977f")
endif()

file(STRINGS "${VERSION_FILE}" message LIMIT_COUNT 1)
string(STRIP "${message}" message)
if(NOT message MATCHES "^[0-9A-Za-z.+-]+$")
    message(FATAL_ERROR "${VERSION_FILE}: not a version: '${message}'")
endif()

# Text is 11px Verdana; 7px per character is a close upper bound, so nothing is clipped.
string(LENGTH "${LABEL}" labelLength)
string(LENGTH "${message}" messageLength)
math(EXPR labelWidth "${labelLength} * 7 + 12")
math(EXPR messageWidth "${messageLength} * 7 + 12")
math(EXPR width "${labelWidth} + ${messageWidth}")
math(EXPR labelCenter "${labelWidth} * 5")
math(EXPR messageCenter "(${labelWidth} + ${messageWidth} / 2) * 10")
math(EXPR labelTextLength "${labelLength} * 70")
math(EXPR messageTextLength "${messageLength} * 70")

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(WRITE "${OUTPUT_DIR}/version.svg"
"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"${width}\" height=\"20\" role=\"img\" aria-label=\"${LABEL}: ${message}\">
<title>${LABEL}: ${message}</title>
<linearGradient id=\"s\" x2=\"0\" y2=\"100%\"><stop offset=\"0\" stop-color=\"#bbb\" stop-opacity=\".1\"/><stop offset=\"1\" stop-opacity=\".1\"/></linearGradient>
<clipPath id=\"r\"><rect width=\"${width}\" height=\"20\" rx=\"3\" fill=\"#fff\"/></clipPath>
<g clip-path=\"url(#r)\">
<rect width=\"${labelWidth}\" height=\"20\" fill=\"#555\"/>
<rect x=\"${labelWidth}\" width=\"${messageWidth}\" height=\"20\" fill=\"#${COLOR}\"/>
<rect width=\"${width}\" height=\"20\" fill=\"url(#s)\"/>
</g>
<g fill=\"#fff\" text-anchor=\"middle\" font-family=\"Verdana,Geneva,DejaVu Sans,sans-serif\" text-rendering=\"geometricPrecision\" font-size=\"110\">
<text aria-hidden=\"true\" x=\"${labelCenter}\" y=\"150\" fill=\"#010101\" fill-opacity=\".3\" transform=\"scale(.1)\" textLength=\"${labelTextLength}\">${LABEL}</text>
<text x=\"${labelCenter}\" y=\"140\" transform=\"scale(.1)\" textLength=\"${labelTextLength}\">${LABEL}</text>
<text aria-hidden=\"true\" x=\"${messageCenter}\" y=\"150\" fill=\"#010101\" fill-opacity=\".3\" transform=\"scale(.1)\" textLength=\"${messageTextLength}\">${message}</text>
<text x=\"${messageCenter}\" y=\"140\" transform=\"scale(.1)\" textLength=\"${messageTextLength}\">${message}</text>
</g>
</svg>
")
file(WRITE "${OUTPUT_DIR}/version.json"
"{\"schemaVersion\":1,\"label\":\"${LABEL}\",\"message\":\"${message}\",\"color\":\"${COLOR}\"}
")
