#pragma once

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace euengine
{

/// Vertex with position, normal, texture coordinates, and optional
/// skinning/morph data
struct model_vertex final
{
    glm::vec3 position {};
    glm::vec3 normal { 0.0f, 1.0f, 0.0f };
    glm::vec2 texcoord {};

    // Skinning (up to 4 joints per vertex)
    glm::uvec4 joints { 0, 0, 0, 0 };
    glm::vec4  weights { 0.0f, 0.0f, 0.0f, 0.0f };
};

/// Axis-aligned bounding box
struct aabb final
{
    glm::vec3 min { std::numeric_limits<float>::max() };
    glm::vec3 max { std::numeric_limits<float>::lowest() };

    [[nodiscard]] constexpr glm::vec3 center() const noexcept
    {
        return (min + max) * 0.5f;
    }

    [[nodiscard]] constexpr glm::vec3 extents() const noexcept
    {
        return (max - min) * 0.5f;
    }

    [[nodiscard]] constexpr glm::vec3 size() const noexcept
    {
        return max - min;
    }

    constexpr void expand(const glm::vec3& point) noexcept
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

/// Texture type/usage
enum class texture_type : uint8_t
{
    base_color,         // Albedo/diffuse
    normal,             // Normal map
    metallic_roughness, // Metallic-roughness map
    occlusion,          // Ambient occlusion
    emissive,           // Emissive map
    unknown,            // Unknown/unspecified
};

/// Texture information
struct model_texture final
{
    std::filesystem::path path; // Path to texture file (empty if embedded)
    texture_type          type = texture_type::base_color;
    std::vector<uint8_t>
        embedded_data; // Embedded texture data (if path is empty)
    std::string
        mime_type; // MIME type for embedded textures (e.g., "image/png")
    std::int32_t width       = 0; // Width (if known from metadata)
    std::int32_t height      = 0; // Height (if known from metadata)
    bool         is_embedded = false;
};

/// Morph target (blend shape)
struct morph_target final
{
    std::string            name;
    std::vector<glm::vec3> positions; // Position deltas
    std::vector<glm::vec3> normals;   // Normal deltas (optional)
    std::vector<glm::vec3> tangents;  // Tangent deltas (optional)
};

/// Single mesh within a model
struct loaded_mesh final
{
    std::vector<model_vertex> vertices;
    std::vector<uint16_t>     indices;
    std::string               material_name;
    std::size_t               material_index = 0; // Index into materials array

    // Morph targets
    std::vector<morph_target> morph_targets;

    // Skin index (if mesh is skinned)
    std::size_t skin_index = SIZE_MAX;
};

/// Material information
struct model_material final
{
    std::string name;
    std::size_t base_color_texture_index =
        SIZE_MAX; // Index into textures array
    std::size_t normal_texture_index             = SIZE_MAX;
    std::size_t metallic_roughness_texture_index = SIZE_MAX;
    std::size_t occlusion_texture_index          = SIZE_MAX;
    std::size_t emissive_texture_index           = SIZE_MAX;
    glm::vec4   base_color_factor                = glm::vec4(1.0f);
    glm::vec3   emissive_factor                  = glm::vec3(0.0f);
    float       metallic_factor                  = 0.0f;
    float       roughness_factor                 = 1.0f;
    float       alpha_cutoff                     = 0.5f;
    bool        double_sided                     = false;
    bool        unlit = false; // KHR_materials_unlit extension
};

/// Animation interpolation type
enum class animation_interpolation : uint8_t
{
    linear,
    step,
    cubic_spline
};

/// Animation target path (what property is animated)
enum class animation_target_path : uint8_t
{
    translation,
    rotation,
    scale,
    weights // Morph target weights
};

/// Animation channel (targets a node property)
struct animation_channel final
{
    std::size_t           sampler_index = SIZE_MAX;
    std::size_t           target_node   = SIZE_MAX;
    animation_target_path target_path   = animation_target_path::translation;
};

/// Animation sampler (keyframe data)
struct animation_sampler final
{
    std::vector<float>     input;  // Time keyframes
    std::vector<glm::vec4> output; // Value keyframes (vec4 for
                                   // translation/scale/rotation quat/weights)
    animation_interpolation interpolation = animation_interpolation::linear;
};

/// Animation (collection of channels and samplers)
struct model_animation final
{
    std::string                    name;
    std::vector<animation_channel> channels;
    std::vector<animation_sampler> samplers;
    float                          duration = 0.0f; // Maximum time in seconds
};

/// Joint (bone) in a skin
struct joint final
{
    std::string name;
    glm::mat4   inverse_bind_matrix = glm::mat4(1.0f);
    std::size_t node_index          = SIZE_MAX; // Index into scene nodes
};

/// Skin (skeleton for skinned meshes)
struct model_skin final
{
    std::string        name;
    std::vector<joint> joints;
    std::vector<glm::mat4>
                inverse_bind_matrices;    // Per-joint inverse bind matrices
    std::size_t skeleton_root = SIZE_MAX; // Root joint node index
};

/// Scene graph node
struct scene_node final
{
    std::string              name;
    glm::mat4                transform    = glm::mat4(1.0f);
    std::size_t              mesh_index   = SIZE_MAX; // Index into meshes array
    std::size_t              skin_index   = SIZE_MAX; // Index into skins array
    std::size_t              camera_index = SIZE_MAX;
    std::vector<std::size_t> children; // Indices of child nodes
};

/// Complete loaded model data
struct loaded_model final
{
    std::vector<loaded_mesh>     meshes;
    std::vector<model_texture>   textures;   // All textures used by the model
    std::vector<model_material>  materials;  // All materials in the model
    std::vector<model_animation> animations; // All animations
    std::vector<model_skin>      skins;      // All skins (skeletons)
    std::vector<scene_node>      nodes;      // Scene graph nodes
    std::vector<std::size_t>     root_nodes; // Root node indices
    std::filesystem::path
         texture_path; // Legacy: first base color texture (for backward compat)
    aabb bounds;
    bool has_uvs = false;

    [[nodiscard]] bool empty() const noexcept { return meshes.empty(); }

    [[nodiscard]] std::size_t total_vertices() const noexcept
    {
        std::size_t total = 0;
        for (const auto& mesh : meshes)
        {
            total += mesh.vertices.size();
        }
        return total;
    }

    [[nodiscard]] std::size_t total_indices() const noexcept
    {
        std::size_t total = 0;
        for (const auto& mesh : meshes)
        {
            total += mesh.indices.size();
        }
        return total;
    }

    /// Get the primary texture path (for backward compatibility)
    /// Returns the first base color texture path, or empty if none
    [[nodiscard]] std::filesystem::path get_primary_texture_path()
        const noexcept
    {
        if (!texture_path.empty())
        {
            return texture_path; // Legacy field
        }

        // Find first base color texture
        for (const auto& tex : textures)
        {
            if (tex.type == texture_type::base_color && !tex.path.empty())
            {
                return tex.path;
            }
        }

        return {};
    }
};

enum class loader_error
{
    file_not_found,
    parse_error,
    invalid_format,
    no_meshes,
    unsupported_feature,
};

[[nodiscard]] constexpr std::string_view to_string(loader_error err) noexcept
{
    switch (err)
    {
        case loader_error::file_not_found:
            return "file not found";
        case loader_error::parse_error:
            return "parse error";
        case loader_error::invalid_format:
            return "invalid format";
        case loader_error::no_meshes:
            return "no meshes in file";
        case loader_error::unsupported_feature:
            return "unsupported feature";
    }
    return "unknown error";
}

/// Result type for model loading operations
using load_result = std::expected<loaded_model, std::string>;

class IModelLoader
{
public:
    virtual ~IModelLoader() = default;

    /// Load a model from file
    [[nodiscard]] virtual load_result load(
        const std::filesystem::path& path) const = 0;

    /// Check if this loader supports the given file extension
    [[nodiscard]] virtual bool supports(std::string_view extension) const = 0;

    /// Get supported extensions for this loader
    [[nodiscard]] virtual std::span<const std::string_view> extensions()
        const = 0;
};

} // namespace euengine