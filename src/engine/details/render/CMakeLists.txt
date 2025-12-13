# Add texture, shader, and model subdirectories
add_subdirectory(texture)
add_subdirectory(shader)
add_subdirectory(model)

# Only include .cpp files in this directory, not subdirectories
file(GLOB src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(renderer_manager STATIC ${src})

target_include_directories(renderer_manager PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    renderer_manager
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    renderer_manager
    PRIVATE
        engine_interface
        euengine::texture
        euengine::shader
        euengine::model
        warnings
        glm
        spdlog
        SDL3::SDL3-static
)

add_library(euengine::render ALIAS renderer_manager)
