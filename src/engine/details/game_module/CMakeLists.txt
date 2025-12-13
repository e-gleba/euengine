file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(game_module_manager STATIC ${src})

target_include_directories(game_module_manager PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    game_module_manager
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    game_module_manager
    PRIVATE engine_interface warnings spdlog EnTT::EnTT SDL3::SDL3-static
)

add_library(euengine::game_module ALIAS game_module_manager)
