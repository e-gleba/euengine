#include "model_system.hpp"
#include "gltf_loader.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>

namespace egen
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

    bool supports(std::string_view extension) const
    {
        return loader_ && loader_->supports(extension);
    }

    std::span<const std::string_view> extensions() const
    {
        if (!loader_)
        {
            return {};
        }
        return loader_->extensions();
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

bool model_system::supports(std::string_view extension) const
{
    return pimpl_->supports(extension);
}

std::span<const std::string_view> model_system::extensions() const
{
    return pimpl_->extensions();
}

} // namespace egen
