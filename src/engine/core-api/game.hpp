#pragma once

#include "audio_system.hpp"
#include "engine.hpp"
#include "model_loader.hpp"
#include "overlay_layer.hpp"
#include "profiler.hpp"
#include "render_system.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <entt/entt.hpp>

namespace euengine
{

// Forward declaration to break circular dependency
class i_game_module_system;

/// Input state provided to game each frame
struct input_state final
{
    const bool* keyboard   = nullptr; ///< Keyboard state array (SDL scan codes)
    float       mouse_x    = 0.0f;    ///< Mouse X position in window
    float       mouse_y    = 0.0f;    ///< Mouse Y position in window
    float       mouse_xrel = 0.0f;    ///< Relative mouse X motion
    float       mouse_yrel = 0.0f;    ///< Relative mouse Y motion
    bool        mouse_left = false;
    bool        mouse_right    = false;
    bool        mouse_middle   = false;
    bool        mouse_captured = false;

    /// Check if a key is pressed (using scan code index)
    /// @param scancode_index Scan code index (e.g., 224 for LCTRL, 228 for
    /// RCTRL, 18 for O)
    [[nodiscard]] bool is_key_pressed(int scancode_index) const noexcept
    {
        return keyboard != nullptr && keyboard[scancode_index];
    }

    /// Check if Ctrl is pressed (either left or right)
    [[nodiscard]] bool is_ctrl_pressed() const noexcept
    {
        return is_key_pressed(224) || is_key_pressed(228); // LCTRL or RCTRL
    }

    /// Check if Alt is pressed (either left or right)
    [[nodiscard]] bool is_alt_pressed() const noexcept
    {
        return is_key_pressed(226) || is_key_pressed(230); // LALT or RALT
    }

    /// Check if Shift is pressed (either left or right)
    [[nodiscard]] bool is_shift_pressed() const noexcept
    {
        return is_key_pressed(225) || is_key_pressed(229); // LSHIFT or RSHIFT
    }
};

/// Display information
struct display_info final
{
    int   width  = 0;
    int   height = 0;
    float aspect = 1.0f;
};

/// Time information for animations and game logic
struct time_info final
{
    float    elapsed     = 0.0f; ///< Total elapsed time since start (seconds)
    float    delta       = 0.0f; ///< Frame delta time (seconds)
    uint64_t frame_count = 0;    ///< Total frames rendered
    float    fps         = 0.0f; ///< Smoothed FPS estimate
};

/// Clear color for background
struct clear_color final
{
    float r = 0.08f;
    float g = 0.08f;
    float b = 0.12f;
    float a = 1.0f;

    [[nodiscard]] static constexpr clear_color dark() noexcept
    {
        return { .r = 0.08f, .g = 0.08f, .b = 0.12f, .a = 1.0f };
    }

    [[nodiscard]] static constexpr clear_color sky() noexcept
    {
        return { .r = 0.4f, .g = 0.6f, .b = 0.9f, .a = 1.0f };
    }

    [[nodiscard]] static constexpr clear_color sunset() noexcept
    {
        return { .r = 0.95f, .g = 0.5f, .b = 0.3f, .a = 1.0f };
    }

    [[nodiscard]] static constexpr clear_color night() noexcept
    {
        return { .r = 0.02f, .g = 0.02f, .b = 0.05f, .a = 1.0f };
    }
};

/// Engine context passed to game callbacks
/// Provides access to all engine subsystems through getter methods
struct engine_context final
{
    // Subsystem interfaces (set by engine, accessed via getters)
    i_render_system*      render_system_      = nullptr;
    i_shader_system*      shader_system_      = nullptr;
    i_audio_system*       audio_system_       = nullptr;
    i_overlay_layer*      overlay_layer_      = nullptr;
    i_model_loader*       model_loader_       = nullptr;
    i_game_module_system* game_module_system_ = nullptr;
    i_engine_settings*    settings_           = nullptr;
    i_profiler*           profiler_           = nullptr;
    entt::registry*       registry_           = nullptr;
    void*                 imgui_ctx_          = nullptr;

    // Frame data (updated each frame)
    display_info display      = {};
    input_state  input        = {};
    time_info    time         = {};
    clear_color* background   = nullptr;
    const char*  key_sequence = nullptr;

    /// Get the ECS registry
    [[nodiscard]] entt::registry* get_registry() const noexcept
    {
        return registry_;
    }

