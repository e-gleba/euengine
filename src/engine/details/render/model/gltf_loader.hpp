#pragma once

/// @file gltf_loader.hpp
/// @brief glTF 2.0 model loader (supports .gltf and .glb)

#include <core-api/model_loader.hpp>

#include <array>
#include <span>
#include <string_view>

namespace egen
{

/// glTF 2.0 file format loader (internal implementation)
class gltf_loader final
{
public:
    gltf_loader()  = default;
    ~gltf_loader() = default;

    // Non-copyable, non-movable
    gltf_loader(const gltf_loader&)            = delete;
    gltf_loader& operator=(const gltf_loader&) = delete;
    gltf_loader(gltf_loader&&)                 = delete;
    gltf_loader& operator=(gltf_loader&&)      = delete;

    [[nodiscard]] load_result load(const std::filesystem::path& path);

    [[nodiscard]] bool supports(std::string_view extension);

    [[nodiscard]] std::span<const std::string_view> extensions();

private:
    static constexpr std::array<std::string_view, 2> k_extensions { ".gltf",
                                                                    ".glb" };
};

} // namespace egen