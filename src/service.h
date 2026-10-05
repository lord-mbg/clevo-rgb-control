#pragma once

#include <filesystem>

int start_background(const std::filesystem::path& device_dir, int argc, char** argv);
int stop_background();
bool notify_effect_ready();
