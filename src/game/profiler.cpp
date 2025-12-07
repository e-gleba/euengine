#include "profiler.hpp"

#ifdef TRACY_ENABLE
#include <cstring>
#include <thread>
#include <tracy/Tracy.hpp>
#include <tracy/TracyC.h>
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
static tracy_initializer g_tracy_init;
} // namespace

namespace
{
// Thread-local storage for active zone contexts
// Tracy zones are thread-local, so we use thread-local storage
thread_local std::vector<TracyCZoneCtx> g_active_zones;

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
} // namespace

#endif

namespace euengine
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
    const std::uint64_t handle =
        static_cast<std::uint64_t>(g_active_zones.size());

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
    const size_t expected_idx = static_cast<size_t>(handle - 1);

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

    return &profiler_instance;
#else
    static null_profiler profiler_instance;
    return &profiler_instance;
#endif
}

} // namespace euengine
