file(GLOB_RECURSE game_src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
add_library(game SHARED ${game_src})

target_include_directories(game PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    game
    PROPERTIES
        CXX_STANDARD 26
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        LIBRARY_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
        RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:engine>
)

target_link_libraries(
    game
    PRIVATE warnings glm EnTT::EnTT spdlog implot imgui engine_interface
)
