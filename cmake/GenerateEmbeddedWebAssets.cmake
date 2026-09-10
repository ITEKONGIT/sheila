if(NOT DEFINED OUTPUT_FILE OR NOT DEFINED WEB_ROOT)
    message(FATAL_ERROR "OUTPUT_FILE and WEB_ROOT are required")
endif()

set(ASSETS
    "index.html|text/html"
    "styles.css|text/css"
    "app.js|application/javascript"
    "manifest.webmanifest|application/manifest+json"
)

file(WRITE "${OUTPUT_FILE}"
    "#include \"ssheila/http/embedded_web_assets.hpp\"\n\n"
    "namespace ssheila::http {\nnamespace {\n")

foreach(ENTRY IN LISTS ASSETS)
    string(REPLACE "|" ";" PARTS "${ENTRY}")
    list(GET PARTS 0 FILE_NAME)
    list(GET PARTS 1 CONTENT_TYPE)
    string(MAKE_C_IDENTIFIER "${FILE_NAME}" IDENTIFIER)
    file(READ "${WEB_ROOT}/${FILE_NAME}" HEX_CONTENT HEX)
    string(REGEX REPLACE "([0-9A-Fa-f][0-9A-Fa-f])" "0x\\1," BYTE_LIST "${HEX_CONTENT}")
    file(APPEND "${OUTPUT_FILE}"
        "constexpr unsigned char ${IDENTIFIER}[] = {${BYTE_LIST}};\n"
        "constexpr std::string_view ${IDENTIFIER}_type = \"${CONTENT_TYPE}\";\n")
endforeach()

file(APPEND "${OUTPUT_FILE}" "}\n\n"
    "std::optional<EmbeddedWebAsset> find_embedded_web_asset(std::string_view path) {\n"
    "    if (path == \"/\" || path == \"/index.html\") return EmbeddedWebAsset{{reinterpret_cast<const char*>(index_html), sizeof(index_html)}, index_html_type};\n"
    "    if (path == \"/styles.css\") return EmbeddedWebAsset{{reinterpret_cast<const char*>(styles_css), sizeof(styles_css)}, styles_css_type};\n"
    "    if (path == \"/app.js\") return EmbeddedWebAsset{{reinterpret_cast<const char*>(app_js), sizeof(app_js)}, app_js_type};\n"
    "    if (path == \"/manifest.webmanifest\") return EmbeddedWebAsset{{reinterpret_cast<const char*>(manifest_webmanifest), sizeof(manifest_webmanifest)}, manifest_webmanifest_type};\n"
    "    return std::nullopt;\n}\n\n}  // namespace ssheila::http\n")
