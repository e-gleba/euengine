#pragma once

#include <cstdint>
#include <string_view>

namespace euengine
{

/// Abstract profiling interface - engine uses this without knowing
/// implementation Thread-safe operations for multi-threaded profiling
class i_profiler
{
public:
    virtual ~i_profiler() = default;

    /// Begin a profiling zone (scope)
    /// Returns a handle that must be passed to end_zone()
    /// Thread-safe
    [[nodiscard]] virtual std::uint64_t begin_zone(
        const char* name) noexcept = 0;

    /// End a profiling zone
    /// Thread-safe
    virtual void end_zone(std::uint64_t handle) noexcept = 0;

    /// Mark the end of a frame (for frame-based profilers)
    /// Thread-safe
    virtual void mark_frame() noexcept = 0;

    /// Set a thread name for profiling
    /// Thread-safe
    virtual void set_thread_name(const char* name) noexcept = 0;

    /// Allocate a message/event (for profilers that support it)
    /// Thread-safe
    virtual void message(const char* text) noexcept = 0;

    /// Capture frame image from GPU texture (for frame visualization)
    /// @param pixels RGBA8 pixel data (width * height * 4 bytes)
    /// @param width Image width in pixels
    /// @param height Image height in pixels
    /// Thread-safe
    virtual void capture_frame_image(const void*   pixels,
                                     std::uint32_t width,
                                     std::uint32_t height) noexcept = 0;
};

/// RAII helper for profiling zones
/// Automatically ends the zone when it goes out of scope
class profiler_zone final
{
public:
    profiler_zone(i_profiler* profiler, const char* name) noexcept
        : profiler_(profiler)
        , handle_(profiler_ ? profiler_->begin_zone(name) : 0)
    {
    }

    ~profiler_zone() noexcept
    {
        if (profiler_ && handle_ != 0)
        {
            profiler_->end_zone(handle_);
        }
    }

    // Non-copyable, movable
    profiler_zone(const profiler_zone&)            = delete;
    profiler_zone& operator=(const profiler_zone&) = delete;
    profiler_zone(profiler_zone&& other) noexcept
        : profiler_(other.profiler_)
        , handle_(other.handle_)
    {
        other.profiler_ = nullptr;
        other.handle_   = 0;
    }
    profiler_zone& operator=(profiler_zone&& other) noexcept
    {
        if (this != &other)
        {
            if (profiler_ && handle_ != 0)
            {
                profiler_->end_zone(handle_);
            }
            profiler_       = other.profiler_;
            handle_         = other.handle_;
            other.profiler_ = nullptr;
            other.handle_   = 0;
        }
        return *this;
    }

private:
    i_profiler*   profiler_ = nullptr;
    std::uint64_t handle_   = 0;
};

/// Macro helper for creating zones (similar to Tracy's ZoneScoped)
/// Usage: PROFILER_ZONE(profiler, "MyZoneName");
#define PROFILER_ZONE(profiler, name)                                          \
    ::euengine::profiler_zone _profiler_zone(profiler, name)

} // namespace euengine
