/// @file renderer_manager.cpp
/// @brief Renderer manager implementation wrapping Renderer class

#include "renderer_manager.hpp"
#include "../shader.hpp"
#include "renderer.hpp"

#include <SDL3/SDL_gpu.h>

namespace euengine
{

// PIMPL implementation to hide Renderer class
class renderer_manager::impl
{
public:
    impl() = default;
    ~impl() { renderer_.shutdown(); }

    bool init(SDL_GPUDevice* device, ShaderManager* shaders)
    {
        return renderer_.init(device, shaders);
    }

    void shutdown() { renderer_.shutdown(); }

    i_renderer* get_renderer() noexcept { return &renderer_; }

    void begin_frame(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass)
    {
        renderer_.begin_frame(cmd, pass);
    }

    void end_frame() { renderer_.end_frame(); }

    void ensure_depth_texture(std::uint32_t width, std::uint32_t height)
    {
        renderer_.ensure_depth_texture(width, height);
    }

    void ensure_msaa_targets(std::uint32_t width,
                             std::uint32_t height,
                             std::uint32_t format)
    {
        renderer_.ensure_msaa_targets(
            width, height, static_cast<SDL_GPUTextureFormat>(format));
    }

    SDL_GPUTexture* msaa_color_target() const noexcept
    {
        return renderer_.msaa_color_target();
    }

    SDL_GPUTexture* msaa_depth_target() const noexcept
    {
        return renderer_.msaa_depth_target();
    }

    msaa_samples get_msaa_samples() const noexcept
    {
        return renderer_.get_msaa_samples();
    }

    void resolve_msaa(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target)
    {
        renderer_.resolve_msaa(cmd, target);
    }

    void ensure_pp_target(std::uint32_t width,
                          std::uint32_t height,
                          std::uint32_t format)
    {
        renderer_.ensure_pp_target(
            width, height, static_cast<SDL_GPUTextureFormat>(format));
    }

    SDL_GPUTexture* pp_color_target() const noexcept
    {
        return renderer_.pp_color_target();
    }

    void apply_postprocess(SDL_GPUCommandBuffer*     cmd,
                           SDL_GPUTexture*           target,
                           const postprocess_params& params)
    {
        // Convert to Renderer's postprocess_params
        Renderer::postprocess_params pp_params;
        pp_params.gamma        = params.gamma;
        pp_params.brightness   = params.brightness;
        pp_params.contrast     = params.contrast;
        pp_params.saturation   = params.saturation;
        pp_params.vignette     = params.vignette;
        pp_params.fxaa_enabled = params.fxaa_enabled;
        pp_params.res_x        = params.res_x;
        pp_params.res_y        = params.res_y;
        renderer_.apply_postprocess(cmd, target, pp_params);
    }

    SDL_GPUTexture* depth_texture() const noexcept
    {
        return renderer_.depth_texture();
    }

    void bind_pipeline() { renderer_.bind_pipeline(); }

    void reload_pipelines() { renderer_.reload_pipelines(); }

    void set_profiler(i_profiler* profiler) noexcept
    {
        renderer_.set_profiler(profiler);
    }

    bool pipeline_valid() const noexcept { return renderer_.pipeline_valid(); }

    void set_msaa_samples(msaa_samples samples)
    {
        renderer_.set_msaa_samples(samples);
    }

    void set_max_anisotropy(float anisotropy)
    {
        renderer_.set_max_anisotropy(anisotropy);
    }

    void set_texture_filter(i_renderer::texture_filter filter)
    {
        renderer_.set_texture_filter(filter);
    }

    void set_view_projection(const glm::mat4& vp)
    {
        renderer_.set_view_projection(vp);
    }

private:
    Renderer renderer_;
};

// Public interface implementation
renderer_manager::renderer_manager()
    : pimpl_(std::make_unique<impl>())
{
}

renderer_manager::~renderer_manager() = default;

bool renderer_manager::init(SDL_GPUDevice* device, ShaderManager* shaders)
{
    return pimpl_->init(device, shaders);
}

void renderer_manager::shutdown()
{
    pimpl_->shutdown();
}

i_renderer* renderer_manager::get_renderer() const noexcept
{
    return pimpl_->get_renderer();
}

void renderer_manager::begin_frame(SDL_GPUCommandBuffer* cmd,
                                   SDL_GPURenderPass*    pass)
{
    pimpl_->begin_frame(cmd, pass);
}

void renderer_manager::end_frame()
{
    pimpl_->end_frame();
}

void renderer_manager::ensure_depth_texture(std::uint32_t width,
                                            std::uint32_t height)
{
    pimpl_->ensure_depth_texture(width, height);
}

void renderer_manager::ensure_msaa_targets(std::uint32_t width,
                                           std::uint32_t height,
                                           std::uint32_t format)
{
    pimpl_->ensure_msaa_targets(width, height, format);
}

SDL_GPUTexture* renderer_manager::msaa_color_target() const noexcept
{
    return pimpl_->msaa_color_target();
}

SDL_GPUTexture* renderer_manager::msaa_depth_target() const noexcept
{
    return pimpl_->msaa_depth_target();
}

msaa_samples renderer_manager::get_msaa_samples() const noexcept
{
    return pimpl_->get_msaa_samples();
}

void renderer_manager::resolve_msaa(SDL_GPUCommandBuffer* cmd,
                                    SDL_GPUTexture*       target)
{
    pimpl_->resolve_msaa(cmd, target);
}

void renderer_manager::ensure_pp_target(std::uint32_t width,
                                        std::uint32_t height,
                                        std::uint32_t format)
{
    pimpl_->ensure_pp_target(width, height, format);
}

SDL_GPUTexture* renderer_manager::pp_color_target() const noexcept
{
    return pimpl_->pp_color_target();
}

void renderer_manager::apply_postprocess(SDL_GPUCommandBuffer*     cmd,
                                         SDL_GPUTexture*           target,
                                         const postprocess_params& params)
{
    pimpl_->apply_postprocess(cmd, target, params);
}

SDL_GPUTexture* renderer_manager::depth_texture() const noexcept
{
    return pimpl_->depth_texture();
}

void renderer_manager::bind_pipeline()
{
    pimpl_->bind_pipeline();
}

void renderer_manager::reload_pipelines()
{
    pimpl_->reload_pipelines();
}

void renderer_manager::set_profiler(i_profiler* profiler) noexcept
{
    pimpl_->set_profiler(profiler);
}

bool renderer_manager::pipeline_valid() const noexcept
{
    return pimpl_->pipeline_valid();
}

void renderer_manager::set_msaa_samples(msaa_samples samples)
{
    pimpl_->set_msaa_samples(samples);
}

void renderer_manager::set_max_anisotropy(float anisotropy)
{
    pimpl_->set_max_anisotropy(anisotropy);
}

void renderer_manager::set_texture_filter(i_renderer::texture_filter filter)
{
    pimpl_->set_texture_filter(filter);
}

void renderer_manager::set_view_projection(const glm::mat4& vp)
{
    pimpl_->set_view_projection(vp);
}

} // namespace euengine
