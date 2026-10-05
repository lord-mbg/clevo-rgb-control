#include "service.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr const char* kUnit = "clevo-rgb-effect.service";
bool user_scope() { return ::geteuid() != 0; }

struct Result {
    int code;
    std::string output;
};

Result command(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    int pipe_fds[2];
    if (::pipe(pipe_fds) < 0) return {1, std::strerror(errno)};
    const pid_t child = ::fork();
    if (child < 0) {
        const std::string error = std::strerror(errno);
        ::close(pipe_fds[0]); ::close(pipe_fds[1]);
        return {1, error};
    }
    if (child == 0) {
        ::close(pipe_fds[0]);
        if (::dup2(pipe_fds[1], STDOUT_FILENO) < 0 || ::dup2(pipe_fds[1], STDERR_FILENO) < 0) ::_exit(127);
        ::close(pipe_fds[1]);
        ::execvp(argv[0], argv.data());
        ::perror("Cannot execute service manager");
        ::_exit(127);
    }
    ::close(pipe_fds[1]);
    std::string output;
    char buffer[4096];
    for (;;) {
        const ssize_t size = ::read(pipe_fds[0], buffer, sizeof(buffer));
        if (size > 0) output.append(buffer, static_cast<std::size_t>(size));
        else if (size < 0 && errno == EINTR) continue;
        else break;
    }
    ::close(pipe_fds[0]);
    int status;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return {1, std::strerror(errno)};
    }
    return {WIFEXITED(status) ? WEXITSTATUS(status) : 1, std::move(output)};
}

std::vector<std::string> manager(const char* executable) {
    std::vector<std::string> args{executable};
    if (user_scope()) args.emplace_back("--user");
    args.emplace_back("--no-ask-password");
    return args;
}

int failed(const Result& result) {
    std::cerr << "systemd " << (user_scope() ? "user" : "system") << " manager: " << result.output << '\n';
    return 1;
}
}  // namespace

// Type=notify makes systemd-run wait for actual device initialization, not just exec.
bool notify_effect_ready() {
    const char* path = std::getenv("NOTIFY_SOCKET");
    if (!path) return true; // An ordinary --foreground invocation has no supervisor.
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const std::size_t size = std::strlen(path);
    if (size == 0 || size >= sizeof(address.sun_path)) return false;
    std::memcpy(address.sun_path, path, size + 1);
    if (path[0] == '@') address.sun_path[0] = '\0';
    const int socket = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (socket < 0) return false;
    constexpr char ready[] = "READY=1";
    const ssize_t sent = ::sendto(socket, ready, sizeof(ready) - 1, MSG_NOSIGNAL,
                                 reinterpret_cast<const sockaddr*>(&address),
                                 static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + size + (path[0] == '@' ? 0 : 1)));
    ::close(socket);
    return sent == static_cast<ssize_t>(sizeof(ready) - 1);
}

int stop_background() {
    auto args = manager("systemctl");
    args.insert(args.end(), {"show", "--property=ActiveState", "--value", kUnit});
    auto result = command(args);
    if (result.code != 0) return failed(result);
    if (result.output == "inactive\n") return 0; // Includes a nonexistent collected unit.
    args = manager("systemctl");
    args.insert(args.end(), {"stop", kUnit}); // Waits until SIGTERM cleanup has finished.
    result = command(args);
    if (result.code != 0) return failed(result);
    return 0;
}

int start_background(const std::filesystem::path& device_dir, int argc, char** argv) {
    std::error_code error;
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) { std::cerr << "Cannot resolve executable: " << error.message() << '\n'; return 1; }
    const auto device = std::filesystem::canonical(device_dir, error);
    if (error) { std::cerr << "Cannot resolve device: " << error.message() << '\n'; return 1; }
    if (stop_background() != 0) return 1;
    auto args = manager("systemd-run");
    args.reserve(static_cast<std::size_t>(argc) + 16);
    args.insert(args.end(), {"--unit=clevo-rgb-effect", "--collect", "--service-type=notify",
                            "--property=NotifyAccess=main", "--property=Restart=no",
                            "--property=StandardInput=null", "--property=StandardOutput=journal",
                            "--property=StandardError=journal", "--"});
    args.push_back(executable.string());
    args.insert(args.end(), {"--device-dir", device.string(), "--foreground"});
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--device-dir") { ++i; continue; }
        args.emplace_back(arg);
    }
    const auto result = command(args);
    if (result.code != 0) return failed(result);
    std::cout << "Started detached effect (" << (user_scope() ? "user" : "system") << " service).\n"
              << "Stop: " << (user_scope() ? "" : "sudo ") << "clevo-rgb --stop\n";
    return 0;
}
