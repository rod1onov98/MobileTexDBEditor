#pragma once

#include "TextureDb.h"

#include <filesystem>

namespace imageio {

texdb::ImageRGBA LoadRGBA(const std::filesystem::path& file);
void SavePNG(const std::filesystem::path& file, const texdb::ImageRGBA& image);

} // namespace imageio