    /// Get the render system interface
    [[nodiscard]] i_render_system* get_render_system() const noexcept
    {
        return render_system_;
    }

    /// Get the shader system interface
    [[nodiscard]] i_shader_system* get_shader_system() const noexcept
    {
        return shader_system_;
    }

    /// Get the audio system interface
    [[nodiscard]] i_audio_system* get_audio_system() const noexcept
    {
        return audio_system_;
    }

    /// Get the overlay layer interface
    [[nodiscard]] i_overlay_layer* get_overlay_layer() const noexcept
    {
        return overlay_layer_;
    }

    /// Get the model loader interface
    [[nodiscard]] i_model_loader* get_model_loader() const noexcept
    {
        return model_loader_;
    }

    /// Get the game module system interface
    [[nodiscard]] i_game_module_system* get_game_module_system() const noexcept
    {
        return game_module_system_;
    }

    /// Get the engine settings interface
    [[nodiscard]] i_engine_settings* get_settings() const noexcept
    {
        return settings_;
    }

    /// Get the profiler interface (may be nullptr)
    [[nodiscard]] i_profiler* get_profiler() const noexcept
    {
        return profiler_;
    }

    /// Get the ImGui context (may be nullptr, implementation-specific)
    [[nodiscard]] void* get_imgui_context() const noexcept
    {
        return imgui_ctx_;
    }

    /// Get display information
    [[nodiscard]] const display_info& get_display() const noexcept
    {
        return display;
    }

    /// Get input state
    [[nodiscard]] const input_state& get_input() const noexcept
    {
        return input;
    }

    /// Get time information
    [[nodiscard]] const time_info& get_time() const noexcept { return time; }

    /// Get background clear color (mutable, game can modify)
    [[nodiscard]] clear_color* get_background() const noexcept
    {
        return background;
    }

    /// Get current key sequence for vim-like bindings (read-only)
    [[nodiscard]] const char* get_key_sequence() const noexcept
    {
        return key_sequence;
    }

    // Legacy compatibility - direct accessors (deprecated, use getters)
    [[deprecated("Use get_render_system() instead")]] i_renderer* renderer =
        nullptr;
    [[deprecated("Use get_shader_system() instead")]] i_shader_system* shaders =
        nullptr;
    [[deprecated("Use get_audio_system() instead")]] i_audio* audio = nullptr;
    [[deprecated("Use get_settings() instead")]] i_engine_settings* settings =
        nullptr;
    [[deprecated("Use get_profiler() instead")]] i_profiler* profiler = nullptr;
    [[deprecated("Use get_imgui_context() instead")]] void* imgui_ctx = nullptr;
    [[deprecated("Use get_registry() instead")]] entt::registry* registry =
        nullptr;

    // Legacy compatibility
    [[nodiscard]] float delta_time() const noexcept { return time.delta; }
};

/// Pre-initialization settings that game can configure before engine init
/// All SDL-free, pure data structures
struct preinit_settings final
{
    window_settings   window     = {};
    renderer_settings renderer   = {};
    audio_settings    audio      = {};
    clear_color       background = clear_color::dark();

    [[nodiscard]] static constexpr preinit_settings defaults() noexcept
    {
        return preinit_settings {
            .window     = window_settings {},
            .renderer   = renderer_settings::defaults(),
            .audio      = audio_settings::defaults(),
            .background = clear_color::dark(),
        };
    }
};

/// Result from game preinit callback
enum class preinit_result : std::uint8_t
{
    ok,   ///< Continue with initialization
    skip, ///< Skip game loading (engine runs without game)
    quit, ///< Abort application launch
};

extern "C"
{
    /// Called before SDL initialization - game can modify settings
    /// Return preinit_result to control engine behavior
    using game_preinit_fn = preinit_result (*)(preinit_settings* settings);

    /// Called after engine initialization
    using game_init_fn = bool (*)(engine_context* ctx);

    /// Called on engine shutdown
    using game_shutdown_fn = void (*)();

    /// Called each frame for game logic
    using game_update_fn = void (*)(engine_context* ctx);

    /// Called each frame for rendering
    using game_render_fn = void (*)(engine_context* ctx);

    /// Called each frame for UI (ImGui)
    using game_ui_fn = void (*)(engine_context* ctx);
}

#if defined(_WIN32) || defined(_WIN64)
#define GAME_API extern "C" __declspec(dllexport)
#else
#define GAME_API extern "C" __attribute__((visibility("default")))
#endif

} // namespace euengine