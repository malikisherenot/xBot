#pragma once

#include "src/core/Bot.hpp"

#include <filesystem>
#include <string>

namespace xbot::formats {

bool supported(std::filesystem::path const& path);
bool load(std::filesystem::path const& path, Macro& out);
bool save(std::filesystem::path const& path, Macro const& macro, int levelId, std::string const& levelName);

}
