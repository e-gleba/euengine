#pragma once

namespace euengine
{

// Forward declarations
struct engine_context;
class i_engine_settings;
class i_profiler;

/// Engine interface - main entry point for game interaction
/// Game uses this interface to interact with the engine
class i_engine
{
public:
    virtual ~i_engine() = default;

    /// Get the engine context (provides access to all subsystems)
    [[nodiscard]] virtual engine_context* get_context() noexcept = 0;

    /// Get the engine settings interface
    [[nodiscard]] virtual i_engine_settings* get_settings() noexcept = 0;
};

} // namespace euengine