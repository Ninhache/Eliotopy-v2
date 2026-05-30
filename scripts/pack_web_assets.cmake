file(READ "${SRC_DIR}/src/overlay/assets/index.html" HTML_CONTENT)
file(READ "${SRC_DIR}/src/overlay/assets/style.css" CSS_CONTENT)
file(READ "${SRC_DIR}/src/overlay/assets/i18n.js" I18N_CONTENT)
file(READ "${SRC_DIR}/src/overlay/assets/app.js" JS_CONTENT)

file(READ "${SRC_DIR}/src/overlay/assets/tabs/settings.html" TAB_SETTINGS)
file(READ "${SRC_DIR}/src/overlay/assets/tabs/overlay.html" TAB_OVERLAY)
file(READ "${SRC_DIR}/src/overlay/assets/tabs/debug.html" TAB_DEBUG)

string(REPLACE "<!-- INJECT_SETTINGS -->" "${TAB_SETTINGS}" HTML_CONTENT "${HTML_CONTENT}")
string(REPLACE "<!-- INJECT_OVERLAY -->" "${TAB_OVERLAY}" HTML_CONTENT "${HTML_CONTENT}")
string(REPLACE "<!-- INJECT_DEBUG -->" "${TAB_DEBUG}" HTML_CONTENT "${HTML_CONTENT}")

string(REPLACE "<link rel=\"stylesheet\" href=\"style.css\">" "<style>\n${CSS_CONTENT}\n</style>" HTML_CONTENT "${HTML_CONTENT}")
string(REPLACE "<script src=\"i18n.js\"></script>" "<script>\n${I18N_CONTENT}\n</script>" HTML_CONTENT "${HTML_CONTENT}")
string(REPLACE "<script src=\"app.js\"></script>" "<script>\n${JS_CONTENT}\n</script>" HTML_CONTENT "${HTML_CONTENT}")

file(WRITE "${BIN_DIR}/web_temp.html" "${HTML_CONTENT}")
file(READ "${BIN_DIR}/web_temp.html" HEX_CONTENT HEX)
file(REMOVE "${BIN_DIR}/web_temp.html")

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " ARRAY_CONTENT "${HEX_CONTENT}")

set(HEADER_CONTENT "#pragma once\n\n")
string(APPEND HEADER_CONTENT "namespace webview {\n")
string(APPEND HEADER_CONTENT "    constexpr unsigned char web_html_data[] = { ${ARRAY_CONTENT} 0x00 };\n")
string(APPEND HEADER_CONTENT "}\n")

file(WRITE "${BIN_DIR}/WebAssets.h" "${HEADER_CONTENT}")
