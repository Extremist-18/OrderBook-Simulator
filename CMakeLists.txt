cmake_minimum_required(VERSION 3.15)
project(OrderBook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/implot)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(FetchContent)

FetchContent_Declare(
    uWebSockets
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG v20.67.0
    GIT_SUBMODULES "uSockets"
    GIT_SUBMODULES_RECURSE FALSE
)

FetchContent_MakeAvailable(uWebSockets)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
# find_package(libuv REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(orderbook_gui
    src/main.cpp
    src/core/OrderBook.cpp
    src/simulation/Simulator.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)

target_include_directories(orderbook_gui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(orderbook_gui
    ${OPENGL_gl_LIBRARY}
    glfw
)

if(APPLE)
    target_link_libraries(orderbook_gui
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

set(USOCKETS_DIR ${CMAKE_BINARY_DIR}/_deps/uwebsockets-src/uSockets)
add_custom_command(
    OUTPUT ${USOCKETS_DIR}/uSockets.a
    COMMAND make
    WORKING_DIRECTORY ${USOCKETS_DIR}
)
add_custom_target(build_usockets DEPENDS ${USOCKETS_DIR}/uSockets.a)

add_executable(orderbook_server
    src/ServerMain.cpp
    src/server/Server.cpp
    src/core/OrderBook.cpp
    src/simulation/Simulator.cpp
)
add_dependencies(orderbook_server build_usockets)

target_include_directories(orderbook_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/_deps/uwebsockets-src/src
    ${USOCKETS_DIR}/src
)

target_link_libraries(orderbook_server PRIVATE
    nlohmann_json::nlohmann_json
    ${USOCKETS_DIR}/uSockets.a
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    ${LIBUV_LIBRARIES}
)