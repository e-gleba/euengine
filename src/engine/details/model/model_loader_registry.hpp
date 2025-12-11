#pragma once

#include "model_loader.hpp"

#include <memory>
#include <vector>

namespace euengine
{
class ModelLoaderRegistry final
{
public:
    ModelLoaderRegistry();

    ~ModelLoaderRegistry() = default;

    ModelLoaderRegistry(const ModelLoaderRegistry&)            = delete;
    ModelLoaderRegistry& operator=(const ModelLoaderRegistry&) = delete;
    ModelLoaderRegistry(ModelLoaderRegistry&&)                 = default;
    ModelLoaderRegistry& operator=(ModelLoaderRegistry&&)      = default;

    void register_loader(std::unique_ptr<IModelLoader> loader);

    [[nodiscard]] const IModelLoader* find_loader(
        const std::filesystem::path& path) const;

    [[nodiscard]] load_result load(const std::filesystem::path& path) const;

    [[nodiscard]] std::vector<std::string_view> supported_extensions() const;

private:
    std::vector<std::unique_ptr<IModelLoader>> loaders;
};

[[nodiscard]] inline ModelLoaderRegistry& get_model_loader_registry()
{
    static ModelLoaderRegistry registry;
    return registry;
}

} // namespace euengine