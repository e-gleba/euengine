#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace egen
{

/// Profiling event types - similar to SDL3 event system
enum class profiling_event_type : std::uint8_t
{
    zone_begin,      ///< A profiling zone has begun
    zone_end,        ///< A profiling zone has ended
    frame_mark,      ///< End of frame marker
    thread_name_set, ///< Thread name was set
    message,         ///< Profiling message/event
    frame_image,     ///< Frame image capture
};

/// Zone begin event data
struct profiling_zone_begin_event final
{
    const char*   zone_name;   ///< Name of the zone
    std::uint64_t zone_handle; ///< Unique handle for this zone instance
};

/// Zone end event data
struct profiling_zone_end_event final
{
    std::uint64_t zone_handle; ///< Handle of the zone ending
};

/// Thread name event data
struct profiling_thread_name_event final
{
    const char* thread_name; ///< Name of the thread
};

/// Message event data
struct profiling_message_event final
{
    const char* text; ///< Message text
};

/// Frame image event data
struct profiling_frame_image_event final
{
    const void*   pixels; ///< RGBA8 pixel data
    std::uint32_t width;  ///< Image width
    std::uint32_t height; ///< Image height
};

/// Union of all profiling event data
union profiling_event_data
{
    profiling_zone_begin_event  zone_begin;
    profiling_zone_end_event    zone_end;
    profiling_thread_name_event thread_name;
    profiling_message_event     message;
    profiling_frame_image_event frame_image;
};

/// Profiling event structure
struct profiling_event final
{
    profiling_event_type type; ///< Type of event
    profiling_event_data data; ///< Event-specific data
    std::uint64_t timestamp;   ///< Optional timestamp (0 = use current time)
};

/// Callback function type for profiling events
/// @param event The profiling event
/// @param userdata User-provided data pointer
using profiling_event_callback = void (*)(const profiling_event& event,
                                          void*                  userdata);

/// Callback entry for internal storage
struct profiling_callback_entry final
{
    profiling_event_callback callback = nullptr;
    void*                    userdata = nullptr;
    std::uint32_t            id       = 0;
};

/// Profiling event dispatcher - manages callbacks and emits events
/// Similar to SDL3's event system, but for profiling
/// Thread-safe for concurrent callback registration/removal and event emission
class profiling_event_dispatcher
{
private:
    // Static storage for callbacks (similar to SDL3's event system)
    static constexpr std::size_t           k_max_callbacks = 32;
    inline static profiling_callback_entry callbacks_[k_max_callbacks] {};
    inline static std::uint32_t            next_callback_id_ = 0;
    inline static std::mutex               callbacks_mutex_ {};

public:
    /// Add a callback to receive profiling events
    /// @param callback Function to call when events occur
    /// @param userdata User data to pass to callback
    /// @return ID that can be used to remove the callback, or 0 on failure
    [[nodiscard]] static std::uint32_t add_callback(
        profiling_event_callback callback, void* userdata) noexcept
    {
        if (callback == nullptr)
        {
            return 0;
        }

        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        // Find empty slot
        for (std::size_t i = 0; i < k_max_callbacks; ++i)
        {
            if (callbacks_[i].callback == nullptr)
            {
                callbacks_[i].callback = callback;
                callbacks_[i].userdata = userdata;
                callbacks_[i].id       = ++next_callback_id_;
                return callbacks_[i].id;
            }
        }

        return 0; // No free slots
    }

    /// Remove a callback by ID
    /// @param callback_id ID returned from add_callback
    static void remove_callback(std::uint32_t callback_id) noexcept
    {
        if (callback_id == 0)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        for (auto& callback : callbacks_)
        {
            if (callback.id == callback_id)
            {
                callback.callback = nullptr;
                callback.userdata = nullptr;
                callback.id       = 0;
                return;
            }
        }
    }

    /// Emit a profiling event to all registered callbacks
    /// @param event The event to emit
    static void emit_event(const profiling_event& event) noexcept
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        for (std::size_t i = 0; i < k_max_callbacks; ++i)
        {
            if (callbacks_[i].callback != nullptr)
            {
                callbacks_[i].callback(event, callbacks_[i].userdata);
            }
        }
    }

    /// Emit a zone begin event
    static void emit_zone_begin(const char*   zone_name,
                                std::uint64_t zone_handle) noexcept
    {
        profiling_event event {};
        event.type                        = profiling_event_type::zone_begin;
        event.data.zone_begin.zone_name   = zone_name;
        event.data.zone_begin.zone_handle = zone_handle;
        emit_event(event);
    }

    /// Emit a zone end event
    static void emit_zone_end(std::uint64_t zone_handle) noexcept
    {
        profiling_event event {};
        event.type                      = profiling_event_type::zone_end;
        event.data.zone_end.zone_handle = zone_handle;
        emit_event(event);
    }

    /// Emit a frame mark event
    static void emit_frame_mark() noexcept
    {
        profiling_event event {};
        event.type = profiling_event_type::frame_mark;
        emit_event(event);
    }

    /// Emit a thread name event
    static void emit_thread_name(const char* thread_name) noexcept
    {
        profiling_event event {};
        event.type = profiling_event_type::thread_name_set;
        event.data.thread_name.thread_name = thread_name;
        emit_event(event);
    }

    /// Emit a message event
    static void emit_message(const char* text) noexcept
    {
        profiling_event event {};
        event.type              = profiling_event_type::message;
        event.data.message.text = text;
        emit_event(event);
    }

    /// Emit a frame image event
    static void emit_frame_image(const void*   pixels,
                                 std::uint32_t width,
                                 std::uint32_t height) noexcept
    {
        profiling_event event {};
        event.type                    = profiling_event_type::frame_image;
        event.data.frame_image.pixels = pixels;
        event.data.frame_image.width  = width;
        event.data.frame_image.height = height;
        emit_event(event);
    }
};

} // namespace egen