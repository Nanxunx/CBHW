#include "turtle335/m2/WotlkM2Reader.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot open input M2");
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), {});
}

bool HasExternalAnim(const std::filesystem::path& m2)
{
    const auto parent = m2.parent_path();
    const auto stem = m2.stem().string();
    if (!std::filesystem::exists(parent))
        return false;
    for (const auto& entry : std::filesystem::directory_iterator(parent))
    {
        if (!entry.is_regular_file())
            continue;
        const auto p = entry.path();
        if (p.extension() == ".anim" && p.stem().string().rfind(stem, 0u) == 0u)
            return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: turtle335_probe_m2 <source.m2>\n";
        return 2;
    }
    try
    {
        const std::filesystem::path path(argv[1]);
        const auto model = turtle335::m2::ParseWotlkM2(ReadFile(path));
        const auto features = turtle335::m2::InspectM2Features(model);
        const bool externalAnim = HasExternalAnim(path);
        const auto gate = turtle335::m2::ClassifyM2(model, externalAnim);

        std::cout << "version=" << model.version << '\n'
                  << "animations=" << model.animations.count << '\n'
                  << "bones=" << model.bones.count << '\n'
                  << "vertices=" << model.vertices.count << '\n'
                  << "views=" << model.viewCount << '\n'
                  << "textures=" << model.textures.count << '\n'
                  << "texanims=" << model.textureAnimations.count << '\n'
                  << "events=" << model.events.count << '\n'
                  << "ribbons=" << model.ribbons.count << '\n'
                  << "particles=" << model.particles.count << '\n'
                  << "alias_sequences=" << (features.hasAliasSequences ? 1 : 0) << '\n'
                  << "subanimations=" << (features.hasSubAnimations ? 1 : 0) << '\n'
                  << "duplicate_animation_ids=" << (features.hasDuplicateAnimationIds ? 1 : 0) << '\n'
                  << "external_anim=" << (externalAnim ? 1 : 0) << '\n'
                  << "conversion_gate=" << turtle335::m2::ToString(gate) << '\n';
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "M2 probe error: " << e.what() << '\n';
        return 2;
    }
}
