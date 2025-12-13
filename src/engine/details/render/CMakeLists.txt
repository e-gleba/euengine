file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

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
        warnings
        glm
        spdlog
        SDL3::SDL3-static
)

add_library(euengine::renderer ALIAS renderer_manager)
