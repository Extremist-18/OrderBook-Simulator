cmake_minimum_required(VERSION 3.15)
project(OrderBook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui)
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/implot)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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