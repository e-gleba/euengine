file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(overlay_layer STATIC ${src})

target_include_directories(overlay_layer PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(overlay_layer PRIVATE ${CMAKE_SOURCE_DIR}/src/engine)

set_target_properties(
    overlay_layer
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    overlay_layer
    PRIVATE warnings spdlog SDL3::SDL3-static engine_interface imgui_sdl3
)

add_library(egen::overlay ALIAS overlay_layer)
