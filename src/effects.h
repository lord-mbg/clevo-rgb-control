#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

struct EffectSettings {
    std::string name;
    std::array<int, 3> from{};
    bool has_from{false};
    std::optional<std::array<int, 3>> to;
    std::vector<std::array<int, 3>> palette;
    std::optional<unsigned int> brightness;
    unsigned int period_ms{3000};
    unsigned int duration_ms{0};
};

int run_effect(const std::filesystem::path& device_dir, const EffectSettings& settings);
int validate_effect(const std::filesystem::path& device_dir, const EffectSettings& settings);
void print_palette(std::ostream& out);
