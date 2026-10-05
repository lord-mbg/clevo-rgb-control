#include "effects.h"
#include "service.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/magic.h>
#include <string_view>
#include <sys/vfs.h>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopped = 0;
void stop_effect(int) { stopped = 1; }
struct PaletteColor {
    std::string_view name;
    std::array<int, 3> rgb;
};

constexpr PaletteColor kNominalPalette[] = {
    {"red", {255, 0, 0}},
    {"orange", {255, 128, 0}},
    {"yellow", {255, 255, 0}},
    {"chartreuse", {128, 255, 0}},
    {"green", {0, 255, 0}},
    {"spring green", {0, 255, 128}},
    {"cyan", {0, 255, 255}},
    {"azure", {0, 128, 255}},
    {"blue", {0, 0, 255}},
    {"violet", {128, 0, 255}},
    {"magenta", {255, 0, 255}},
    {"rose", {255, 0, 128}},
    {"white", {255, 255, 255}},
};

// Keep sysfs descriptors open; ordinary fixture files also need their length updated.
class Attribute {
public:
    explicit Attribute(const std::filesystem::path& path) : fd_(::open(path.c_str(), O_WRONLY | O_CLOEXEC)) {
        if (fd_ < 0) {
            std::cerr << "Cannot open " << path << ": " << std::strerror(errno) << ". Check write permissions.\n";
            return;
        }
        struct statfs info {};
        if (::fstatfs(fd_, &info) < 0) {
            std::cerr << "Cannot inspect " << path << ": " << std::strerror(errno) << '\n';
            ::close(fd_);
            fd_ = -1;
            return;
        }
        truncate_ = info.f_type != SYSFS_MAGIC;
    }
    ~Attribute() { if (fd_ >= 0) ::close(fd_); }
    Attribute(const Attribute&) = delete;
    Attribute& operator=(const Attribute&) = delete;
    bool valid() const { return fd_ >= 0; }
    bool set(const char* value, std::size_t size) {
        if (::lseek(fd_, 0, SEEK_SET) < 0) return failed();
        ssize_t written;
        do { written = ::write(fd_, value, size); } while (written < 0 && errno == EINTR);
        if (written < 0) return failed();
        if (static_cast<std::size_t>(written) != size) {
            std::cerr << "Incomplete LED attribute write.\n";
            return false;
        }
        if (truncate_ && ::ftruncate(fd_, static_cast<off_t>(size)) < 0) return failed();
        return true;
    }
    bool color(const std::array<int, 3>& rgb) {
        char buffer[32];
        const int size = std::snprintf(buffer, sizeof(buffer), "%d %d %d\n", rgb[0], rgb[1], rgb[2]);
        return set(buffer, static_cast<std::size_t>(size));
    }
    bool brightness(unsigned int value) {
        char buffer[32];
        const int size = std::snprintf(buffer, sizeof(buffer), "%u\n", value);
        return set(buffer, static_cast<std::size_t>(size));
    }
private:
    bool failed() const {
        std::cerr << "LED attribute write failed: " << std::strerror(errno) << '\n';
        return false;
    }
    int fd_;
    bool truncate_{false};
};
struct InitialLedState {
    std::array<int, 3> color{};
    unsigned int brightness{0};
    unsigned int max_brightness{0};
};

bool read_initial_state(const std::filesystem::path& device_dir, InitialLedState& state) {
    std::ifstream color_file(device_dir / "multi_intensity");
    std::ifstream brightness_file(device_dir / "brightness");
    std::ifstream maximum_file(device_dir / "max_brightness");
    if (!(color_file >> state.color[0] >> state.color[1] >> state.color[2]) ||
        !(brightness_file >> state.brightness) || !(maximum_file >> state.max_brightness) ||
        std::any_of(state.color.begin(), state.color.end(), [](int c) { return c < 0 || c > 255; }) ||
        state.brightness > state.max_brightness) {
        std::cerr << "Cannot read valid initial LED color, brightness, and maximum.\n";
        return false;
    }
    return true;
}

std::array<int, 3> rainbow(double phase) {
    const double hue = phase * 6.0;
    const int sector = static_cast<int>(hue);
    const int rising = static_cast<int>(std::lround((hue - sector) * 255.0));
    const int falling = 255 - rising;
    switch (sector) {
        case 0: return {255, rising, 0};
        case 1: return {falling, 255, 0};
        case 2: return {0, 255, rising};
        case 3: return {0, falling, 255};
        case 4: return {rising, 0, 255};
        default: return {255, 0, falling};
    }
}
}  // namespace
void print_palette(std::ostream& out) {
    for (const auto& entry : kNominalPalette) {
        out << entry.name << ": " << entry.rgb[0] << ' ' << entry.rgb[1] << ' ' << entry.rgb[2] << '\n';
    }
}

int validate_effect(const std::filesystem::path& device_dir, const EffectSettings& settings) {
    if (settings.period_ms == 0) {
        return 1;
    }
    InitialLedState state;
    if (!read_initial_state(device_dir, state)) {
        return 1;
    }
    const unsigned int peak = settings.brightness.value_or(state.brightness);
    if (peak > state.max_brightness) {
        std::cerr << "Brightness must be between 0 and " << state.max_brightness << ".\n";
        return 2;
    }
    Attribute color(device_dir / "multi_intensity");
    Attribute brightness(device_dir / "brightness");
    if (!color.valid() || !brightness.valid()) {
        return 1;
    }
    return 0;
}


