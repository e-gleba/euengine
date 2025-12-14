# Add texture, shader, and model subdirectories
add_subdirectory(texture)
add_subdirectory(shader)
add_subdirectory(model)

# Only include .cpp files in this directory, not subdirectories
file(GLOB src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(render_system STATIC ${src})

target_include_directories(render_system PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    render_system
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    render_system
    PRIVATE
        engine_interface
        egen::texture
        egen::shader
        egen::model
        warnings
        glm
        spdlog
        SDL3::SDL3-static
)

add_library(egen::render ALIAS render_system)
