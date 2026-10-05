#include "profiles.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr std::string_view kHeader = "clevo-rgb-profile-v1";
constexpr std::size_t kMaxSize = 8192;

struct Descriptor {
    int fd;
    explicit Descriptor(int value) : fd(value) {}
    ~Descriptor() { if (fd >= 0) ::close(fd); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
};

struct Store {
    std::filesystem::path directory;
    uid_t owner;
    bool sudo_user;
};

bool valid_name(std::string_view name) {
    const auto alnum = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };
    if (name.empty() || name.size() > 64 || !alnum(name.front()) ||
        !std::all_of(name.begin(), name.end(), [&](char c) { return alnum(c) || c == '-' || c == '_'; })) {
        std::cerr << "Profile names must be 1..64 ASCII letters, digits, '-' or '_', starting with a letter or digit.\n";
        return false;
    }
    return true;
}

std::optional<Store> locate_store() {
    uid_t owner = ::geteuid();
    bool sudo_user = false;
    if (owner == 0) {
        if (const char* text = std::getenv("SUDO_UID")) {
            const char* end = text + std::strlen(text);
            const auto result = std::from_chars(text, end, owner);
            if (result.ec != std::errc{} || result.ptr != end) {
                std::cerr << "Invalid SUDO_UID; cannot select profile owner.\n";
                return std::nullopt;
            }
            sudo_user = owner != 0;
        }
    }
    const passwd* account = ::getpwuid(owner);
    if (!account) {
        std::cerr << "Cannot find profile owner's home directory.\n";
        return std::nullopt;
    }
    std::filesystem::path home = account->pw_dir;
    if (!sudo_user) {
        if (const char* value = std::getenv("HOME"); value && std::filesystem::path(value).is_absolute()) home = value;
    }
    std::filesystem::path config = home / ".config";
    if (const char* value = std::getenv("XDG_CONFIG_HOME"); value && std::filesystem::path(value).is_absolute()) config = value;
    return Store{config / "clevo-rgb" / "profiles", owner, sudo_user};
}

bool writable_store(const Store& store) {
    if (store.sudo_user) {
        std::cerr << "Save/delete profiles without sudo; sudo is only needed to apply keyboard settings.\n";
        return false;
    }
    return true;
}

int open_directory(const Store& store, bool create, bool& missing) {
    missing = false;
    if (create) {
        std::error_code error;
        std::filesystem::create_directories(store.directory.parent_path(), error);
        if (error) { std::cerr << "Cannot create profile directory: " << error.message() << '\n'; return -1; }
        if (::mkdir(store.directory.c_str(), 0700) < 0 && errno != EEXIST) {
            std::cerr << "Cannot create profile directory: " << std::strerror(errno) << '\n';
            return -1;
        }
    }
    const int fd = ::open(store.directory.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        missing = errno == ENOENT;
        if (!missing) std::cerr << "Cannot open profile directory: " << std::strerror(errno) << '\n';
        return -1;
    }
    struct stat info {};
    if (::fstat(fd, &info) < 0 || info.st_uid != store.owner) {
        std::cerr << "Profile directory must be owned by the profile user.\n";
        ::close(fd);
        return -1;
    }
    return fd;
}

// The CLI performs range/combination validation. This boundary excludes commands,
// paths, arbitrary text, and recursive profile loads even when a file is edited.
int arity(std::string_view option) {
    if (option == "--color" || option == "--to-color" || option == "--palette") return 3;
    if (option == "--effect" || option == "--brightness" || option == "--period-ms" || option == "--duration-ms") return 1;
    return 0;
}

bool safe_arguments(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    for (std::size_t i = 0; i < args.size();) {
        const auto& option = args[i];
        const int count = arity(option);
        if (count == 0 || args.size() - i - 1 < static_cast<std::size_t>(count)) return false;
        for (int j = 1; j <= count; ++j) {
            const auto& value = args[i + static_cast<std::size_t>(j)];
            if (option == "--effect") {
                if (value != "breathe" && value != "cycle" && value != "transition") return false;
            } else if (value.empty() || !std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; })) {
                return false;
            }
        }
        i += static_cast<std::size_t>(count) + 1;
    }
    return true;
}

bool owned_file(int fd, uid_t owner) {
    struct stat info {};
    return ::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_uid == owner &&
           info.st_size >= 0 && static_cast<unsigned long long>(info.st_size) <= kMaxSize;
}
}  // namespace

