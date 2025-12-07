file(GLOB_RECURSE src CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

add_library(model_loaders STATIC ${src})

target_include_directories(model_loaders PUBLIC ${CMAKE_CURRENT_LIST_DIR})

set_target_properties(
    model_loaders
    PROPERTIES CXX_STANDARD 26 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

target_link_libraries(model_loaders PRIVATE warnings glm spdlog fastgltf)

add_library(euengine::model ALIAS model_loaders)