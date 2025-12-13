#pragma once

#include <core-api/model_loader.hpp>

#include <filesystem>
#include <memory>

namespace euengine
{

/// Model loading system - handles model loading with format-specific loaders
/// Provides a clean interface without exposing implementation details
class model_system
{
public:
    model_system();
    ~model_system();

    model_system(const model_system&)            = delete;
    model_system& operator=(const model_system&) = delete;
    model_system(model_system&&)                 = delete;
    model_system& operator=(model_system&&)      = delete;

    /// Load a model from file
    /// @param path Path to model file
    /// @return Loaded model data on success, error message on failure
    [[nodiscard]] load_result load(const std::filesystem::path& path) const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace euengine
