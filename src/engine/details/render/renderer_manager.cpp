/// @file renderer_manager.cpp
/// @brief Render system implementation wrapping Renderer class

#include "renderer_manager.hpp"
#include "renderer.hpp"
#include "shader/shader.hpp"

#include <SDL3/SDL_gpu.h>

namespace euengine
{

// PIMPL implementation to hide Renderer class
class render_system::impl
{
public:
    impl() = default;
    ~impl() { renderer_.shutdown(); }

    bool init(SDL_GPUDevice* device, shader_system* shaders)
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
render_system::render_system()
    : pimpl_(std::make_unique<impl>())
{
}

render_system::~render_system() = default;

bool render_system::init(SDL_GPUDevice* device, shader_system* shaders)
{
    return pimpl_->init(device, shaders);
}

void render_system::shutdown()
{
    pimpl_->shutdown();
}

i_renderer* render_system::get_renderer() const noexcept
{
    return pimpl_->get_renderer();
}

void render_system::begin_frame(SDL_GPUCommandBuffer* cmd,
                                SDL_GPURenderPass*    pass)
{
    pimpl_->begin_frame(cmd, pass);
}

void render_system::end_frame()
{
    pimpl_->end_frame();
}

void render_system::ensure_depth_texture(std::uint32_t width,
                                         std::uint32_t height)
{
    pimpl_->ensure_depth_texture(width, height);
}

void render_system::ensure_msaa_targets(std::uint32_t width,
                                        std::uint32_t height,
                                        std::uint32_t format)
{
    pimpl_->ensure_msaa_targets(width, height, format);
}

SDL_GPUTexture* render_system::msaa_color_target() const noexcept
{
    return pimpl_->msaa_color_target();
}

SDL_GPUTexture* render_system::msaa_depth_target() const noexcept
{
    return pimpl_->msaa_depth_target();
}

msaa_samples render_system::get_msaa_samples() const noexcept
{
    return pimpl_->get_msaa_samples();
}

void render_system::resolve_msaa(SDL_GPUCommandBuffer* cmd,
                                 SDL_GPUTexture*       target)
{
    pimpl_->resolve_msaa(cmd, target);
}

void render_system::ensure_pp_target(std::uint32_t width,
                                     std::uint32_t height,
                                     std::uint32_t format)
{
    pimpl_->ensure_pp_target(width, height, format);
}

SDL_GPUTexture* render_system::pp_color_target() const noexcept
{
    return pimpl_->pp_color_target();
}

void render_system::apply_postprocess(SDL_GPUCommandBuffer*     cmd,
                                      SDL_GPUTexture*           target,
                                      const postprocess_params& params)
{
    pimpl_->apply_postprocess(cmd, target, params);
}

SDL_GPUTexture* render_system::depth_texture() const noexcept
{
    return pimpl_->depth_texture();
}

void render_system::bind_pipeline()
{
    pimpl_->bind_pipeline();
}

void render_system::reload_pipelines()
{
    pimpl_->reload_pipelines();
}

void render_system::set_profiler(i_profiler* profiler) noexcept
{
    pimpl_->set_profiler(profiler);
}

bool render_system::pipeline_valid() const noexcept
{
    return pimpl_->pipeline_valid();
}

void render_system::set_msaa_samples(msaa_samples samples)
{
    pimpl_->set_msaa_samples(samples);
}

void render_system::set_max_anisotropy(float anisotropy)
{
    pimpl_->set_max_anisotropy(anisotropy);
}

void render_system::set_texture_filter(i_renderer::texture_filter filter)
{
    pimpl_->set_texture_filter(filter);
}

void render_system::set_view_projection(const glm::mat4& vp)
{
    pimpl_->set_view_projection(vp);
}

} // namespace euengine
