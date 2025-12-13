/// @file game_module_manager.cpp
/// @brief Game module manager implementation using SDL3_loadso

#include "game_module_manager.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_loadso.h>
#include <entt/entt.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <format>

namespace euengine
{

// PIMPL implementation to hide SDL3 details
class game_module_system::impl final
{
public:
    impl() = default;
    ~impl() { unload_internal(nullptr); }

    bool load(const std::filesystem::path& path, engine_context* ctx)
    {
        if (is_loaded())
        {
            spdlog::warn("Game module already loaded, unloading first");
            unload_internal(nullptr);
        }

        spdlog::info("=> loading game: {}", path.string());
        game_lib_path_ = path;

        // For hot-reload to work, we must copy the DLL to a temp location
        // Otherwise the dynamic loader caches the old version
        std::filesystem::path load_path = path;

        if (std::filesystem::exists(path))
        {
            // Create temp copy with timestamp to ensure unique name
            auto temp_dir = std::filesystem::temp_directory_path();
            auto timestamp =
                std::chrono::steady_clock::now().time_since_epoch().count();
            auto temp_name = std::format("game_{}_{}{}",
                                         timestamp,
                                         std::rand(),
                                         path.extension().string());
            auto temp_path = temp_dir / temp_name;

            std::error_code ec;
            std::filesystem::copy_file(
                path,
                temp_path,
                std::filesystem::copy_options::overwrite_existing,
                ec);

            if (!ec)
            {
                load_path       = temp_path;
                game_temp_path_ = temp_path; // Store for cleanup
                spdlog::debug("=> copied game lib to: {}", temp_path.string());
            }
            else
            {
                spdlog::warn("=> failed to copy game lib: {}", ec.message());
            }
        }

        auto* raw_lib = SDL_LoadObject(load_path.c_str());
        if (raw_lib == nullptr)
        {
            spdlog::error("SDL_LoadObject: {}", SDL_GetError());
            return false;
        }
        game_lib_.reset(raw_lib);

        // Load function pointers (preinit is optional for hot-reload)
        game_preinit_ = reinterpret_cast<game_preinit_fn>(
            SDL_LoadFunction(raw_lib, "game_preinit"));
        game_init_ = reinterpret_cast<game_init_fn>(
            SDL_LoadFunction(raw_lib, "game_init"));
        game_shutdown_ = reinterpret_cast<game_shutdown_fn>(
            SDL_LoadFunction(raw_lib, "game_shutdown"));
        game_update_ = reinterpret_cast<game_update_fn>(
            SDL_LoadFunction(raw_lib, "game_update"));
        game_render_ = reinterpret_cast<game_render_fn>(
            SDL_LoadFunction(raw_lib, "game_render"));
        game_ui_ =
            reinterpret_cast<game_ui_fn>(SDL_LoadFunction(raw_lib, "game_ui"));

        // Verify all required exports are present (preinit is optional)
        if ((game_init_ == nullptr) || (game_shutdown_ == nullptr) ||
            (game_update_ == nullptr) || (game_render_ == nullptr) ||
            (game_ui_ == nullptr))
        {
            spdlog::error("missing game exports");
            cleanup_pointers();
            game_lib_.reset();
            return false;
        }

        if (ctx != nullptr && !game_init_(ctx))
        {
            cleanup_pointers();
            game_lib_.reset();
            return false;
        }
        return true;
    }

    void unload(entt::registry* registry) noexcept
    {
        unload_internal(registry);
    }

    bool reload(engine_context* ctx) noexcept
    {
        if (game_lib_path_.empty())
        {
            return false;
        }
        auto path = game_lib_path_;
        unload_internal(nullptr);
        return load(path, ctx);
    }

    bool is_loaded() const noexcept { return game_lib_ != nullptr; }

    const std::filesystem::path& get_path() const noexcept
    {
        return game_lib_path_;
    }

    preinit_result call_preinit(preinit_settings* settings) const
    {
        if (game_preinit_ != nullptr && settings != nullptr)
        {
            return game_preinit_(settings);
        }
        return preinit_result::ok;
    }

    void call_update(engine_context* ctx) const
    {
        if (game_update_ != nullptr && ctx != nullptr)
        {
            game_update_(ctx);
        }
    }

    void call_render(engine_context* ctx) const
    {
        if (game_render_ != nullptr && ctx != nullptr)
        {
            game_render_(ctx);
        }
    }

    void call_ui(engine_context* ctx) const
    {
        if (game_ui_ != nullptr && ctx != nullptr)
        {
            game_ui_(ctx);
        }
    }

private:
    void unload_internal(entt::registry* registry) noexcept
    {
        if (game_shutdown_ != nullptr)
        {
            try
            {
                game_shutdown_();
            }
            catch (...)
            {
            }
            spdlog::default_logger()->flush();
        }

        if (registry != nullptr)
        {
            registry->clear();
        }

        cleanup_pointers();
        game_lib_.reset();

        // Cleanup temp file after unloading the library
        if (!game_temp_path_.empty())
        {
            std::error_code ec;
            std::filesystem::remove(game_temp_path_, ec);
            if (ec)
            {
                spdlog::debug("=> failed to remove temp game lib: {}",
                              ec.message());
            }
            game_temp_path_.clear();
        }
    }

    void cleanup_pointers() noexcept
    {
        game_preinit_  = nullptr;
        game_init_     = nullptr;
        game_shutdown_ = nullptr;
        game_update_   = nullptr;
        game_render_   = nullptr;
        game_ui_       = nullptr;
    }

    // SDL3 deleter for shared objects
    struct sdl_shared_object_deleter
    {
        void operator()(SDL_SharedObject* o) const noexcept
        {
            if (o != nullptr)
            {
                SDL_UnloadObject(o);
            }
        }
    };

    using sdl_shared_object_ptr =
        std::unique_ptr<SDL_SharedObject, sdl_shared_object_deleter>;

    sdl_shared_object_ptr game_lib_;
    std::filesystem::path game_lib_path_;
    std::filesystem::path game_temp_path_; // Temp copy for hot-reload

    game_preinit_fn  game_preinit_  = nullptr;
    game_init_fn     game_init_     = nullptr;
    game_shutdown_fn game_shutdown_ = nullptr;
    game_update_fn   game_update_   = nullptr;
    game_render_fn   game_render_   = nullptr;
    game_ui_fn       game_ui_       = nullptr;
};

// Public interface implementation
game_module_system::game_module_system()
    : pimpl_(std::make_unique<impl>())
{
}

game_module_system::~game_module_system() = default;

bool game_module_system::load(const std::filesystem::path& path,
                              engine_context*              ctx)
{
    return pimpl_->load(path, ctx);
}

void game_module_system::unload(entt::registry* registry) noexcept
{
    pimpl_->unload(registry);
}

bool game_module_system::reload(engine_context* ctx) noexcept
{
    return pimpl_->reload(ctx);
}

bool game_module_system::is_loaded() const noexcept
{
    return pimpl_->is_loaded();
}

const std::filesystem::path& game_module_system::get_path() const noexcept
{
    return pimpl_->get_path();
}

preinit_result game_module_system::call_preinit(
    preinit_settings* settings) const
{
    return pimpl_->call_preinit(settings);
}

void game_module_system::call_update(engine_context* ctx) const
{
    pimpl_->call_update(ctx);
}

void game_module_system::call_render(engine_context* ctx) const
{
    pimpl_->call_render(ctx);
}

void game_module_system::call_ui(engine_context* ctx) const
{
    pimpl_->call_ui(ctx);
}

} // namespace euengine