bool save_profile(std::string_view name, const std::vector<std::string>& arguments) {
    if (!valid_name(name)) return false;
    if (!safe_arguments(arguments)) { std::cerr << "Invalid profile settings.\n"; return false; }
    const auto store = locate_store();
    if (!store || !writable_store(*store)) return false;
    bool missing;
    Descriptor directory(open_directory(*store, true, missing));
    if (directory.fd < 0) return false;
    const std::string filename = std::string(name) + ".profile";
    const std::string temporary = "." + filename + ".tmp-" + std::to_string(::getpid());
    Descriptor file(::openat(directory.fd, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
    if (file.fd < 0) { std::cerr << "Cannot create profile: " << std::strerror(errno) << '\n'; return false; }
    std::string content(kHeader);
    content += '\n';
    for (std::size_t i = 0; i < arguments.size();) {
        const int count = arity(arguments[i]);
        content += arguments[i];
        for (int j = 1; j <= count; ++j) { content += ' '; content += arguments[i + static_cast<std::size_t>(j)]; }
        content += '\n';
        i += static_cast<std::size_t>(count) + 1;
    }
    bool success = content.size() <= kMaxSize;
    std::size_t written = 0;
    while (success && written < content.size()) {
        const ssize_t size = ::write(file.fd, content.data() + written, content.size() - written);
        if (size < 0 && errno == EINTR) continue;
        if (size <= 0) { success = false; break; }
        written += static_cast<std::size_t>(size);
    }
    if (success) success = ::fsync(file.fd) == 0;
    if (success) success = ::renameat(directory.fd, temporary.c_str(), directory.fd, filename.c_str()) == 0;
    if (!success) {
        ::unlinkat(directory.fd, temporary.c_str(), 0);
        std::cerr << "Failed to save profile; previous record left unchanged.\n";
        return false;
    }
    if (::fsync(directory.fd) < 0) { std::cerr << "Profile saved, but directory sync failed.\n"; return false; }
    return true;
}

std::optional<std::vector<std::string>> load_profile(std::string_view name) {
    if (!valid_name(name)) return std::nullopt;
    const auto store = locate_store();
    if (!store) return std::nullopt;
    bool missing;
    Descriptor directory(open_directory(*store, false, missing));
    if (directory.fd < 0) {
        if (missing) std::cerr << "Profile not found.\n";
        return std::nullopt;
    }
    const std::string filename = std::string(name) + ".profile";
    Descriptor file(::openat(directory.fd, filename.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    if (file.fd < 0 || !owned_file(file.fd, store->owner)) {
        std::cerr << "Cannot read profile: expected a regular, user-owned file of at most 8 KiB (no symlinks).\n";
        return std::nullopt;
    }
    std::string content;
    char buffer[1024];
    for (;;) {
        const ssize_t size = ::read(file.fd, buffer, sizeof(buffer));
        if (size < 0 && errno == EINTR) continue;
        if (size < 0) { std::cerr << "Cannot read profile.\n"; return std::nullopt; }
        if (size == 0) break;
        content.append(buffer, static_cast<std::size_t>(size));
        if (content.size() > kMaxSize) { std::cerr << "Profile exceeds 8 KiB.\n"; return std::nullopt; }
    }
    const std::string_view text(content);
    const std::size_t header_end = text.find('\n');
    if (header_end == std::string_view::npos || text.substr(0, header_end) != kHeader) {
        std::cerr << "Unsupported profile format.\n";
        return std::nullopt;
    }
    std::vector<std::string> args;
    std::size_t position = header_end + 1;
    while ((position = text.find_first_not_of(" \t\r\n\v\f", position)) != std::string_view::npos) {
        const std::size_t end = text.find_first_of(" \t\r\n\v\f", position);
        args.emplace_back(text.substr(position, end == std::string_view::npos ? end : end - position));
        if (end == std::string_view::npos) break;
        position = end;
    }
    if (!safe_arguments(args)) { std::cerr << "Unsupported or incomplete profile settings.\n"; return std::nullopt; }
    return args;
}

std::optional<std::vector<std::string>> list_profiles() {
    const auto store = locate_store();
    if (!store) return std::nullopt;
    bool missing;
    const int fd = open_directory(*store, false, missing);
    if (fd < 0) return missing ? std::optional<std::vector<std::string>>(std::vector<std::string>{}) : std::nullopt;
    DIR* directory = ::fdopendir(fd);
    if (!directory) { ::close(fd); std::cerr << "Cannot list profiles.\n"; return std::nullopt; }
    std::vector<std::string> names;
    for (;;) {
        errno = 0;
        const dirent* entry = ::readdir(directory);
        if (!entry) {
            const int error = errno;
            ::closedir(directory);
            if (error) { std::cerr << "Cannot enumerate profiles.\n"; return std::nullopt; }
            break;
        }
        const std::string_view filename(entry->d_name);
        constexpr std::string_view suffix = ".profile";
        if (filename.size() <= suffix.size() || filename.substr(filename.size() - suffix.size()) != suffix) continue;
        if (filename.front() == '.') continue;
        struct stat info {};
        if (::fstatat(fd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) == 0 && S_ISREG(info.st_mode) && info.st_uid == store->owner)
            names.emplace_back(filename.substr(0, filename.size() - suffix.size()));
    }
    std::sort(names.begin(), names.end());
    return names;
}

bool delete_profile(std::string_view name) {
    if (!valid_name(name)) return false;
    const auto store = locate_store();
    if (!store || !writable_store(*store)) return false;
    bool missing;
    Descriptor directory(open_directory(*store, false, missing));
    if (directory.fd < 0) { if (missing) std::cerr << "Profile not found.\n"; return false; }
    const std::string filename = std::string(name) + ".profile";
    struct stat info {};
    if (::fstatat(directory.fd, filename.c_str(), &info, AT_SYMLINK_NOFOLLOW) < 0 ||
        !S_ISREG(info.st_mode) || info.st_uid != store->owner) {
        std::cerr << "Profile not found or not a regular user-owned file.\n";
        return false;
    }
    if (::unlinkat(directory.fd, filename.c_str(), 0) < 0 || ::fsync(directory.fd) < 0) {
        std::cerr << "Cannot delete profile: " << std::strerror(errno) << '\n';
        return false;
    }
    return true;
}
