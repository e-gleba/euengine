#pragma once

#include <functional>

// Forward declarations to avoid exposing SDL3 to game module
struct SDL_Window;
struct SDL_GPUDevice;
typedef union SDL_Event SDL_Event; // SDL3 uses typedef union
struct SDL_GPUCommandBuffer;
struct SDL_GPUTexture;

namespace euengine
{

/// Overlay layer interface - abstracts overlay/UI rendering implementation
/// Engine doesn't know about specific UI library (ImGui, etc.)
class i_overlay_layer
{
public:
    using draw_callback = std::function<void()>;

    virtual ~i_overlay_layer() = default;

    /// Initialize UI layer with window and GPU device
    /// @param window SDL window (non-owning)
    /// @param device GPU device (non-owning)
    /// @return true if initialized successfully
    [[nodiscard]] virtual bool init(SDL_Window*    window,
                                    SDL_GPUDevice* device) = 0;

    /// Shutdown UI layer and release resources
    virtual void shutdown() = 0;

    /// Process SDL event for UI input
    /// @param event SDL event to process
    virtual void process_event(const SDL_Event& event) = 0;

    /// Begin a new UI frame
    virtual void begin_frame() = 0;

    /// End UI frame and render to target texture
    /// @param cmd GPU command buffer
    /// @param target Target texture to render to
    virtual void end_frame(SDL_GPUCommandBuffer* cmd,
                           SDL_GPUTexture*       target) = 0;

    /// Enable or disable UI input processing
    /// @param enabled Whether input should be enabled
    virtual void set_input_enabled(bool enabled) = 0;

    /// Check if input is enabled
    /// @return true if input is enabled
    [[nodiscard]] virtual bool input_enabled() const = 0;

    /// Set callback to be called during frame rendering
    /// @param callback Function to call for drawing UI
    virtual void set_draw_callback(draw_callback callback) = 0;

    /// Check if UI wants to capture mouse input
    /// @return true if UI wants to capture mouse
    [[nodiscard]] virtual bool wants_capture_mouse() const = 0;
};

} // namespace euengine
