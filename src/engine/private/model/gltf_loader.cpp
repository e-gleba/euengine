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
#include <format>
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
    // GLTF uses V=0 at top (DirectX convention), but OpenGL/Vulkan expect V=0
    // at bottom Since textures are flipped vertically when loaded, we need to
    // flip V coordinate here
    if (auto uv_it = primitive.findAttribute("TEXCOORD_0");
        uv_it != primitive.attributes.end())
    {
        const auto& uv_accessor = asset.accessors[uv_it->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset,
            uv_accessor,
            [&](glm::vec2 uv, std::size_t idx)
            {
                mesh.vertices[base_vertex + idx].texcoord =
                    glm::vec2(uv.x, 1.0f - uv.y);
            });
    }

    // Load joints (for skinning)
    if (auto joints_it = primitive.findAttribute("JOINTS_0");
        joints_it != primitive.attributes.end())
    {
        const auto& joints_accessor = asset.accessors[joints_it->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::u16vec4>(
            asset,
            joints_accessor,
            [&](glm::u16vec4 j, std::size_t idx)
            {
                mesh.vertices[base_vertex + idx].joints =
                    glm::uvec4(j.x, j.y, j.z, j.w);
            });
    }

    // Load weights (for skinning)
    if (auto weights_it = primitive.findAttribute("WEIGHTS_0");
        weights_it != primitive.attributes.end())
    {
        const auto& weights_accessor =
            asset.accessors[weights_it->accessorIndex];
        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset,
            weights_accessor,
            [&](glm::vec4 w, std::size_t idx)
            { mesh.vertices[base_vertex + idx].weights = w; });
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

/// Extract morph targets from a primitive
void extract_morph_targets(const fastgltf::Asset&     asset,
                           const fastgltf::Primitive& primitive,
                           loaded_mesh&               mesh)
{
    if (primitive.targets.empty())
    {
        return;
    }

    const std::size_t vertex_count = mesh.vertices.size();
    mesh.morph_targets.reserve(primitive.targets.size());

    for (const auto& target : primitive.targets)
    {
        morph_target morph;

        // Extract position deltas
        if (auto pos_it = std::ranges::find_if(
                target,
                [](const auto& attr) { return attr.name == "POSITION"; });
            pos_it != target.end())
        {
            const auto& pos_accessor = asset.accessors[pos_it->accessorIndex];
            morph.positions.resize(vertex_count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset,
                pos_accessor,
                [&](glm::vec3 delta, std::size_t idx)
                {
                    if (idx < vertex_count)
                    {
                        morph.positions[idx] = delta;
                    }
                });
        }

        // Extract normal deltas
        if (auto norm_it = std::ranges::find_if(
                target, [](const auto& attr) { return attr.name == "NORMAL"; });
            norm_it != target.end())
        {
            const auto& norm_accessor = asset.accessors[norm_it->accessorIndex];
            morph.normals.resize(vertex_count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset,
                norm_accessor,
                [&](glm::vec3 delta, std::size_t idx)
                {
                    if (idx < vertex_count)
                    {
                        morph.normals[idx] = delta;
                    }
                });
        }

        mesh.morph_targets.push_back(std::move(morph));
    }
}

/// Extract texture data from glTF image - handles both external files and
/// embedded textures
[[nodiscard]] model_texture extract_texture(
    const fastgltf::Asset&       asset,
    std::size_t                  image_index,
    texture_type                 type,
    const std::filesystem::path& base_path)
{
    model_texture result;
    result.type = type;

    if (image_index >= asset.images.size())
    {
        return result;
    }

    const auto& img = asset.images[image_index];

    // External texture file
    if (std::holds_alternative<fastgltf::sources::URI>(img.data))
    {
        const auto& uri = std::get<fastgltf::sources::URI>(img.data);
        std::string uri_str(uri.uri.path().begin(), uri.uri.path().end());

        // Data URI (embedded base64)
        if (uri_str.find("data:") == 0)
        {
            // Extract MIME type and data
            auto comma_pos = uri_str.find(',');
            if (comma_pos != std::string::npos)
            {
                auto mime_part     = uri_str.substr(5, comma_pos - 5);
                auto semicolon_pos = mime_part.find(';');
                result.mime_type   = (semicolon_pos != std::string::npos)
                                         ? mime_part.substr(0, semicolon_pos)
                                         : mime_part;

                // Decode base64 data
                std::string base64_data = uri_str.substr(comma_pos + 1);
                // Simple base64 decode (we'll use a basic implementation)
                // For production, consider using a proper base64 library
                result.is_embedded = true;
                // TODO: Implement base64 decoding for data URIs
                spdlog::warn("  embedded data URI texture (base64 decode not "
                             "yet implemented)");
            }
        }
        else
        {
            // External file path
            auto tex_path = base_path.parent_path() / uri_str;
            if (std::filesystem::exists(tex_path))
            {
                result.path = tex_path;
                spdlog::info("  texture: {}", uri_str);
                return result;
            }

            // Try without path - just filename
            auto filename_only = std::filesystem::path(uri_str).filename();
            tex_path           = base_path.parent_path() / filename_only;
            if (std::filesystem::exists(tex_path))
            {
                result.path = tex_path;
                spdlog::info("  texture: {}", filename_only.string());
                return result;
            }
        }
    }

    // Embedded texture from buffer view
    if (std::holds_alternative<fastgltf::sources::BufferView>(img.data))
    {
        const auto& buffer_view =
            std::get<fastgltf::sources::BufferView>(img.data);
        result.is_embedded = true;

        if (buffer_view.bufferViewIndex < asset.bufferViews.size())
        {
            const auto& view = asset.bufferViews[buffer_view.bufferViewIndex];
            if (view.bufferIndex < asset.buffers.size())
            {
                const auto& buffer = asset.buffers[view.bufferIndex];

                // Get buffer data
                if (std::holds_alternative<fastgltf::sources::Array>(
                        buffer.data))
                {
                    const auto& array =
                        std::get<fastgltf::sources::Array>(buffer.data);
                    std::size_t offset = view.byteOffset;
                    std::size_t length = view.byteLength;

                    if (offset + length <= array.bytes.size())
                    {
                        // Convert std::byte to unsigned char
                        result.embedded_data.reserve(length);
                        for (std::size_t i = 0; i < length; ++i)
                        {
                            result.embedded_data.push_back(
                                static_cast<unsigned char>(
                                    array.bytes[offset + i]));
                        }

                        // Try to determine MIME type from magic bytes
                        if (length >= 4)
                        {
                            if (result.embedded_data[0] == 0x89 &&
                                result.embedded_data[1] == 0x50 &&
                                result.embedded_data[2] == 0x4E &&
                                result.embedded_data[3] == 0x47)
                            {
                                result.mime_type = "image/png";
                            }
                            else if (result.embedded_data[0] == 0xFF &&
                                     result.embedded_data[1] == 0xD8)
                            {
                                result.mime_type = "image/jpeg";
                            }
                        }

                        spdlog::info("  embedded texture: {} bytes ({})",
                                     length,
                                     result.mime_type.empty()
                                         ? "unknown"
                                         : result.mime_type);
                        return result;
                    }
                }
                else if (std::holds_alternative<fastgltf::sources::URI>(
                             buffer.data))
                {
                    // External buffer - would need to load it
                    spdlog::warn(
                        "  texture from external buffer (not yet supported)");
                }
            }
        }
    }

    return result;
}

/// Extract all textures and materials from glTF asset
void extract_textures_and_materials(const fastgltf::Asset&       asset,
                                    const std::filesystem::path& base_path,
                                    std::vector<model_texture>&  textures,
                                    std::vector<model_material>& materials)
{
    // Extract all unique textures
    std::unordered_map<std::size_t, std::size_t>
        texture_index_map; // glTF texture index -> our texture index

    for (std::size_t mat_idx = 0; mat_idx < asset.materials.size(); ++mat_idx)
    {
        const auto&    gltf_mat = asset.materials[mat_idx];
        model_material mat;
        // Convert fastgltf string to std::string
        std::string mat_name(gltf_mat.name.begin(), gltf_mat.name.end());
        mat.name =
            mat_name.empty() ? std::format("Material_{}", mat_idx) : mat_name;

        // Extract PBR material properties
        const auto& pbr = gltf_mat.pbrData;

        // Base color factor (direct value, not optional)
        const auto& factor = pbr.baseColorFactor;
        mat.base_color_factor =
            glm::vec4(factor[0], factor[1], factor[2], factor[3]);

        // Metallic and roughness factors (direct values, not optionals)
        mat.metallic_factor  = pbr.metallicFactor;
        mat.roughness_factor = pbr.roughnessFactor;

        // Base color texture
        if (pbr.baseColorTexture.has_value())
        {
            std::size_t tex_idx = pbr.baseColorTexture->textureIndex;
            if (tex_idx < asset.textures.size())
            {
                const auto& tex = asset.textures[tex_idx];
                if (tex.imageIndex.has_value())
                {
                    auto it = texture_index_map.find(tex_idx);
                    if (it == texture_index_map.end())
                    {
                        auto model_tex =
                            extract_texture(asset,
                                            *tex.imageIndex,
                                            texture_type::base_color,
                                            base_path);
                        if (!model_tex.path.empty() ||
                            !model_tex.embedded_data.empty())
                        {
                            mat.base_color_texture_index = textures.size();
                            texture_index_map[tex_idx]   = textures.size();
                            textures.push_back(std::move(model_tex));
                        }
                    }
                    else
                    {
                        mat.base_color_texture_index = it->second;
                    }
                }
            }
        }

        // Metallic-roughness texture
        if (pbr.metallicRoughnessTexture.has_value())
        {
            std::size_t tex_idx = pbr.metallicRoughnessTexture->textureIndex;
            if (tex_idx < asset.textures.size())
            {
                const auto& tex = asset.textures[tex_idx];
                if (tex.imageIndex.has_value())
                {
                    auto it = texture_index_map.find(tex_idx);
                    if (it == texture_index_map.end())
                    {
                        auto model_tex =
                            extract_texture(asset,
                                            *tex.imageIndex,
                                            texture_type::metallic_roughness,
                                            base_path);
                        if (!model_tex.path.empty() ||
                            !model_tex.embedded_data.empty())
                        {
                            mat.metallic_roughness_texture_index =
                                textures.size();
                            texture_index_map[tex_idx] = textures.size();
                            textures.push_back(std::move(model_tex));
                        }
                    }
                    else
                    {
                        mat.metallic_roughness_texture_index = it->second;
                    }
                }
            }
        }

        // Normal texture
        if (gltf_mat.normalTexture.has_value())
        {
            std::size_t tex_idx = gltf_mat.normalTexture->textureIndex;
            if (tex_idx < asset.textures.size())
            {
                const auto& tex = asset.textures[tex_idx];
                if (tex.imageIndex.has_value())
                {
                    auto it = texture_index_map.find(tex_idx);
                    if (it == texture_index_map.end())
                    {
                        auto model_tex = extract_texture(asset,
                                                         *tex.imageIndex,
                                                         texture_type::normal,
                                                         base_path);
                        if (!model_tex.path.empty() ||
                            !model_tex.embedded_data.empty())
                        {
                            mat.normal_texture_index   = textures.size();
                            texture_index_map[tex_idx] = textures.size();
                            textures.push_back(std::move(model_tex));
                        }
                    }
                    else
                    {
                        mat.normal_texture_index = it->second;
                    }
                }
            }
        }

        // Occlusion texture
        if (gltf_mat.occlusionTexture.has_value())
        {
            std::size_t tex_idx = gltf_mat.occlusionTexture->textureIndex;
            if (tex_idx < asset.textures.size())
            {
                const auto& tex = asset.textures[tex_idx];
                if (tex.imageIndex.has_value())
                {
                    auto it = texture_index_map.find(tex_idx);
                    if (it == texture_index_map.end())
                    {
                        auto model_tex =
                            extract_texture(asset,
                                            *tex.imageIndex,
                                            texture_type::occlusion,
                                            base_path);
                        if (!model_tex.path.empty() ||
                            !model_tex.embedded_data.empty())
                        {
                            mat.occlusion_texture_index = textures.size();
                            texture_index_map[tex_idx]  = textures.size();
                            textures.push_back(std::move(model_tex));
                        }
                    }
                    else
                    {
                        mat.occlusion_texture_index = it->second;
                    }
                }
            }
        }

        // Emissive texture
        if (gltf_mat.emissiveTexture.has_value())
        {
            std::size_t tex_idx = gltf_mat.emissiveTexture->textureIndex;
            if (tex_idx < asset.textures.size())
            {
                const auto& tex = asset.textures[tex_idx];
                if (tex.imageIndex.has_value())
                {
                    auto it = texture_index_map.find(tex_idx);
                    if (it == texture_index_map.end())
                    {
                        auto model_tex = extract_texture(asset,
                                                         *tex.imageIndex,
                                                         texture_type::emissive,
                                                         base_path);
                        if (!model_tex.path.empty() ||
                            !model_tex.embedded_data.empty())
                        {
                            mat.emissive_texture_index = textures.size();
                            texture_index_map[tex_idx] = textures.size();
                            textures.push_back(std::move(model_tex));
                        }
                    }
                    else
                    {
                        mat.emissive_texture_index = it->second;
                    }
                }
            }
        }

        // Emissive factor
        const auto& emissive = gltf_mat.emissiveFactor;
        mat.emissive_factor  = glm::vec3(emissive[0], emissive[1], emissive[2]);

        // Alpha mode and cutoff
        if (gltf_mat.alphaMode == fastgltf::AlphaMode::Mask)
        {
            mat.alpha_cutoff = gltf_mat.alphaCutoff;
        }

        // Double sided
        mat.double_sided = gltf_mat.doubleSided;

        // Check for KHR_materials_unlit extension
        // fastgltf stores unlit as a bool directly, not optional
        mat.unlit = false; // TODO: Check if fastgltf exposes unlit extension

        materials.push_back(std::move(mat));
    }

    // Fallback: if no materials found, try to find any texture file next to the
    // model
    if (textures.empty())
    {
        std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".tga",
                                                ".PNG", ".JPG", ".TGA" };
        auto                     stem       = base_path.stem().string();

        for (const auto& ext : extensions)
        {
            auto tex_path = base_path.parent_path() / (stem + ext);
            if (std::filesystem::exists(tex_path))
            {
                model_texture fallback_tex;
                fallback_tex.path = tex_path;
                fallback_tex.type = texture_type::base_color;
                textures.push_back(std::move(fallback_tex));
                spdlog::info("  texture (fallback): {}",
                             tex_path.filename().string());
                break;
            }
        }

        // Check for any texture in same directory
        if (textures.empty() &&
            std::filesystem::exists(base_path.parent_path()))
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
                    model_texture fallback_tex;
                    fallback_tex.path = entry.path();
                    fallback_tex.type = texture_type::base_color;
                    textures.push_back(std::move(fallback_tex));
                    spdlog::info("  texture (dir scan): {}",
                                 entry.path().filename().string());
                    break;
                }
            }
        }
    }
}

