#pragma once

#include <core-api/profiler.hpp>

namespace egen
{

/// Factory function to create a profiler instance
/// Returns nullptr if profiling is disabled, or a profiler instance if enabled
/// The caller is responsible for the lifetime of the returned profiler
/// This function is implemented in profiler.cpp (separate compile unit)
i_profiler* create_profiler() noexcept;

} // namespace egen
