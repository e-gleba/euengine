#pragma once

#include <core-api/model_loader.hpp>

#include <filesystem>
#include <memory>

namespace euengine
{

/// Model loading system - handles model loading with format-specific loaders
/// Provides a clean interface without exposing implementation details
class model_system final : public i_model_loader
{
public:
    model_system();
    ~model_system() override;

    model_system(const model_system&)            = delete;
    model_system& operator=(const model_system&) = delete;
    model_system(model_system&&)                 = delete;
    model_system& operator=(model_system&&)      = delete;

    /// Load a model from file
    /// @param path Path to model file
    /// @return Loaded model data on success, error message on failure
    [[nodiscard]] load_result load(
        const std::filesystem::path& path) const override;

    /// Check if this loader supports the given file extension
    [[nodiscard]] bool supports(std::string_view extension) const override;

    /// Get supported extensions for this loader
    [[nodiscard]] std::span<const std::string_view> extensions() const override;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace euengine
