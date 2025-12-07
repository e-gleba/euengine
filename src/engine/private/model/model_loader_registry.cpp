#include "model_loader_registry.hpp"
#include "gltf_loader.hpp"

#include <algorithm>
#include <memory>
#include <ranges>
#include <vector>

euengine::ModelLoaderRegistry::ModelLoaderRegistry()
{
    register_loader(std::make_unique<gltf_loader>());
}

void euengine::ModelLoaderRegistry::register_loader(
    std::unique_ptr<IModelLoader> loader)
{
    if (loader)
        loaders.push_back(std::move(loader));
}

[[nodiscard]] const euengine::IModelLoader*
euengine::ModelLoaderRegistry::find_loader(
    const std::filesystem::path& path) const
{
    auto ext = path.extension().string();
    std::ranges::transform(ext, ext.begin(), ::tolower);

    for (const auto& loader : loaders)
    {
        if (loader->supports(ext))
            return loader.get();
    }
    return nullptr;
}

[[nodiscard]] euengine::load_result euengine::ModelLoaderRegistry::load(
    const std::filesystem::path& path) const
{
    const auto* loader = find_loader(path);
    if (!loader)
    {
        return std::unexpected("no loader found for extension: " +
                               path.extension().string());
    }
    return loader->load(path);
}

[[nodiscard]] std::vector<std::string_view>
euengine::ModelLoaderRegistry::supported_extensions() const
{
    std::vector<std::string_view> result;
    for (const auto& loader : loaders)
    {
        auto exts = loader->extensions();
        result.insert(result.end(), exts.begin(), exts.end());
    }
    return result;
}
