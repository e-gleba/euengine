#pragma once

/// @file gltf_loader.hpp
/// @brief glTF 2.0 model loader (supports .gltf and .glb)

#include "model_loader.hpp"

namespace euengine
{

/// glTF 2.0 file format loader
class gltf_loader final : public IModelLoader
{
public:
    gltf_loader()           = default;
    ~gltf_loader() override = default;

    // Non-copyable, non-movable
    gltf_loader(const gltf_loader&)            = delete;
    gltf_loader& operator=(const gltf_loader&) = delete;
    gltf_loader(gltf_loader&&)                 = delete;
    gltf_loader& operator=(gltf_loader&&)      = delete;

    [[nodiscard]] load_result load(
        const std::filesystem::path& path) const override;

    [[nodiscard]] bool supports(std::string_view extension) const override;

    [[nodiscard]] std::span<const std::string_view> extensions() const override;

private:
    static constexpr std::array<std::string_view, 2> k_extensions { ".gltf",
                                                                    ".glb" };
};

} // namespace euengine