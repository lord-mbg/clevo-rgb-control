#include "effects.h"
#include "service.h"

#include <cerrno>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace {

constexpr std::string_view kDefaultDeviceDir = "/sys/class/leds/rgb:kbd_backlight";

struct Options {
    std::filesystem::path device_dir{kDefaultDeviceDir};
    std::optional<int> red;
    std::optional<int> green;
    std::optional<int> blue;
    std::optional<unsigned int> brightness;
    EffectSettings effect;
    bool has_period{false};
    bool has_duration{false};
    bool show_help{false};
    bool foreground{false};
    bool stop{false};
    bool list_colors{false};
};

void print_usage(std::ostream& out) {
    out << "Usage: clevo-rgb [--device-dir PATH] [--color RED GREEN BLUE] [--brightness VALUE]\n"
        << "       clevo-rgb --effect breathe|cycle|transition [effect options]\n"
        << "\n"
        << "Color channels: 0..255; brightness: 0..max_brightness.\n"
        << "Effects start a detached systemd service by default; --stop restores and stops it.\n"
        << "--foreground: run attached to this terminal instead.\n"
        << "breathe: default palette, --color for one color, or --color + --to-color to alternate.\n"
        << "--palette R G B: repeat for 2..64 breathing colors; cannot combine with --color.\n"
        << "transition requires --color and --to-color; cycle uses the full rainbow.\n"
        << "--list-colors: print a coarse nominal RGB palette (not hardware calibration).\n"
        << "--period-ms N: full cycle/breath, 100..3600000 (default 3000).\n"
        << "--duration-ms N: stop after N milliseconds (default: until --stop).\n"
        << "Effects restore previous color/brightness on completion or graceful stop.\n"
        << "Default device: " << kDefaultDeviceDir << "\n"
        << "--device-dir is intended for alternate LED devices and testing.\n";
}

template <typename Integer>
bool parse_integer(std::string_view text, Integer& value) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value, 10);
    return result.ec == std::errc{} && result.ptr == end;
}

bool require_argument(int argc, int index, std::string_view option) {
    if (index >= argc) {
        std::cerr << "Missing value for " << option << ".\n";
        return false;
    }
    return true;
}

