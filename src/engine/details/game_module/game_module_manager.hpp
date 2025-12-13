#pragma once

#include <core-api/game.hpp>

#include <filesystem>
#include <memory>

namespace euengine
{

/// Game module manager - handles loading/unloading of game shared libraries
/// Provides a clean interface without exposing implementation details
class game_module_manager
{
public:
    game_module_manager();
    ~game_module_manager();

    game_module_manager(const game_module_manager&)            = delete;
    game_module_manager& operator=(const game_module_manager&) = delete;
    game_module_manager(game_module_manager&&)                 = delete;
    game_module_manager& operator=(game_module_manager&&)      = delete;

    /// Load a game module from the given path
    /// @param path Path to the game shared library (.so/.dll)
    /// @param ctx Engine context to pass to game_init
    /// @return true if loaded successfully, false otherwise
    [[nodiscard]] bool load(const std::filesystem::path& path,
                            engine_context*              ctx);

    /// Unload the currently loaded game module
    /// @param registry ECS registry to clear (optional, can be nullptr)
    void unload(entt::registry* registry = nullptr) noexcept;

    /// Reload the game module from the same path (for hot-reload)
    /// @param ctx Engine context to pass to game_init
    /// @return true if reloaded successfully, false otherwise
    [[nodiscard]] bool reload(engine_context* ctx) noexcept;

    /// Check if a game module is currently loaded
    [[nodiscard]] bool is_loaded() const noexcept;

    /// Get the path of the currently loaded game module
    [[nodiscard]] const std::filesystem::path& get_path() const noexcept;

    /// Call game_preinit (optional, may be nullptr)
    /// @param settings Preinit settings to modify
    /// @return preinit_result from the game
    [[nodiscard]] preinit_result call_preinit(preinit_settings* settings) const;

    /// Call game_update
    void call_update(engine_context* ctx) const;

    /// Call game_render
    void call_render(engine_context* ctx) const;

    /// Call game_ui
    void call_ui(engine_context* ctx) const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace euengine
