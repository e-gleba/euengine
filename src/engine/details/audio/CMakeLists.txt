file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(audio_manager STATIC ${src})

target_include_directories(audio_manager PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    audio_manager
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    audio_manager
    PRIVATE engine_interface warnings spdlog SDL3_mixer::SDL3_mixer-static
)

add_library(euengine::audio ALIAS audio_manager)