std::optional<Options> parse_options(int argc, char** argv) {
    Options options;
    bool has_setting = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            options.show_help = true;
            continue;
        }
        if (arg == "--foreground") { options.foreground = true; continue; }
        if (arg == "--stop") { options.stop = true; continue; }
        if (arg == "--list-colors") { options.list_colors = true; continue; }
        if (arg == "--device-dir") {
            if (!require_argument(argc, i + 1, arg)) return std::nullopt;
            options.device_dir = argv[++i];
            if (options.device_dir.empty()) {
                std::cerr << "Device directory must not be empty.\n";
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--color" || arg == "--to-color" || arg == "--palette") {
            if (i + 3 >= argc) {
                std::cerr << arg << " requires three channel values: RED GREEN BLUE.\n";
                return std::nullopt;
            }
            int red = 0;
            int green = 0;
            int blue = 0;
            if (!parse_integer(argv[i + 1], red) || !parse_integer(argv[i + 2], green) ||
                !parse_integer(argv[i + 3], blue) || red < 0 || red > 255 || green < 0 || green > 255 ||
                blue < 0 || blue > 255) {
                std::cerr << "RGB channel values must be integers from 0 to 255.\n";
                return std::nullopt;
            }
            if (arg == "--color") {
                options.red = red;
                options.green = green;
                options.blue = blue;
            } else if (arg == "--palette") {
                if (options.effect.palette.size() >= 64) {
                    std::cerr << "At most 64 palette colors are allowed.\n";
                    return std::nullopt;
                }
                options.effect.palette.push_back({red, green, blue});
            } else {
                options.effect.to = std::array<int, 3>{red, green, blue};
            }
            i += 3;
            has_setting = true;
            continue;
        }
        if (arg == "--brightness") {
            if (!require_argument(argc, i + 1, arg)) return std::nullopt;
            unsigned int brightness = 0;
            if (!parse_integer(argv[i + 1], brightness)) {
                std::cerr << "Brightness must be a non-negative integer.\n";
                return std::nullopt;
            }
            options.brightness = brightness;
            ++i;
            has_setting = true;
            continue;
        }
        if (arg == "--effect") {
            if (!require_argument(argc, i + 1, arg)) return std::nullopt;
            options.effect.name = argv[++i];
            if (options.effect.name != "breathe" && options.effect.name != "cycle" &&
                options.effect.name != "transition") {
                std::cerr << "Effect must be breathe, cycle, or transition.\n";
                return std::nullopt;
            }
            has_setting = true;
            continue;
        }
        if (arg == "--period-ms" || arg == "--duration-ms") {
            if (!require_argument(argc, i + 1, arg)) return std::nullopt;
            unsigned int value = 0;
            if (!parse_integer(argv[++i], value) || value == 0 ||
                (arg == "--period-ms" && (value < 100 || value > 3600000))) {
                std::cerr << "Invalid value for " << arg << ".\n";
                return std::nullopt;
            }
            if (arg == "--period-ms") {
                options.effect.period_ms = value;
                options.has_period = true;
            } else {
                options.effect.duration_ms = value;
                options.has_duration = true;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return std::nullopt;
    }

    if (!options.show_help) {
        const auto& effect = options.effect;
        if ((options.stop || options.list_colors) &&
            (has_setting || options.foreground || options.has_period || options.has_duration ||
             (options.stop && options.list_colors))) {
            std::cerr << "--stop and --list-colors must be used alone.\n";
            return std::nullopt;
        }
        if (effect.name.empty() &&
            (effect.to || !effect.palette.empty() || options.foreground || options.has_period || options.has_duration)) {
            std::cerr << "Effect options require --effect.\n";
            return std::nullopt;
        }
        if (effect.name == "transition" && !options.red) {
            std::cerr << "This effect requires --color RED GREEN BLUE.\n";
            return std::nullopt;
        }
        if ((effect.name == "transition" && !effect.to) ||
            (effect.name != "transition" && effect.name != "breathe" && effect.to) ||
            (effect.name == "breathe" && effect.to && !options.red) ||
            (effect.name == "cycle" && options.red)) {
            std::cerr << "transition requires --to-color; breathe --to-color requires --color; cycle accepts no colors.\n";
            return std::nullopt;
        }
        if (!effect.palette.empty() &&
            (effect.name != "breathe" || effect.palette.size() < 2 || options.red || effect.to)) {
            std::cerr << "Use 2..64 --palette colors with breathe, without --color or --to-color.\n";
            return std::nullopt;
        }
    }

    if (!options.show_help && !options.stop && !options.list_colors && !has_setting) {
        std::cerr << "Specify --color, --brightness, or --effect.\n";
        return std::nullopt;
    }
    if (options.red) options.effect.from = {*options.red, *options.green, *options.blue};
    options.effect.has_from = options.red.has_value();
    options.effect.brightness = options.brightness;
    return options;
}

bool can_write(const std::filesystem::path& path) {
    if (::access(path.c_str(), W_OK) == 0) {
        return true;
    }
    std::cerr << "Cannot write " << path << ": " << std::system_error(errno, std::generic_category()).what()
              << ". Try running this command with sudo.\n";
    return false;
}

bool write_value(const std::filesystem::path& path, const std::string& value) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Failed to open " << path << " for writing.\n";
        return false;
    }
    file << value << '\n';
    if (!file) {
        std::cerr << "Failed to write to " << path << ".\n";
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const auto parsed = parse_options(argc, argv);
    if (!parsed) {
        print_usage(std::cerr);
        return 2;
    }
    const Options& options = *parsed;
    if (options.show_help) {
        print_usage(std::cout);
        return 0;
    }
    if (options.list_colors) {
        print_palette(std::cout);
        return 0;
    }
    if (options.stop) {
        const int result = stop_background();
        if (result == 0) std::cout << "Background effect stopped.\n";
        return result;
    }
    if (!options.effect.name.empty()) {
        if (options.foreground) return run_effect(options.device_dir, options.effect);
        const int checked = validate_effect(options.device_dir, options.effect);
        if (checked != 0) return checked;
        return start_background(options.device_dir, argc, argv);
    }

    const auto color_path = options.device_dir / "multi_intensity";
    const auto brightness_path = options.device_dir / "brightness";
    const auto max_brightness_path = options.device_dir / "max_brightness";

    if (options.red && !can_write(color_path)) return 1;
    if (options.brightness && !can_write(brightness_path)) return 1;

    if (options.brightness) {
        std::ifstream max_file(max_brightness_path);
        unsigned int max_brightness = 0;
        std::string max_text;
        if (!max_file || !(max_file >> max_text) || !parse_integer(max_text, max_brightness)) {
            std::cerr << "Cannot read a valid max_brightness value from " << max_brightness_path << ".\n";
            return 1;
        }
        if (*options.brightness > max_brightness) {
            std::cerr << "Brightness must be between 0 and " << max_brightness << ".\n";
            return 2;
        }
    }

    // Check every requested destination before changing either setting.
    if (options.red && !std::filesystem::is_regular_file(color_path)) {
        std::cerr << "RGB control file not found: " << color_path << ".\n";
        return 1;
    }
    if (options.brightness && !std::filesystem::is_regular_file(brightness_path)) {
        std::cerr << "Brightness control file not found: " << brightness_path << ".\n";
        return 1;
    }
    // Stop the managed effect before static writes so it cannot overwrite them.
    if (stop_background() != 0) return 1;

    if (options.red) {
        const std::string color = std::to_string(*options.red) + " " + std::to_string(*options.green) + " " +
                                  std::to_string(*options.blue);
        if (!write_value(color_path, color)) return 1;
        std::cout << "Set keyboard color to RGB " << color << ".\n";
    }
    if (options.brightness) {
        if (!write_value(brightness_path, std::to_string(*options.brightness))) return 1;
        std::cout << "Set keyboard brightness to " << *options.brightness << ".\n";
    }
    return 0;
}
