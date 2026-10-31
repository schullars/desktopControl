#include "platform/linux/private/linuxSessionIpc.h"

#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <poll.h>
#include <string>

namespace desktopctl {

bool LinuxSessionClient::isWaylandSession()
{
    const char* sessionType = std::getenv("XDG_SESSION_TYPE");
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    return (sessionType != nullptr && std::string(sessionType) == "wayland")
        || (waylandDisplay != nullptr && *waylandDisplay != '\0');
}

OperationResult LinuxSessionClient::ensureRuntimeDirectory(std::string& directory)
{
    const char* runtimeDirectory = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirectory != nullptr && *runtimeDirectory != '\0') {
        directory = std::string(runtimeDirectory) + "/desktopctl";
    } else {
        directory = "/tmp/desktopctl-" + std::to_string(static_cast<unsigned long>(getuid()));
    }

    if (mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) {
        return {false, "Unable to create the Linux session directory: "
                           + std::string(std::strerror(errno)) + "."};
    }
    struct stat status{};
    if (lstat(directory.c_str(), &status) != 0 || !S_ISDIR(status.st_mode)
        || S_ISLNK(status.st_mode)) {
        return {false, "The Linux session path is not a directory."};
    }
    if (status.st_uid != getuid()) {
        return {false, "The Linux session directory is not owned by the current user."};
    }
    if (chmod(directory.c_str(), 0700) != 0) {
        return {false, "Unable to secure the Linux session directory: "
                           + std::string(std::strerror(errno)) + "."};
    }
    return {true, {}};
}

OperationResult LinuxSessionClient::socketPath(std::string& path)
{
    std::string directory;
    OperationResult result = ensureRuntimeDirectory(directory);
    if (!result.success) {
        return result;
    }
    path = directory + "/session.sock";
    if (path.size() >= sizeof(sockaddr_un::sun_path)) {
        return {false, "The Linux session socket path is too long."};
    }
    return {true, {}};
}

OperationResult LinuxSessionClient::startupErrorPath(std::string& path)
{
    std::string directory;
    OperationResult result = ensureRuntimeDirectory(directory);
    if (!result.success) {
        return result;
    }
    path = directory + "/startup-error.txt";
    return {true, {}};
}

OperationResult LinuxSessionClient::request(const std::string& command, std::string* response)
{
    if (command.empty() || command.find('\n') != std::string::npos) {
        return {false, "The Linux session command is invalid."};
    }
    std::string path;
    OperationResult pathResult = socketPath(path);
    if (!pathResult.success) {
        return pathResult;
    }

    const int descriptor = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (descriptor < 0) {
        return {false, "Unable to create the Linux session client socket: "
                           + std::string(std::strerror(errno)) + "."};
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1U);
    if (connect(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        const int failure = errno;
        ::close(descriptor);
        return {false, "No desktopctl Linux session is running: "
                           + std::string(std::strerror(failure)) + "."};
    }

    const std::string payload = command + "\n";
    std::size_t written = 0;
    while (written < payload.size()) {
        const ssize_t count = send(descriptor, payload.data() + written,
                                   payload.size() - written, MSG_NOSIGNAL);
        if (count <= 0) {
            const int failure = errno;
            ::close(descriptor);
            return {false, "Unable to send the Linux session command: "
                               + std::string(std::strerror(failure)) + "."};
        }
        written += static_cast<std::size_t>(count);
    }
    shutdown(descriptor, SHUT_WR);

    pollfd pollDescriptor{descriptor, POLLIN, 0};
    const int responseTimeout = command.rfind("SCREENSHOT ", 0) == 0 ? 120000 : 5000;
    if (poll(&pollDescriptor, 1, responseTimeout) <= 0) {
        ::close(descriptor);
        return {false, responseTimeout == 120000
                           ? "The Linux session did not answer the screenshot request within "
                             "two minutes."
                           : "The Linux session did not answer within five seconds."};
    }

    std::string reply;
    char buffer[4096];
    for (;;) {
        const ssize_t count = recv(descriptor, buffer, sizeof(buffer), 0);
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int failure = errno;
            ::close(descriptor);
            return {false, "Unable to read the Linux session response: "
                               + std::string(std::strerror(failure)) + "."};
        }
        reply.append(buffer, static_cast<std::size_t>(count));
        if (reply.size() > 1024U * 1024U) {
            ::close(descriptor);
            return {false, "The Linux session response exceeded one MiB."};
        }
    }
    ::close(descriptor);
    if (!reply.empty() && reply.back() == '\n') {
        reply.pop_back();
    }
    const bool succeeded = reply == "OK" || reply.rfind("OK ", 0) == 0;
    const bool failed = reply == "ERR" || reply.rfind("ERR ", 0) == 0;
    if (!succeeded && !failed) {
        return {false, "The Linux session returned an invalid response."};
    }
    const std::size_t separator = reply.find(' ');
    const std::string message = separator == std::string::npos ? std::string()
                                                               : reply.substr(separator + 1U);
    if (response != nullptr) {
        *response = message;
    }
    return {succeeded, message};
}

