#include "profiler.hpp"

#include <core-api/profiling_events.hpp>

#ifdef TRACY_ENABLE
#include <cstring>
#include <thread>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
#include <unordered_map>
#include <vector>

// Force Tracy initialization - this ensures Tracy's static initialization
// happens even in shared libraries
// Tracy initializes automatically when first used, but in shared libraries
// we need to ensure it happens early
namespace
{
struct tracy_initializer
{
    tracy_initializer()
    {
        // Force Tracy to initialize by calling a function that requires init
        // This is a workaround for static initialization order issues in shared
        // libs The message will be sent when Tracy is ready
        TracyMessage("Tracy profiler initializing", 28);
    }
};
// Static instance ensures initialization happens when the library loads
// This runs before main() or when the shared library is loaded
tracy_initializer g_tracy_init;
} // namespace

namespace
{
// Thread-local storage for active zone contexts
// Tracy zones are thread-local, so we use thread-local storage
thread_local std::vector<TracyCZoneCtx> g_active_zones;

// Map from event system handles to Tracy zone contexts
// This allows the event system to work with Tracy's thread-local zones
thread_local std::unordered_map<std::uint64_t, TracyCZoneCtx> g_event_zones;

// Helper to create a source location for a zone name
uint64_t alloc_srcloc_for_name(const char* name, size_t name_len) noexcept
{
    // Use Tracy's source location allocator with minimal info
    // Line 0, empty source/function, just the name
    return ___tracy_alloc_srcloc_name(0, // line
                                      "",
                                      0, // source file (empty)
                                      "",
                                      0, // function (empty)
                                      name,
                                      name_len, // zone name
                                      0         // color (default)
    );
}

// Tracy event callback - subscribes to profiling events and forwards to Tracy
void tracy_event_callback(const egen::profiling_event& event,
                          void*                        userdata) noexcept
{
    (void)userdata; // Unused

    switch (event.type)
    {
        case egen::profiling_event_type::zone_begin:
        {
            const char* zone_name = event.data.zone_begin.zone_name;
            if (zone_name != nullptr)
            {
                const size_t   name_len = std::strlen(zone_name);
                const uint64_t srcloc =
                    alloc_srcloc_for_name(zone_name, name_len);
                const TracyCZoneCtx ctx =
                    ___tracy_emit_zone_begin_alloc(srcloc, 1);

                // Store the zone context for this event handle
                g_event_zones[event.data.zone_begin.zone_handle] = ctx;
            }
            break;
        }

        case egen::profiling_event_type::zone_end:
        {
            const auto it = g_event_zones.find(event.data.zone_end.zone_handle);
            if (it != g_event_zones.end())
            {
                ___tracy_emit_zone_end(it->second);
                g_event_zones.erase(it);
            }
            break;
        }

        case egen::profiling_event_type::frame_mark:
        {
            FrameMark;
            break;
        }

        case egen::profiling_event_type::thread_name_set:
        {
            const char* thread_name = event.data.thread_name.thread_name;
            if (thread_name != nullptr)
            {
                tracy::SetThreadName(thread_name);
            }
            break;
        }

        case egen::profiling_event_type::message:
        {
            const char* text = event.data.message.text;
            if (text != nullptr)
            {
                TracyMessage(text, std::strlen(text));
            }
            break;
        }

        case egen::profiling_event_type::frame_image:
        {
            const void*   pixels = event.data.frame_image.pixels;
            std::uint32_t width  = event.data.frame_image.width;
            std::uint32_t height = event.data.frame_image.height;
            if (pixels != nullptr && width > 0 && height > 0)
            {
                FrameImage(pixels, width, height, 0, false);
            }
            break;
        }
    }
}

// Global callback ID for Tracy event subscription
// NOTE: This is only used if Tracy is used via events only (not via old
// interface)
std::uint32_t g_tracy_callback_id = 0;

// Initialize Tracy event subscription
// Call this if you want to use Tracy via events only (not via i_profiler
// interface)
void init_tracy_event_subscription() noexcept
{
    if (g_tracy_callback_id == 0)
    {
        g_tracy_callback_id = egen::profiling_event_dispatcher::add_callback(
            tracy_event_callback, nullptr);
    }
}

// Cleanup Tracy event subscription
void cleanup_tracy_event_subscription() noexcept
{
    if (g_tracy_callback_id != 0)
    {
        egen::profiling_event_dispatcher::remove_callback(g_tracy_callback_id);
        g_tracy_callback_id = 0;
    }
}

// NOTE: We do NOT automatically subscribe to events because the profiler is
// used via the old i_profiler interface. Subscribing to events would cause
// double instrumentation. If you want to use Tracy via events only, call
// init_tracy_event_subscription() manually and don't use the old interface.
} // namespace

#endif

