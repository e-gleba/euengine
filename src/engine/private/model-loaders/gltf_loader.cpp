#include "gltf_loader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <ranges>
#include <stack>
#include <unordered_map>

namespace euengine
{

namespace
{

/// Get local transform matrix from a glTF node
[[nodiscard]] glm::mat4 get_node_transform(const fastgltf::Node& node)
{
    // Use fastgltf's built-in helper
    auto transform_matrix = fastgltf::getTransformMatrix(node);

    // Convert from fastgltf::math::fmat4x4 to glm::mat4
    glm::mat4 result;
    std::memcpy(
        glm::value_ptr(result), transform_matrix.data(), sizeof(glm::mat4));
    return result;
}

/// Compute world transforms for all nodes
[[nodiscard]] std::unordered_map<std::size_t, glm::mat4>
compute_world_transforms(const fastgltf::Asset& asset)
{
    std::unordered_map<std::size_t, glm::mat4> world_transforms;

    // Find root nodes
    std::vector<bool> is_child(asset.nodes.size(), false);
    for (const auto& node : asset.nodes)
    {
        for (std::size_t child : node.children)
        {
            if (child < asset.nodes.size())
            {
                is_child[child] = true;
            }
        }
    }

    // Get root nodes from default scene or find orphans
    std::vector<std::size_t> roots;
    if (asset.defaultScene.has_value())
    {
        const auto& scene = asset.scenes[*asset.defaultScene];
        // Convert SmallVector to std::vector
        roots.assign(scene.nodeIndices.begin(), scene.nodeIndices.end());
    }
    else
    {
        for (std::size_t i = 0; i < asset.nodes.size(); ++i)
        {
            if (!is_child[i])
            {
                roots.push_back(i);
            }
        }
    }

    // DFS to compute world transforms
    std::stack<std::pair<std::size_t, glm::mat4>> stack;
    for (std::size_t root : roots)
    {
        stack.push({ root, glm::mat4 { 1.0f } });
    }

    while (!stack.empty())
    {
        auto [node_idx, parent_transform] = stack.top();
        stack.pop();

        if (node_idx >= asset.nodes.size())
        {
            continue;
        }

        const auto& node           = asset.nodes[node_idx];
        glm::mat4   local          = get_node_transform(node);
        glm::mat4   world          = parent_transform * local;
        world_transforms[node_idx] = world;

        for (std::size_t child : node.children)
        {
            stack.push({ child, world });
        }
    }

    return world_transforms;
}

/// Process a single primitive with transform applied
void process_primitive(const fastgltf::Asset&     asset,
                       const fastgltf::Primitive& primitive,
                       const glm::mat4&           transform,
                       loaded_mesh&               mesh,
                       aabb&                      bounds)
{
    // Find position attribute (required)
    auto pos_it = primitive.findAttribute("POSITION");
    if (pos_it == primitive.attributes.end())
    {
        return;
    }

    const auto&       pos_accessor = asset.accessors[pos_it->accessorIndex];
    const std::size_t vertex_count = pos_accessor.count;

    if (vertex_count == 0)
    {
        return;
    }

    // Normal matrix for transforming normals
    glm::mat3 normal_matrix =
        glm::transpose(glm::inverse(glm::mat3(transform)));

    const std::size_t base_vertex = mesh.vertices.size();
    mesh.vertices.resize(base_vertex + vertex_count);

    // Load positions
    fastgltf::iterateAccessorWithIndex<glm::vec3>(
        asset,
        pos_accessor,
        [&](glm::vec3 pos, std::size_t idx)
        {
            glm::vec4 transformed_pos = transform * glm::vec4(pos, 1.0f);
            mesh.vertices[base_vertex + idx].position =
                glm::vec3(transformed_pos);
            bounds.expand(mesh.vertices[base_vertex + idx].position);
        });

    // Load normals
    if (auto norm_it = primitive.findAttribute("NORMAL");
        norm_it != primitive.attributes.end())
    {
        const auto& norm_accessor = asset.accessors[norm_it->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset,
            norm_accessor,
            [&](glm::vec3 n, std::size_t idx)
            {
                mesh.vertices[base_vertex + idx].normal =
                    glm::normalize(normal_matrix * n);
            });
    }

    // Load texcoords
    if (auto uv_it = primitive.findAttribute("TEXCOORD_0");
        uv_it != primitive.attributes.end())
    {
        const auto& uv_accessor = asset.accessors[uv_it->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset,
            uv_accessor,
            [&](glm::vec2 uv, std::size_t idx)
            { mesh.vertices[base_vertex + idx].texcoord = uv; });
    }

    // Build indices
    const auto base_index = static_cast<uint16_t>(base_vertex);

    if (primitive.indicesAccessor.has_value())
    {
        const auto& idx_accessor = asset.accessors[*primitive.indicesAccessor];
        mesh.indices.reserve(mesh.indices.size() + idx_accessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(
            asset,
            idx_accessor,
            [&](std::uint32_t idx)
            {
                mesh.indices.push_back(static_cast<uint16_t>(base_index + idx));
            });
    }
    else
    {
        // No indices - generate sequential
        mesh.indices.reserve(mesh.indices.size() + vertex_count);
        for (std::size_t i = 0; i < vertex_count; ++i)
        {
            mesh.indices.push_back(static_cast<uint16_t>(base_index + i));
        }
    }
}

/// Find texture path - handles both external files and embedded textures
[[nodiscard]] std::filesystem::path find_texture(
    const fastgltf::Asset& asset, const std::filesystem::path& base_path)
{
    // First try to find texture from materials
    for (const auto& mat : asset.materials)
    {
        if (!mat.pbrData.baseColorTexture.has_value())
        {
            continue;
        }

        std::size_t tex_idx = mat.pbrData.baseColorTexture->textureIndex;
        if (tex_idx >= asset.textures.size())
        {
            continue;
        }

        const auto& tex = asset.textures[tex_idx];
        if (!tex.imageIndex.has_value() ||
            *tex.imageIndex >= asset.images.size())
        {
            continue;
        }

        const auto& img = asset.images[*tex.imageIndex];

        // External texture file
        if (std::holds_alternative<fastgltf::sources::URI>(img.data))
        {
            const auto& uri = std::get<fastgltf::sources::URI>(img.data);
            std::string uri_str(uri.uri.path().begin(), uri.uri.path().end());

            if (uri_str.find("data:") != 0)
            {
                auto tex_path = base_path.parent_path() / uri_str;
                if (std::filesystem::exists(tex_path))
                {
                    spdlog::info("  texture: {}", uri_str);
                    return tex_path;
                }

                // Try without path - just filename
                auto filename_only = std::filesystem::path(uri_str).filename();
                tex_path           = base_path.parent_path() / filename_only;
                if (std::filesystem::exists(tex_path))
                {
                    spdlog::info("  texture: {}", filename_only.string());
                    return tex_path;
                }
            }
        }

        // Embedded texture
        if (std::holds_alternative<fastgltf::sources::Array>(img.data) ||
            std::holds_alternative<fastgltf::sources::BufferView>(img.data))
        {
            spdlog::info("  embedded texture (not yet supported)");
            // TODO: handle embedded textures
        }
    }

    // Fallback: look for any image file next to the model
    std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".tga",
                                            ".PNG", ".JPG", ".TGA" };
    auto                     stem       = base_path.stem().string();

    for (const auto& ext : extensions)
    {
        auto tex_path = base_path.parent_path() / (stem + ext);
        if (std::filesystem::exists(tex_path))
        {
            spdlog::info("  texture (fallback): {}",
                         tex_path.filename().string());
            return tex_path;
        }
    }

    // Check for any texture in same directory
    if (std::filesystem::exists(base_path.parent_path()))
    {
        for (const auto& entry :
             std::filesystem::directory_iterator(base_path.parent_path()))
        {
            if (!entry.is_regular_file())
                continue;
            auto ext = entry.path().extension().string();
            std::ranges::transform(ext, ext.begin(), ::tolower);
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                ext == ".tga")
            {
                spdlog::info("  texture (dir scan): {}",
                             entry.path().filename().string());
                return entry.path();
            }
        }
    }

