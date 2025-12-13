# Only include specific files, exclude registry
file(
    GLOB src
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/gltf_loader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/model_system.cpp
)

add_library(model_system STATIC ${src})

target_include_directories(model_system PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(model_system PRIVATE ${CMAKE_SOURCE_DIR}/src/engine)

set_target_properties(
    model_system
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(
    model_system
    PRIVATE warnings glm spdlog fastgltf engine_interface
)

add_library(egen::model ALIAS model_system)
