#pragma once

#include "audio.hpp"
#include "engine.hpp"
#include "profiler.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <entt/entt.hpp>

namespace euengine
{

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
        return { 0.08f, 0.08f, 0.12f, 1.0f };
    }

    [[nodiscard]] static constexpr clear_color sky() noexcept
    {
        return { 0.4f, 0.6f, 0.9f, 1.0f };
    }

    [[nodiscard]] static constexpr clear_color sunset() noexcept
    {
        return { 0.95f, 0.5f, 0.3f, 1.0f };
    }

    [[nodiscard]] static constexpr clear_color night() noexcept
    {
        return { 0.02f, 0.02f, 0.05f, 1.0f };
    }
};

/// Engine context passed to game callbacks
struct engine_context final
{
    entt::registry*    registry  = nullptr;
    i_renderer*        renderer  = nullptr;
    i_shader_manager*  shaders   = nullptr;
    i_audio*           audio     = nullptr;
    i_engine_settings* settings  = nullptr;
    i_profiler*        profiler  = nullptr; ///< Optional profiler interface
    void*              imgui_ctx = nullptr;
    display_info       display   = {};
    input_state        input     = {};
    time_info          time      = {};
    clear_color*       background =
        nullptr; ///< Game can modify to change clear color

    /// Current key sequence for vim-like bindings (read-only)
    /// Format: string representation of keys pressed (e.g., "gg", "dd")
    /// Empty if no sequence is active
    const char* key_sequence =
        nullptr; ///< Pointer to internal buffer, valid until next frame

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
