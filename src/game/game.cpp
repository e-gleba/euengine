#include "profiler.hpp"
#include "python_bindings.hpp"
#include "scene.hpp"
#include "ui.hpp"

#include <core-api/profiler.hpp>
#include <core-api/profiling_events.hpp>

#include <imgui.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <mutex>

namespace
{

// Sanitize UTF-8 string to avoid Pango warnings
std::string sanitize_utf8(const std::string& str)
{
    std::string result;
    result.reserve(str.size());

    for (std::size_t i = 0; i < str.size(); ++i)
    {
        auto c = static_cast<unsigned char>(str[i]);

        // Valid ASCII
        if (c < 0x80)
        {
            result += static_cast<char>(c);
        }
        // Valid UTF-8 continuation or start
        else if ((c & 0xE0) == 0xC0 && i + 1 < str.size())
        {
            // 2-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            if ((c2 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                ++i;
            }
            else
            {
                result += '?'; // Invalid, replace
            }
        }
        else if ((c & 0xF0) == 0xE0 && i + 2 < str.size())
        {
            // 3-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            auto c3 = static_cast<unsigned char>(str[i + 2]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                i += 2;
            }
            else
            {
                result += '?';
            }
        }
        else if ((c & 0xF8) == 0xF0 && i + 3 < str.size())
        {
            // 4-byte sequence
            auto c2 = static_cast<unsigned char>(str[i + 1]);
            auto c3 = static_cast<unsigned char>(str[i + 2]);
            auto c4 = static_cast<unsigned char>(str[i + 3]);
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80 &&
                (c4 & 0xC0) == 0x80)
            {
                result += static_cast<char>(c);
                result += static_cast<char>(c2);
                result += static_cast<char>(c3);
                result += static_cast<char>(c4);
                i += 3;
            }
            else
            {
                result += '?';
            }
        }
        else
        {
            result += '?'; // Invalid byte, replace with ?
        }
    }

    return result;
}

// Custom spdlog sink -> UI console
class ui_sink : public spdlog::sinks::base_sink<std::mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        int lvl = 2;
        switch (msg.level)
        {
            case spdlog::level::trace:
                lvl = 0;
                break;
            case spdlog::level::debug:
                lvl = 1;
                break;
            case spdlog::level::info:
                lvl = 2;
                break;
            case spdlog::level::warn:
                lvl = 3;
                break;
            case spdlog::level::err:
            case spdlog::level::critical:
                lvl = 4;
                break;
            default:
                break;
        }
        // Sanitize UTF-8 before logging to avoid Pango warnings
        std::string payload(msg.payload.data(), msg.payload.size());
        ui::log(lvl, sanitize_utf8(payload));
    }
    void flush_() override {}
};

std::shared_ptr<ui_sink> g_sink;

} // namespace

GAME_API euengine::preinit_result game_preinit(euengine::preinit_settings* s)
{
    s->window.title     = "euengine showcase";
    s->window.width     = 1600;
    s->window.height    = 900;
    s->window.vsync     = euengine::vsync_mode::enabled;
    s->window.resizable = true;
    s->window.high_dpi  = true;

    s->audio.master_volume = 0.8f;
    s->audio.music_volume  = 0.5f;

    s->background = { .r = 0.12f, .g = 0.14f, .b = 0.18f, .a = 1.0f };

    return euengine::preinit_result::ok;
}

GAME_API bool game_init(euengine::engine_context* ctx)
{
    // Add console sink
    g_sink = std::make_shared<ui_sink>();
    spdlog::default_logger()->sinks().push_back(g_sink);

    spdlog::info("Game module loaded");

    // Set up profiler in engine context (created in separate compile unit)
    ctx->profiler = euengine::create_profiler();
    if (ctx->profiler != nullptr)
    {
        // Set thread name via event system (preferred) and old interface
        // (backward compatibility)
        euengine::profiling_event_dispatcher::emit_thread_name("Main");
        ctx->profiler->set_thread_name("Main");
        spdlog::info("Profiler enabled");

        // Set profiler on engine (which will also set it on renderer)
        ctx->settings->set_profiler(ctx->profiler);

        // Profile this function using the interface (emits events
        // automatically)
        [[maybe_unused]] auto profiler_zone =
            profiler_zone_begin(ctx->profiler, "game_init");
    }

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    ui::init();

    scene::init(ctx);

    // Initialize Python scripting
    python_scripting::init_python();
    python_scripting::set_context(ctx);

    // Run the demo script
    std::filesystem::path script_path =
        std::filesystem::current_path() / "game" / "scripts" / "scene_demo.py";
    python_scripting::run_script(script_path.string());

    return true;
}

GAME_API void game_shutdown()
{
    spdlog::info("Game module unloaded");

    // Shutdown Python scripting
    python_scripting::shutdown_python();

    scene::shutdown();
    ui::log_clear();

    if (g_sink)
    {
        auto& sinks = spdlog::default_logger()->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), g_sink),
                    sinks.end());
        g_sink.reset();
    }
}

GAME_API void game_update(euengine::engine_context* ctx)
{
    [[maybe_unused]] auto profiler_zone =
        profiler_zone_begin(ctx->profiler, "game_update");

    // Check for keyboard shortcuts
    // Check for Ctrl+O (Open file dialog) - using scan code indices
    const bool ctrl  = ctx->input.is_ctrl_pressed();
    const bool o_key = ctx->input.is_key_pressed(18); // O key scan code

    static bool ctrl_o_was_pressed = false;
    if (ctrl && o_key && !ctrl_o_was_pressed)
    {
        ui::g_show_file_dialog = true;
        ctrl_o_was_pressed     = true;
    }
    else if (!(ctrl && o_key))
    {
        ctrl_o_was_pressed = false;
    }

    scene::update(ctx);

    // Update Python scripts
    python_scripting::update_scripts(ctx->time.elapsed, ctx->time.delta);

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    if (!ImGui::GetIO().WantCaptureMouse && ImGui::GetIO().MouseClicked[0])
    {
        ctx->settings->set_mouse_captured(true);
    }
}

GAME_API void game_render(euengine::engine_context* ctx)
{
    [[maybe_unused]] auto profiler_zone =
        profiler_zone_begin(ctx->profiler, "game_render");

    scene::render(ctx);
}

GAME_API void game_ui(euengine::engine_context* ctx)
{
    [[maybe_unused]] auto profiler_zone =
        profiler_zone_begin(ctx->profiler, "game_ui");

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx->imgui_ctx));
    // Update time for console logging before drawing UI
    ui::g_time = ctx->time.elapsed;
    ui::draw(ctx);
}