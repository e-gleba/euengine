#pragma once

#include <core-api/overlay_layer.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

namespace egen
{

/// ImGui-based UI layer implementation
class imgui_layer final : public i_overlay_layer
{
public:
    imgui_layer() = default;
    ~imgui_layer() override;
    imgui_layer(const imgui_layer&)            = delete;
    imgui_layer& operator=(const imgui_layer&) = delete;
    imgui_layer(imgui_layer&&)                 = delete;
    imgui_layer& operator=(imgui_layer&&)      = delete;

    bool init(SDL_Window* window, SDL_GPUDevice* device) override;
    void shutdown() override;

    void process_event(const SDL_Event& event) override;

    void begin_frame() override;
    void end_frame(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target) override;

    void set_input_enabled(bool enabled) override { input_enabled_ = enabled; }
    [[nodiscard]] bool input_enabled() const override { return input_enabled_; }

    void set_draw_callback(draw_callback callback) override;

    [[nodiscard]] bool wants_capture_mouse() const override;

    /// Get ImGui context (ImGui-specific, returns nullptr for other
    /// implementations)
    /// @return ImGui context pointer, or nullptr
    [[nodiscard]] void* get_imgui_context() const;

private:
    SDL_GPUDevice*       device_        = nullptr;
    SDL_GPUTextureFormat target_format_ = SDL_GPU_TEXTUREFORMAT_INVALID;
    draw_callback        draw_callback_;
    bool                 initialized_   = false;
    bool                 input_enabled_ = true;
};

} // namespace egen
