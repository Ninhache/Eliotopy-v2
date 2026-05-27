cmake_minimum_required(VERSION 3.29)
project(Eliotopy)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_subdirectory(libs/glm)

set(WEBVIEW2_DIR ${CMAKE_SOURCE_DIR}/libs/webview2/Microsoft.Web.WebView2.1.0.3240.44)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/WebAssets.h
    COMMAND ${CMAKE_COMMAND} -D SRC_DIR=${CMAKE_SOURCE_DIR} -D BIN_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_SOURCE_DIR}/scripts/pack_web_assets.cmake
    DEPENDS 
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/index.html
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/style.css
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/app.js
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/tabs/settings.html
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/tabs/overlay.html
        ${CMAKE_SOURCE_DIR}/src/overlay/assets/tabs/debug.html
    COMMENT "Packaging web assets..."
)

add_custom_target(WebAssetsTarget DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/WebAssets.h)

add_executable(Eliotopy src/main.cpp ${CMAKE_CURRENT_BINARY_DIR}/WebAssets.h)
add_dependencies(Eliotopy WebAssetsTarget)

target_include_directories(Eliotopy PRIVATE
        libs/unity6
        libs/unity6/include
        libs
        
        src
        src/memory
        src/memory/unity
        src/types
        src/shared

        ${CMAKE_CURRENT_BINARY_DIR}
        ${WEBVIEW2_DIR}/build/native/include
)
target_link_libraries(Eliotopy
        user32 gdi32 glm::glm
        ${WEBVIEW2_DIR}/build/native/x64/WebView2LoaderStatic.lib
)