OperationResult LinuxSessionClient::writeStartupError(const std::string& message)
{
    std::string path;
    OperationResult pathResult = startupErrorPath(path);
    if (!pathResult.success) {
        return pathResult;
    }
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return {false, "Unable to create the Linux session startup error file."};
    }
    stream << message;
    stream.close();
    if (!stream) {
        return {false, "Unable to write the Linux session startup error file."};
    }
    chmod(path.c_str(), 0600);
    return {true, {}};
}

std::string LinuxSessionClient::readStartupError()
{
    std::string path;
    if (!startupErrorPath(path).success) {
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

void LinuxSessionClient::clearStartupError()
{
    std::string path;
    if (startupErrorPath(path).success) {
        unlink(path.c_str());
    }
}

LinuxSessionServer::LinuxSessionServer() = default;

LinuxSessionServer::~LinuxSessionServer()
{
    close();
}

OperationResult LinuxSessionServer::open()
{
    if (socketDescriptor_ >= 0) {
        return {true, {}};
    }
    std::string directory;
    OperationResult directoryResult = LinuxSessionClient::ensureRuntimeDirectory(directory);
    if (!directoryResult.success) {
        return directoryResult;
    }
    socketPath_ = directory + "/session.sock";
    const std::string lockPath = directory + "/session.lock";

    lockDescriptor_ = ::open(lockPath.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lockDescriptor_ < 0) {
        return {false, "Unable to open the Linux session lock: "
                           + std::string(std::strerror(errno)) + "."};
    }
    if (flock(lockDescriptor_, LOCK_EX | LOCK_NB) != 0) {
        return {false, "A desktopctl Linux session is already running."};
    }

    socketDescriptor_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socketDescriptor_ < 0) {
        return {false, "Unable to create the Linux session socket: "
                           + std::string(std::strerror(errno)) + "."};
    }
    unlink(socketPath_.c_str());
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socketPath_.c_str(), socketPath_.size() + 1U);
    if (bind(socketDescriptor_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || chmod(socketPath_.c_str(), 0600) != 0 || listen(socketDescriptor_, 8) != 0) {
        const int failure = errno;
        close();
        return {false, "Unable to publish the Linux session socket: "
                           + std::string(std::strerror(failure)) + "."};
    }
    return {true, {}};
}

void LinuxSessionServer::close()
{
    if (socketDescriptor_ >= 0) {
        ::close(socketDescriptor_);
        socketDescriptor_ = -1;
    }
    if (!socketPath_.empty()) {
        unlink(socketPath_.c_str());
        socketPath_.clear();
    }
    if (lockDescriptor_ >= 0) {
        flock(lockDescriptor_, LOCK_UN);
        ::close(lockDescriptor_);
        lockDescriptor_ = -1;
    }
}

int LinuxSessionServer::descriptor() const
{
    return socketDescriptor_;
}

OperationResult LinuxSessionServer::acceptRequest(std::string& request, int& clientDescriptor)
{
    request.clear();
    clientDescriptor = accept4(socketDescriptor_, nullptr, nullptr, SOCK_CLOEXEC);
    if (clientDescriptor < 0) {
        return {false, "Unable to accept a Linux session request: "
                           + std::string(std::strerror(errno)) + "."};
    }

    pollfd pollDescriptor{clientDescriptor, POLLIN, 0};
    if (poll(&pollDescriptor, 1, 2000) <= 0) {
        ::close(clientDescriptor);
        clientDescriptor = -1;
        return {false, "The Linux session client did not send a request."};
    }
    char buffer[4096];
    for (;;) {
        const ssize_t count = recv(clientDescriptor, buffer, sizeof(buffer), 0);
        if (count == 0) {
            break;
        }
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int failure = errno;
            ::close(clientDescriptor);
            clientDescriptor = -1;
            return {false, "Unable to read a Linux session request: "
                               + std::string(std::strerror(failure)) + "."};
        }
        request.append(buffer, static_cast<std::size_t>(count));
        const std::size_t newline = request.find('\n');
        if (newline != std::string::npos) {
            request.resize(newline);
            break;
        }
        if (request.size() > 1024U * 1024U) {
            ::close(clientDescriptor);
            clientDescriptor = -1;
            return {false, "The Linux session request exceeded one MiB."};
        }
    }
    if (request.empty()) {
        ::close(clientDescriptor);
        clientDescriptor = -1;
        return {false, "The Linux session request was empty."};
    }
    return {true, {}};
}

OperationResult LinuxSessionServer::respond(int clientDescriptor, const OperationResult& result)
{
    if (clientDescriptor < 0) {
        return {false, "The Linux session client descriptor is invalid."};
    }
    std::string message = result.message;
    for (char& character : message) {
        if (character == '\r' || character == '\n') {
            character = ' ';
        }
    }
    const std::string reply = (result.success ? "OK" : "ERR")
        + (message.empty() ? std::string() : " " + message) + "\n";
    std::size_t written = 0;
    while (written < reply.size()) {
        const ssize_t count = send(clientDescriptor, reply.data() + written,
                                   reply.size() - written, MSG_NOSIGNAL);
        if (count <= 0) {
            const int failure = errno;
            ::close(clientDescriptor);
            return {false, "Unable to answer the Linux session client: "
                               + std::string(std::strerror(failure)) + "."};
        }
        written += static_cast<std::size_t>(count);
    }
    ::close(clientDescriptor);
    return {true, {}};
}

} // namespace desktopctl
