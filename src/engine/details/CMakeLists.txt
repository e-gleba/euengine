file(GLOB engine_src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
add_executable(engine ${engine_src})

target_include_directories(engine PRIVATE ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    engine
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

add_subdirectory(model)
add_subdirectory(audio)
add_subdirectory(game_module)
target_link_libraries(
    engine
    PRIVATE
        engine_interface
        euengine::model
        euengine::audio
        euengine::game_module
        warnings
        glm
        EnTT::EnTT
        spdlog
        SDL3::SDL3-static
        SDL3_shadercross::SDL3_shadercross
        yaml-cpp
        stb
        imgui_sdl3
        Microsoft.GSL::GSL
)

add_custom_command(
    TARGET engine
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:engine>/shaders"
    COMMAND
        ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:engine>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:engine>/game"
    COMMAND
        ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_SOURCE_DIR}/src/game/scripts"
        "$<TARGET_FILE_DIR:engine>/game/scripts"
    COMMENT "Creating symlinks to shaders, assets, and game scripts"
    VERBATIM
)
