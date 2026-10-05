#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Profiles contain only lighting arguments, never device paths or management commands.
bool save_profile(std::string_view name, const std::vector<std::string>& arguments);
std::optional<std::vector<std::string>> load_profile(std::string_view name);
std::optional<std::vector<std::string>> list_profiles();
bool delete_profile(std::string_view name);
