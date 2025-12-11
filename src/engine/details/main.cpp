#define SDL_MAIN_USE_CALLBACKS
#include "engine.hpp"

#include <core-api/platform.hpp>
#include <core-api/window.hpp>

#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <filesystem>
#include <memory>

namespace
{

struct app_state final
{
    std::unique_ptr<euengine::engine> eng;
    euengine::preinit_settings        settings =
        euengine::preinit_settings::defaults();
    std::filesystem::path game_lib_path;
    bool                  game_loaded = false;
};

[[nodiscard]] std::filesystem::path get_game_library_path() noexcept
{
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr)
    {
        spdlog::error("SDL_GetBasePath failed: {}", SDL_GetError());
        return {};
    }

    try
    {
        return std::filesystem::path { base_path } /
               euengine::platform::game_library_name();
    }
    catch (const std::exception& e)
    {
        spdlog::error("'{}' path build failed: {}", base_path, e.what());
    }

    return {};
}

[[nodiscard]] euengine::preinit_result try_game_preinit(
    const std::filesystem::path& path, euengine::preinit_settings* settings)
{
    if (!std::filesystem::exists(path))
    {
        spdlog::warn("game library not found: {}", path.string());
        return euengine::preinit_result::skip;
    }

    auto* shared_object = SDL_LoadObject(path.c_str());
    if (shared_object == nullptr)
    {
        spdlog::error("failed to load game library: {}", SDL_GetError());
        return euengine::preinit_result::skip;
    }

    struct so_guard final
    {
        SDL_SharedObject* p {};
        ~so_guard() noexcept { SDL_UnloadObject(p); }
    } lib { shared_object };

    auto* preinit_fn = reinterpret_cast<euengine::game_preinit_fn>(
        SDL_LoadFunction(shared_object, "game_preinit"));

    if (preinit_fn == nullptr)
    {
        spdlog::error("failed to load game preinit funtion in '{}': {}",
                      path,
                      SDL_GetError());
        return euengine::preinit_result::skip;
    }

    spdlog::info("calling game preinit");
    return preinit_fn(settings);
}

} // namespace

SDL_AppResult SDL_AppInit(void**                 appstate,
                          [[maybe_unused]] int   argc,
                          [[maybe_unused]] char* argv[])
{
    spdlog::info("=> SDL_AppInit");

    auto* state = new app_state {};
    *appstate   = state;

    // Set default window settings
    state->settings.window = euengine::window_settings {
        .title     = "euengine",
        .width     = 1280,
        .height    = 720,
        .mode      = euengine::window_mode::windowed,
        .vsync     = euengine::vsync_mode::enabled,
        .resizable = true,
        .high_dpi  = true,
    };

    state->game_lib_path = get_game_library_path();

    auto preinit_result =
        try_game_preinit(state->game_lib_path, &state->settings);

    switch (preinit_result)
    {
        case euengine::preinit_result::quit:
            spdlog::info("game_preinit requested quit");
            delete state;
            *appstate = nullptr;
            return SDL_APP_FAILURE;

        case euengine::preinit_result::skip:
            spdlog::info("game will not be loaded");
            state->game_loaded = false;
            break;

        case euengine::preinit_result::ok:
            state->game_loaded = true;
            break;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        spdlog::error("SDL_Init: {}", SDL_GetError());
        delete state;
        *appstate = nullptr;
        return SDL_APP_FAILURE;
    }

    state->eng = std::make_unique<euengine::engine>();

    if (!state->eng->init(state->settings))
    {
        spdlog::error("engine initialization failed");
        delete state;
        *appstate = nullptr;
        return SDL_APP_FAILURE;
    }

    if (state->game_loaded && !state->game_lib_path.empty())
    {
        if (!state->eng->load_game(state->game_lib_path))
        {
            spdlog::warn("game load failed, continuing without game");
            state->game_loaded = false;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    auto* state = static_cast<app_state*>(appstate);
    if (state == nullptr || state->eng == nullptr)
    {
        return SDL_APP_FAILURE;
    }

    if (!state->eng->process_event(*event))
    {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    auto* state = static_cast<app_state*>(appstate);
    if (state == nullptr || state->eng == nullptr)
    {
        return SDL_APP_FAILURE;
    }

    if (!state->eng->is_running())
    {
        return SDL_APP_SUCCESS;
    }

    state->eng->iterate();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, [[maybe_unused]] SDL_AppResult result)
{
    spdlog::info("=> SDL_AppQuit");

    auto* state = static_cast<app_state*>(appstate);
    if (state != nullptr)
    {
        if (state->eng != nullptr)
        {
            state->eng->shutdown();
        }
        delete state;
    }

    SDL_Quit();
}