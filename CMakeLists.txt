cmake_minimum_required(VERSION 3.24)

project(sSheila VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

option(SSHEILA_BUILD_SERVER "Build the Drogon web server" ON)

add_library(ssheila_core
    src/core/local_subnet_access.cpp
    src/core/storage_layout.cpp
)

target_include_directories(ssheila_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(ssheila_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(ssheila_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(ssheila_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(ssheila_core PUBLIC iphlpapi ws2_32)
endif()

if(SSHEILA_BUILD_SERVER)
    set(SSHEILA_WEB_ASSETS
        ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/web/styles.css
        ${CMAKE_CURRENT_SOURCE_DIR}/web/app.js
        ${CMAKE_CURRENT_SOURCE_DIR}/web/manifest.webmanifest
    )
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SSHEILA_WEB_ASSETS})
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set(SSHEILA_EMBEDDED_WEB_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_web_assets.cpp)
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DOUTPUT_FILE=${SSHEILA_EMBEDDED_WEB_CPP}
            -DWEB_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/web
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateEmbeddedWebAssets.cmake
        COMMAND_ERROR_IS_FATAL ANY
    )

    find_package(Drogon CONFIG REQUIRED)

    find_package(unofficial-sqlite3 CONFIG QUIET)
    if(TARGET unofficial::sqlite3::sqlite3)
        set(SSHEILA_SQLITE_TARGET unofficial::sqlite3::sqlite3)
    elseif(TARGET SQLite::SQLite3)
        set(SSHEILA_SQLITE_TARGET SQLite::SQLite3)
    elseif(TARGET SQLite3_lib)
        # Some distribution Drogon packages create this target while resolving
        # their own SQLite dependency.
        set(SSHEILA_SQLITE_TARGET SQLite3_lib)
    else()
        find_package(SQLite3 REQUIRED)
        if(TARGET SQLite::SQLite3)
            set(SSHEILA_SQLITE_TARGET SQLite::SQLite3)
        elseif(TARGET SQLite3_lib)
            # Drogon ships this target name in its FindSQLite3 module.
            set(SSHEILA_SQLITE_TARGET SQLite3_lib)
        else()
            message(FATAL_ERROR "SQLite was found, but no supported CMake target was exported")
        endif()
    endif()

    add_executable(ssheila
        src/main.cpp
        src/core/database.cpp
        src/http/embedded_web_controller.cpp
        src/http/health_controller.cpp
        src/http/items_controller.cpp
        src/http/system_controller.cpp
        src/http/events_websocket.cpp
        ${SSHEILA_EMBEDDED_WEB_CPP}
    )

    target_include_directories(ssheila PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_compile_features(ssheila PRIVATE cxx_std_20)
    target_link_libraries(ssheila PRIVATE ssheila_core Drogon::Drogon ${SSHEILA_SQLITE_TARGET})

    if(MINGW)
        target_link_options(ssheila PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    if(MSVC)
        target_compile_options(ssheila PRIVATE /W4 /permissive-)
    else()
        target_compile_options(ssheila PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    install(TARGETS ssheila RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTING)
    add_executable(storage_layout_test tests/storage_layout_test.cpp)
    target_link_libraries(storage_layout_test PRIVATE ssheila_core)
    add_test(NAME storage_layout COMMAND storage_layout_test)

    add_executable(local_subnet_access_test tests/local_subnet_access_test.cpp)
    target_link_libraries(local_subnet_access_test PRIVATE ssheila_core)
    add_test(NAME local_subnet_access COMMAND local_subnet_access_test)
endif()
