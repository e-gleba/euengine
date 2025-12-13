file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(texture_manager STATIC ${src})

target_include_directories(texture_manager PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    texture_manager
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    texture_manager
    PRIVATE warnings spdlog SDL3::SDL3-static stb::stb
)

target_include_directories(
    texture_manager
    PRIVATE ${CMAKE_SOURCE_DIR}/src/engine
)

add_library(euengine::texture ALIAS texture_manager)
