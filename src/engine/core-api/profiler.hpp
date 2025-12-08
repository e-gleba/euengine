#pragma once

#include "profiling_events.hpp"

#include <cstdint>

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
/// Also emits events to the profiling event system for callback-based profilers
class profiler_zone final
{
public:
    profiler_zone(i_profiler* profiler, const char* name) noexcept
        : profiler_(profiler)
        , old_handle_(0)
        , event_handle_(0)
    {
        if (name != nullptr)
        {
            if (profiler_ != nullptr)
            {
                // Use old interface - don't emit events to avoid double
                // instrumentation
                old_handle_ = profiler_->begin_zone(name);
            }
            else
            {
                // No profiler provided - use event system only
                // Generate unique handle for event system (thread-local
                // counter)
                static thread_local std::uint64_t zone_counter = 1;
                event_handle_                                  = zone_counter++;

                // Emit event for callback-based profilers
                profiling_event_dispatcher::emit_zone_begin(name,
                                                            event_handle_);
            }
        }
    }

    ~profiler_zone() noexcept
    {
        if (profiler_ != nullptr && old_handle_ != 0)
        {
            // Use old interface
            profiler_->end_zone(old_handle_);
        }
        else if (event_handle_ != 0)
        {
            // Use event system only
            profiling_event_dispatcher::emit_zone_end(event_handle_);
        }
    }

    // Non-copyable, movable
    profiler_zone(const profiler_zone&)            = delete;
    profiler_zone& operator=(const profiler_zone&) = delete;
    profiler_zone(profiler_zone&& other) noexcept
        : profiler_(other.profiler_)
        , old_handle_(other.old_handle_)
        , event_handle_(other.event_handle_)
    {
        other.profiler_     = nullptr;
        other.old_handle_   = 0;
        other.event_handle_ = 0;
    }
    profiler_zone& operator=(profiler_zone&& other) noexcept
    {
        if (this != &other)
        {
            // End current zone if active
            if (event_handle_ != 0)
            {
                profiling_event_dispatcher::emit_zone_end(event_handle_);
            }
            if (profiler_ != nullptr && old_handle_ != 0)
            {
                profiler_->end_zone(old_handle_);
            }

            // Move from other
            profiler_           = other.profiler_;
            old_handle_         = other.old_handle_;
            event_handle_       = other.event_handle_;
            other.profiler_     = nullptr;
            other.old_handle_   = 0;
            other.event_handle_ = 0;
        }
        return *this;
    }

private:
    i_profiler*   profiler_     = nullptr;
    std::uint64_t old_handle_   = 0; // Handle for old i_profiler interface
    std::uint64_t event_handle_ = 0; // Handle for event system
};

/// Helper function for creating zones (similar to Tracy's ZoneScoped)
/// Usage: auto zone = profiler_zone_begin(profiler, "MyZoneName");
/// This function works with both the old i_profiler interface and the new event
/// system
/// The returned profiler_zone will automatically end when it goes out of scope
[[nodiscard]] inline profiler_zone profiler_zone_begin(
    i_profiler* profiler, const char* name) noexcept
{
    return { profiler, name };
}

/// Helper function for creating zones using only the event system
/// Usage: auto zone = profiler_zone_begin_event("MyZoneName");
/// This is the preferred way for new code - profilers subscribe via callbacks
/// The returned profiler_zone will automatically end when it goes out of scope
[[nodiscard]] inline profiler_zone profiler_zone_begin_event(
    const char* name) noexcept
{
    return { nullptr, name };
}

} // namespace euengine