/// Extract all animations from glTF asset
void extract_animations(const fastgltf::Asset&        asset,
                        std::vector<model_animation>& animations)
{
    animations.reserve(asset.animations.size());

    for (std::size_t anim_idx = 0; anim_idx < asset.animations.size();
         ++anim_idx)
    {
        const auto&     gltf_anim = asset.animations[anim_idx];
        model_animation anim;

        // Get animation name
        std::string anim_name(gltf_anim.name.begin(), gltf_anim.name.end());
        anim.name = anim_name.empty() ? std::format("Animation_{}", anim_idx)
                                      : anim_name;

        // Extract samplers
        anim.samplers.reserve(gltf_anim.samplers.size());
        for (const auto& gltf_sampler : gltf_anim.samplers)
        {
            animation_sampler sampler;

            // Get interpolation type
            switch (gltf_sampler.interpolation)
            {
                case fastgltf::AnimationInterpolation::Linear:
                    sampler.interpolation = animation_interpolation::linear;
                    break;
                case fastgltf::AnimationInterpolation::Step:
                    sampler.interpolation = animation_interpolation::step;
                    break;
                case fastgltf::AnimationInterpolation::CubicSpline:
                    sampler.interpolation =
                        animation_interpolation::cubic_spline;
                    break;
                default:
                    sampler.interpolation = animation_interpolation::linear;
                    break;
            }

            // Load input (time keyframes)
            if (gltf_sampler.inputAccessor < asset.accessors.size())
            {
                const auto& input_accessor =
                    asset.accessors[gltf_sampler.inputAccessor];
                sampler.input.reserve(input_accessor.count);
                fastgltf::iterateAccessor<float>(
                    asset,
                    input_accessor,
                    [&](float time)
                    {
                        sampler.input.push_back(time);
                        anim.duration = std::max(anim.duration, time);
                    });
            }

            // Load output (value keyframes)
            if (gltf_sampler.outputAccessor < asset.accessors.size())
            {
                const auto& output_accessor =
                    asset.accessors[gltf_sampler.outputAccessor];
                sampler.output.reserve(output_accessor.count);

                // Output can be vec3 (translation/scale) or vec4 (rotation
                // quaternion/weights)
                if (output_accessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessor<glm::vec3>(
                        asset,
                        output_accessor,
                        [&](glm::vec3 value)
                        { sampler.output.push_back(glm::vec4(value, 0.0f)); });
                }
                else if (output_accessor.type == fastgltf::AccessorType::Vec4)
                {
                    fastgltf::iterateAccessor<glm::vec4>(
                        asset,
                        output_accessor,
                        [&](glm::vec4 value)
                        { sampler.output.push_back(value); });
                }
            }

            anim.samplers.push_back(std::move(sampler));
        }

        // Extract channels
        anim.channels.reserve(gltf_anim.channels.size());
        for (const auto& gltf_channel : gltf_anim.channels)
        {
            animation_channel channel;
            channel.sampler_index = gltf_channel.samplerIndex;

            // nodeIndex is optional in fastgltf
            if (!gltf_channel.nodeIndex.has_value())
            {
                continue; // Skip channels without target node
            }
            channel.target_node = *gltf_channel.nodeIndex;

            // Get target path
            switch (gltf_channel.path)
            {
                case fastgltf::AnimationPath::Translation:
                    channel.target_path = animation_target_path::translation;
                    break;
                case fastgltf::AnimationPath::Rotation:
                    channel.target_path = animation_target_path::rotation;
                    break;
                case fastgltf::AnimationPath::Scale:
                    channel.target_path = animation_target_path::scale;
                    break;
                case fastgltf::AnimationPath::Weights:
                    channel.target_path = animation_target_path::weights;
                    break;
                default:
                    continue; // Skip unknown paths
            }

            anim.channels.push_back(std::move(channel));
        }

        if (!anim.channels.empty())
        {
            animations.push_back(std::move(anim));
        }
    }
}

