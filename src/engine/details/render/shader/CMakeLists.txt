file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(shader_manager STATIC ${src})

target_include_directories(shader_manager PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    shader_manager
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    shader_manager
    PRIVATE
        warnings
        spdlog
        SDL3::SDL3-static
        SDL3_shadercross::SDL3_shadercross
        engine_interface
)

add_library(euengine::shader ALIAS shader_manager)
