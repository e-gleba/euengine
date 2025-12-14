add_library(engine_interface INTERFACE)

target_link_libraries(engine_interface INTERFACE glm)

target_include_directories(
    engine_interface
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(
    engine_interface
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

add_subdirectory(audio)
add_subdirectory(render)
add_subdirectory(game_module)
add_subdirectory(overlay)
add_subdirectory(detail)