/// Extract all skins from glTF asset
void extract_skins(const fastgltf::Asset& asset, std::vector<model_skin>& skins)
{
    skins.reserve(asset.skins.size());

    for (std::size_t skin_idx = 0; skin_idx < asset.skins.size(); ++skin_idx)
    {
        const auto& gltf_skin = asset.skins[skin_idx];
        model_skin  skin;

        // Get skin name
        std::string skin_name(gltf_skin.name.begin(), gltf_skin.name.end());
        skin.name =
            skin_name.empty() ? std::format("Skin_{}", skin_idx) : skin_name;

        // Extract joints
        skin.joints.reserve(gltf_skin.joints.size());
        for (std::size_t joint_idx : gltf_skin.joints)
        {
            joint j;
            j.node_index = joint_idx;

            if (joint_idx < asset.nodes.size())
            {
                std::string joint_name(asset.nodes[joint_idx].name.begin(),
                                       asset.nodes[joint_idx].name.end());
                j.name = joint_name.empty() ? std::format("Joint_{}", joint_idx)
                                            : joint_name;
            }

            skin.joints.push_back(std::move(j));
        }

        // Extract inverse bind matrices
        if (gltf_skin.inverseBindMatrices.has_value())
        {
            const auto& ibm_accessor =
                asset.accessors[*gltf_skin.inverseBindMatrices];
            skin.inverse_bind_matrices.reserve(ibm_accessor.count);

            fastgltf::iterateAccessor<glm::mat4>(
                asset,
                ibm_accessor,
                [&](glm::mat4 matrix)
                { skin.inverse_bind_matrices.push_back(matrix); });
        }
        else
        {
            // Default to identity matrices
            skin.inverse_bind_matrices.resize(skin.joints.size(),
                                              glm::mat4(1.0f));
        }

        // Set skeleton root if specified
        if (gltf_skin.skeleton.has_value())
        {
            skin.skeleton_root = *gltf_skin.skeleton;
        }

        skins.push_back(std::move(skin));
    }
}

