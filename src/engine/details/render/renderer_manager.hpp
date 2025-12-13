#pragma once

#include <core-api/renderer.hpp>

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>

// Forward declarations for SDL3 GPU types (opaque to users)
struct SDL_GPUDevice;
struct SDL_GPUCommandBuffer;
struct SDL_GPURenderPass;
struct SDL_GPUTexture;

namespace euengine
{

// Forward declarations
class ShaderManager;
class i_profiler;

/// Post-processing parameters
struct postprocess_params
{
    float gamma        = 2.2f;
    float brightness   = 0.0f;
    float contrast     = 1.0f;
    float saturation   = 1.0f;
    float vignette     = 0.0f;
    float fxaa_enabled = 0.0f;
    float res_x        = 1920.0f;
    float res_y        = 1080.0f;
};

/// Renderer manager - handles renderer initialization and frame management
/// Provides a clean interface without exposing Renderer implementation details
class renderer_manager
{
public:
    renderer_manager();
    ~renderer_manager();

    renderer_manager(const renderer_manager&)            = delete;
    renderer_manager& operator=(const renderer_manager&) = delete;
    renderer_manager(renderer_manager&&)                 = delete;
    renderer_manager& operator=(renderer_manager&&)      = delete;

    /// Initialize renderer with GPU device and shader manager
    /// @param device SDL3 GPU device (non-owning)
    /// @param shaders Shader manager (non-owning)
    /// @return true if initialized successfully
    [[nodiscard]] bool init(SDL_GPUDevice* device, ShaderManager* shaders);

    /// Shutdown renderer and release all resources
    void shutdown();

    /// Get the i_renderer interface (for game access)
    /// @return Pointer to i_renderer interface, or nullptr if not initialized
    [[nodiscard]] i_renderer* get_renderer() const noexcept;

    /// Begin a new frame
    /// @param cmd Command buffer for this frame
    /// @param pass Render pass for this frame
    void begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);

    /// End current frame
    void end_frame();

    /// Ensure depth texture matches given dimensions
    void ensure_depth_texture(std::uint32_t width, std::uint32_t height);

    /// Ensure MSAA render targets match dimensions and sample count
    /// @param width Target width
    /// @param height Target height
    /// @param format Texture format
    void ensure_msaa_targets(std::uint32_t width,
                             std::uint32_t height,
                             std::uint32_t format);

    /// Get MSAA color target texture (or nullptr if MSAA disabled)
    /// @return SDL3 GPU texture pointer, or nullptr
    [[nodiscard]] SDL_GPUTexture* msaa_color_target() const noexcept;

    /// Get MSAA depth target texture (or nullptr if MSAA disabled)
    /// @return SDL3 GPU texture pointer, or nullptr
    [[nodiscard]] SDL_GPUTexture* msaa_depth_target() const noexcept;

    /// Get current MSAA sample count
    [[nodiscard]] msaa_samples get_msaa_samples() const noexcept;

    /// Resolve MSAA to target texture (call after render pass)
    /// @param cmd Command buffer
    /// @param target Target texture to resolve to
    void resolve_msaa(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target);

    /// Ensure post-processing render target matches dimensions
    /// @param width Target width
    /// @param height Target height
    /// @param format Texture format
    void ensure_pp_target(std::uint32_t width,
                          std::uint32_t height,
                          std::uint32_t format);

    /// Get post-processing color target texture
    /// @return SDL3 GPU texture pointer, or nullptr
    [[nodiscard]] SDL_GPUTexture* pp_color_target() const noexcept;

    /// Apply post-processing effects
    /// @param cmd Command buffer
    /// @param target Target texture to render to
    /// @param params Post-processing parameters
    void apply_postprocess(SDL_GPUCommandBuffer*     cmd,
                           SDL_GPUTexture*           target,
                           const postprocess_params& params);

    /// Get depth texture
    /// @return SDL3 GPU texture pointer, or nullptr
    [[nodiscard]] SDL_GPUTexture* depth_texture() const noexcept;

    /// Bind default pipeline (called at frame start)
    void bind_pipeline();

    /// Rebuild pipelines if shaders changed
    void reload_pipelines();

    /// Set profiler for detailed profiling zones
    void set_profiler(i_profiler* profiler) noexcept;

    /// Check if pipeline is valid
    [[nodiscard]] bool pipeline_valid() const noexcept;

    /// Set MSAA samples
    void set_msaa_samples(msaa_samples samples);

    /// Set max anisotropy
    void set_max_anisotropy(float anisotropy);

    /// Set texture filter
    void set_texture_filter(i_renderer::texture_filter filter);

    /// Set view projection matrix
    void set_view_projection(const glm::mat4& vp);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace euengine
