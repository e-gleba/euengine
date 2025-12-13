#include "model_system.hpp"
#include "gltf_loader.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>

namespace euengine
{

// PIMPL implementation
class model_system::impl
{
public:
    impl()
    {
        // Register GLTF loader
        loader_ = std::make_unique<gltf_loader>();
    }

    load_result load(const std::filesystem::path& path) const
    {
        if (!loader_)
        {
            return std::unexpected("no model loader available");
        }

        // Check if file extension is supported
        auto ext = path.extension().string();
        std::ranges::transform(ext, ext.begin(), ::tolower);

        if (!loader_->supports(ext))
        {
            return std::unexpected("unsupported file extension: " + ext);
        }

        return loader_->load(path);
    }

private:
    std::unique_ptr<gltf_loader> loader_;
};

model_system::model_system()
    : pimpl_(std::make_unique<impl>())
{
}

model_system::~model_system() = default;

load_result model_system::load(const std::filesystem::path& path) const
{
    return pimpl_->load(path);
}

} // namespace euengine