int run_effect(const std::filesystem::path& device_dir, const EffectSettings& settings) {
    InitialLedState state;
    if (!read_initial_state(device_dir, state)) {
        return 1;
    }
    const unsigned int peak = settings.brightness.value_or(state.brightness);
    if (peak > state.max_brightness) {
        std::cerr << "Brightness must be between 0 and " << state.max_brightness << ".\n";
        return 2;
    }
    Attribute color(device_dir / "multi_intensity");
    Attribute brightness(device_dir / "brightness");
    if (!color.valid() || !brightness.valid()) return 1;

    const std::size_t palette_size = !settings.palette.empty() ? settings.palette.size()
                                   : settings.to ? 2 : settings.has_from ? 1
                                   : sizeof(kNominalPalette) / sizeof(kNominalPalette[0]);
    const auto palette_color = [&](std::size_t index) -> const std::array<int, 3>& {
        if (!settings.palette.empty()) return settings.palette[index];
        if (settings.to) return index == 0 ? settings.from : *settings.to;
        if (settings.has_from) return settings.from;
        return kNominalPalette[index].rgb;
    };

    stopped = 0;
    struct sigaction action {}, old_int {}, old_term {};
    action.sa_handler = stop_effect;
    ::sigemptyset(&action.sa_mask);
    if (::sigaction(SIGINT, &action, &old_int) < 0) {
        std::cerr << "Cannot install interrupt handler.\n";
        return 1;
    }
    if (::sigaction(SIGTERM, &action, &old_term) < 0) {
        ::sigaction(SIGINT, &old_int, nullptr);
        std::cerr << "Cannot install termination handler.\n";
        return 1;
    }

    std::cout << "Running " << settings.name << " (whole keyboard); Ctrl+C to stop.\n" << std::flush;
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    const auto deadline = settings.duration_ms ? start + std::chrono::milliseconds(settings.duration_ms)
                                              : Clock::time_point::max();
    const unsigned int period = settings.period_ms;
    auto next = start;
    auto last_color = state.color;
    auto last_brightness = state.brightness;
    bool success = true;

    if (settings.name == "breathe") {
        if (!brightness.brightness(0)) {
            success = false;
        }
        last_brightness = 0;
        const auto& first_color = palette_color(0);
        if (success && !color.color(first_color)) {
            success = false;
        }
        last_color = first_color;
    }
    if (success && !notify_effect_ready()) {
        std::cerr << "Cannot notify service manager that the effect is ready.\n";
        success = false;
    }

    while (!stopped && success && Clock::now() < deadline) {
        const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const double phase = std::fmod(elapsed / period, 1.0);
        auto rgb = settings.from;
        unsigned int level = peak;

        if (settings.name == "breathe") {
            constexpr double pi = 3.14159265358979323846;
            level = static_cast<unsigned int>(std::llround(peak * (1.0 - std::cos(2.0 * pi * phase)) / 2.0));
            const auto cycle = static_cast<std::size_t>(elapsed / period);
            rgb = palette_color(cycle % palette_size);

            if (rgb != last_color) {
                if (!brightness.brightness(0)) { success = false; break; }
                last_brightness = 0;
                if (!color.color(rgb)) { success = false; break; }
                last_color = rgb;
            }
            if (level != last_brightness) {
                if (!brightness.brightness(level)) { success = false; break; }
                last_brightness = level;
            }
        } else if (settings.name == "cycle") {
            rgb = rainbow(phase);
            if (rgb != last_color && !color.color(rgb)) { success = false; break; }
            if (level != last_brightness && !brightness.brightness(level)) { success = false; break; }
            last_color = rgb;
            last_brightness = level;
        } else {
            // One full period goes from the first color to the second and back.
            const double blend = 1.0 - std::abs(2.0 * phase - 1.0);
            for (std::size_t i = 0; i < rgb.size(); ++i)
                rgb[i] = static_cast<int>(std::lround(settings.from[i] + ((*settings.to)[i] - settings.from[i]) * blend));
            if (rgb != last_color && !color.color(rgb)) { success = false; break; }
            if (level != last_brightness && !brightness.brightness(level)) { success = false; break; }
            last_color = rgb;
            last_brightness = level;
        }

        next += std::chrono::milliseconds(40); // At most 25 frames/s; skip missed frames, never busy-spin.
        if (next <= Clock::now()) next = Clock::now() + std::chrono::milliseconds(40);
        std::this_thread::sleep_until(std::min(next, deadline));
    }

    const bool color_restored = color.color(state.color);
    const bool brightness_restored = brightness.brightness(state.brightness);
    ::sigaction(SIGINT, &old_int, nullptr);
    ::sigaction(SIGTERM, &old_term, nullptr);
    if (!color_restored || !brightness_restored) {
        std::cerr << "Could not restore previous LED settings.\n";
        return 1;
    }
    if (!success) return 1;
    std::cout << "Restored previous color and brightness.\n";
    return 0;
}