/// Extract scene graph nodes from glTF asset
void extract_scene_nodes(const fastgltf::Asset&    asset,
                         std::vector<scene_node>&  nodes,
                         std::vector<std::size_t>& root_nodes)
{
    nodes.reserve(asset.nodes.size());

    // First pass: create all nodes
    for (std::size_t node_idx = 0; node_idx < asset.nodes.size(); ++node_idx)
    {
        const auto& gltf_node = asset.nodes[node_idx];
        scene_node  node;

        // Get node name
        std::string node_name(gltf_node.name.begin(), gltf_node.name.end());
        node.name =
            node_name.empty() ? std::format("Node_{}", node_idx) : node_name;

        // Get transform
        node.transform = get_node_transform(gltf_node);

        // Get mesh index
        if (gltf_node.meshIndex.has_value())
        {
            node.mesh_index = *gltf_node.meshIndex;
        }

        // Get skin index
        if (gltf_node.skinIndex.has_value())
        {
            node.skin_index = *gltf_node.skinIndex;
        }

        // Get camera index
        if (gltf_node.cameraIndex.has_value())
        {
            node.camera_index = *gltf_node.cameraIndex;
        }

        // Store children indices (will be set in second pass)
        node.children.assign(gltf_node.children.begin(),
                             gltf_node.children.end());

        nodes.push_back(std::move(node));
    }

    // Second pass: find root nodes
    std::vector<bool> is_child(asset.nodes.size(), false);
    for (const auto& gltf_node : asset.nodes)
    {
        for (std::size_t child : gltf_node.children)
        {
            if (child < asset.nodes.size())
            {
                is_child[child] = true;
            }
        }
    }

    // Get root nodes from default scene
    if (asset.defaultScene.has_value())
    {
        const auto& scene = asset.scenes[*asset.defaultScene];
        root_nodes.assign(scene.nodeIndices.begin(), scene.nodeIndices.end());
    }
    else
    {
        // Find orphan nodes
        for (std::size_t i = 0; i < asset.nodes.size(); ++i)
        {
            if (!is_child[i])
            {
                root_nodes.push_back(i);
            }
        }
    }
}

} // namespace