    return {};
}

} // namespace

load_result GltfLoader::load(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
    {
        return std::unexpected("file not found: " + path.string());
    }

    // Load file data
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
    {
        return std::unexpected("failed to read file: " + path.string());
    }

    // Configure parser options (LoadGLBBuffers is now default)
    constexpr auto options = fastgltf::Options::LoadExternalBuffers;

    fastgltf::Parser parser;

    const auto ext = path.extension().string();
    auto       result =
        (ext == ".glb" || ext == ".GLB")
                  ? parser.loadGltfBinary(data.get(), path.parent_path(), options)
                  : parser.loadGltfJson(data.get(), path.parent_path(), options);

    if (result.error() != fastgltf::Error::None)
    {
        return std::unexpected(std::format(
            "glTF parse error: {}", fastgltf::getErrorMessage(result.error())));
    }

    fastgltf::Asset asset = std::move(result.get());

    loaded_model model {};
    model.has_uvs = true;

    // Compute world transforms for all nodes
    auto world_transforms = compute_world_transforms(asset);

    // Single mesh to collect all geometry
    loaded_mesh combined {};
    combined.material_name = path.stem().string();

    int mesh_node_count = 0;

    // Process nodes that have meshes
    for (std::size_t node_idx = 0; node_idx < asset.nodes.size(); ++node_idx)
    {
        const auto& node = asset.nodes[node_idx];
        if (!node.meshIndex.has_value())
        {
            continue;
        }

        mesh_node_count++;

        // Get world transform for this node
        glm::mat4 world_transform { 1.0f };
        if (auto it = world_transforms.find(node_idx);
            it != world_transforms.end())
        {
            world_transform = it->second;
        }

        const auto& mesh = asset.meshes[*node.meshIndex];
        for (const auto& primitive : mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                continue;
            }
            process_primitive(
                asset, primitive, world_transform, combined, model.bounds);
        }
    }

    if (!combined.vertices.empty() && !combined.indices.empty())
    {
        model.meshes.push_back(std::move(combined));
    }

    if (model.meshes.empty())
    {
        return std::unexpected("glTF file contains no valid meshes");
    }

    // Find texture
    model.texture_path = find_texture(asset, path);

    spdlog::info("=> model (gltf): {} ({} meshes, {} verts)",
                 path.filename().string(),
                 mesh_node_count,
                 model.meshes[0].vertices.size());

    return model;
}

bool GltfLoader::supports(std::string_view extension) const
{
    auto lower = std::string(extension);
    std::ranges::transform(lower, lower.begin(), ::tolower);
    return std::ranges::any_of(k_extensions,
                               [&lower](auto ext) { return ext == lower; });
}

std::span<const std::string_view> GltfLoader::extensions() const
{
    return k_extensions;
}

} // namespace euengine
