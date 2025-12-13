# Add texture and shader subdirectories
add_subdirectory(texture)
add_subdirectory(shader)

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
        euengine::model
        euengine::texture
        euengine::shader
        warnings
        glm
        spdlog
        SDL3::SDL3-static
)

add_library(euengine::render ALIAS renderer_manager)