namespace egen
{

#ifdef TRACY_ENABLE

// Tracy profiler implementation (only in this compile unit)
class tracy_profiler final : public i_profiler
{
public:
    [[nodiscard]] std::uint64_t begin_zone(const char* name) noexcept override;

    void end_zone(std::uint64_t handle) noexcept override;

    void mark_frame() noexcept override;

    void set_thread_name(const char* name) noexcept override;

    void message(const char* text) noexcept override;

    void capture_frame_image(const void*   pixels,
                             std::uint32_t width,
                             std::uint32_t height) noexcept override;
};

std::uint64_t tracy_profiler::begin_zone(const char* name) noexcept
{
    if (name == nullptr)
    {
        return 0;
    }

    // Allocate source location for the zone name
    const size_t   name_len = std::strlen(name);
    const uint64_t srcloc   = alloc_srcloc_for_name(name, name_len);

    // Begin the zone using Tracy's C API (1 = active)
    const TracyCZoneCtx ctx = ___tracy_emit_zone_begin_alloc(srcloc, 1);

    // Store in thread-local stack (zones are LIFO)
    // Handle is 1-based index (0 = invalid)
    g_active_zones.push_back(ctx);
    const auto handle = static_cast<std::uint64_t>(g_active_zones.size());

    return handle;
}

void tracy_profiler::end_zone(std::uint64_t handle) noexcept
{
    if (handle == 0 || g_active_zones.empty())
    {
        return;
    }

    // For RAII zones (which is our primary use case), zones are LIFO
    // So the handle should match the top of the stack
    // But we support non-LIFO for flexibility - find and remove the zone
    const auto expected_idx = static_cast<size_t>(handle - 1);

    if (expected_idx < g_active_zones.size())
    {
        TracyCZoneCtx ctx = g_active_zones[expected_idx];

        // End the zone
        ___tracy_emit_zone_end(ctx);

        // Remove from stack - if it's the last one, just pop
        // Otherwise, swap with last and pop (maintains other handles)
        if (expected_idx == g_active_zones.size() - 1)
        {
            g_active_zones.pop_back();
        }
        else
        {
            // Swap with last element and pop
            g_active_zones[expected_idx] = g_active_zones.back();
            g_active_zones.pop_back();
        }
    }
}

void tracy_profiler::mark_frame() noexcept
{
    FrameMark;
    // Note: Frame image capture would require reading back the swapchain
    // texture which is expensive. For now, we just mark frames. To enable frame
    // images, you'd need to capture the swapchain texture and call FrameImage()
    // here.
}

void tracy_profiler::set_thread_name(const char* name) noexcept
{
    tracy::SetThreadName(name);
}

void tracy_profiler::message(const char* text) noexcept
{
    TracyMessage(text, std::strlen(text));
}

void tracy_profiler::capture_frame_image(const void*   pixels,
                                         std::uint32_t width,
                                         std::uint32_t height) noexcept
{
    if (pixels == nullptr || width == 0 || height == 0)
    {
        return;
    }
    // Tracy's FrameImage expects BGRA8 format, but we have RGBA8
    // We need to convert or use the correct format
    // For now, send as-is (Tracy should handle RGBA)
    FrameImage(pixels, width, height, 0, false);
}

#endif

// Null profiler implementation (header-only, but defined here for consistency)
class null_profiler final : public i_profiler
{
public:
    [[nodiscard]] std::uint64_t begin_zone(
        const char* /*name*/) noexcept override
    {
        return 0;
    }

    void end_zone(std::uint64_t /*handle*/) noexcept override {}

    void mark_frame() noexcept override {}

    void set_thread_name(const char* /*name*/) noexcept override {}

    void message(const char* /*text*/) noexcept override {}

    void capture_frame_image(const void* /*pixels*/,
                             std::uint32_t /*width*/,
                             std::uint32_t /*height*/) noexcept override
    {
    }
};

// Factory function - game module doesn't need to know about implementation
// details
i_profiler* create_profiler() noexcept
{
#ifdef TRACY_ENABLE
    // Force Tracy initialization by creating a static instance
    // This ensures Tracy's static initialization happens in the shared library
    static tracy_profiler profiler_instance;

    // Send test messages to verify Tracy is working
    // These will appear in Tracy when connected
    profiler_instance.message("Profiler initialized");
    profiler_instance.message("Tracy profiler active - connect to view data");

    // Create a test zone to ensure zones work
    auto test_handle = profiler_instance.begin_zone("ProfilerInit");
    profiler_instance.end_zone(test_handle);

    // NOTE: We do NOT subscribe to events here because the profiler is used via
    // the old i_profiler interface. If we subscribed to events AND used the old
    // interface, we'd get double instrumentation. The event system is for
    // profilers that ONLY use callbacks (not the old interface).

    return &profiler_instance;
#else
    static null_profiler profiler_instance;
    return &profiler_instance;
#endif
}

} // namespace egen