load_result gltf_loader::load(const std::filesystem::path& path) const
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

    // Extract scene graph nodes first (needed for skin references)
    extract_scene_nodes(asset, model.nodes, model.root_nodes);

    // Extract all skins (needed for mesh skin indices)
    extract_skins(asset, model.skins);

    // Extract all animations
    extract_animations(asset, model.animations);

    // Extract all textures and materials
    extract_textures_and_materials(
        asset, path, model.textures, model.materials);

    // Set legacy texture_path for backward compatibility
    if (!model.textures.empty() && !model.textures[0].path.empty())
    {
        model.texture_path = model.textures[0].path;
    }

    // Compute world transforms for all nodes (for flattening)
    auto world_transforms = compute_world_transforms(asset);

    int mesh_node_count = 0;

    // Process nodes that have meshes (flatten for backward compatibility)
    // TODO: Support hierarchical rendering with scene graph
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

        const auto& gltf_mesh = asset.meshes[*node.meshIndex];

        // Store skin index for this mesh
        std::size_t mesh_skin_index = SIZE_MAX;
        if (node.skinIndex.has_value())
        {
            mesh_skin_index = *node.skinIndex;
        }

        // Process each primitive as a separate mesh (or combine if same
        // material)
        for (const auto& primitive : gltf_mesh.primitives)
        {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
            {
                continue;
            }

            loaded_mesh mesh;

            // Set skin index
            mesh.skin_index = mesh_skin_index;

            // Get material index
            if (primitive.materialIndex.has_value() &&
                *primitive.materialIndex < model.materials.size())
            {
                mesh.material_index = *primitive.materialIndex;
                mesh.material_name  = model.materials[mesh.material_index].name;
            }
            else
            {
                mesh.material_name = path.stem().string();
            }

            process_primitive(
                asset, primitive, world_transform, mesh, model.bounds);

            // Extract morph targets
            extract_morph_targets(asset, primitive, mesh);

            if (!mesh.vertices.empty() && !mesh.indices.empty())
            {
                model.meshes.push_back(std::move(mesh));
            }
        }
    }

    if (model.meshes.empty())
    {
        return std::unexpected("glTF file contains no valid meshes");
    }

    spdlog::info("=> model (gltf): {} ({} mesh nodes, {} total meshes, {} "
                 "materials, {} textures, {} animations)",
                 path.filename().string(),
                 mesh_node_count,
                 model.meshes.size(),
                 model.materials.size(),
                 model.textures.size(),
                 model.animations.size());

    if (!model.meshes.empty())
    {
        std::size_t total_verts = 0;
        for (const auto& m : model.meshes)
        {
            total_verts += m.vertices.size();
        }
        spdlog::info("  => total vertices: {}, total indices: {}",
                     total_verts,
                     model.meshes[0].indices.size());
    }

    return model;
}

bool gltf_loader::supports(std::string_view extension) const
{
    auto lower = std::string(extension);
    std::ranges::transform(lower, lower.begin(), ::tolower);
    return std::ranges::any_of(k_extensions,
                               [&lower](auto ext) { return ext == lower; });
}

std::span<const std::string_view> gltf_loader::extensions() const
{
    return k_extensions;
}

} // namespace euengine