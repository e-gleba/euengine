#pragma once

#include <entt/entt.hpp>
#include <filesystem>

namespace euengine
{

// Forward declaration
struct engine_context;

/// Game module system interface - abstracts game module loading/unloading
class i_game_module_system
{
public:
    virtual ~i_game_module_system() = default;

    /// Load a game module from the given path
    /// @param path Path to the game shared library (.so/.dll)
    /// @param ctx Engine context to pass to game_init
    /// @return true if loaded successfully, false otherwise
    [[nodiscard]] virtual bool load(const std::filesystem::path& path,
                                    engine_context*              ctx) = 0;

    /// Unload the currently loaded game module
    /// @param registry ECS registry to clear (optional, can be nullptr)
    virtual void unload(entt::registry* registry = nullptr) noexcept = 0;

    /// Reload the game module from the same path (for hot-reload)
    /// @param ctx Engine context to pass to game_init
    /// @return true if reloaded successfully, false otherwise
    [[nodiscard]] virtual bool reload(engine_context* ctx) noexcept = 0;

    /// Check if a game module is currently loaded
    [[nodiscard]] virtual bool is_loaded() const noexcept = 0;

    /// Get the path of the currently loaded game module
    [[nodiscard]] virtual const std::filesystem::path& get_path()
        const noexcept = 0;
};

} // namespace euengine
