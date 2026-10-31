#include "private/activityIndicatorPrivate.h"

#include "platform/linux/private/linuxActivityOverlay.h"
#include "platform/linux/private/linuxPortalRemoteDesktop.h"
#include "platform/linux/private/linuxSessionIpc.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <thread>

namespace desktopctl {

ActivityIndicatorPrivate::ActivityIndicatorPrivate() = default;

ActivityIndicatorPrivate::~ActivityIndicatorPrivate() = default;

OperationResult ActivityIndicatorPrivate::startBackground()
{
    if (isRunning()) {
        OperationResult activityResult = recordActivity();
        if (!activityResult.success) {
            return activityResult;
        }
        return {true, "Session is already started. Green activity indicator is running."};
    }

    LinuxSessionClient::clearStartupError();
    char executablePath[4096]{};
    const ssize_t pathLength = readlink("/proc/self/exe", executablePath,
                                        sizeof(executablePath) - 1U);
    if (pathLength <= 0 || static_cast<std::size_t>(pathLength) >= sizeof(executablePath)) {
        return {false, "Unable to resolve the desktopctl executable path on Linux."};
    }
    executablePath[pathLength] = '\0';

    const pid_t child = fork();
    if (child < 0) {
        return {false, "Unable to fork the Linux background session: "
                           + std::string(std::strerror(errno)) + "."};
    }
    if (child == 0) {
        if (setsid() < 0) {
            LinuxSessionClient::writeStartupError("Unable to detach the Linux session process.");
            _exit(3);
        }
        const int nullDescriptor = open("/dev/null", O_RDWR);
        if (nullDescriptor >= 0) {
            dup2(nullDescriptor, STDIN_FILENO);
            dup2(nullDescriptor, STDOUT_FILENO);
            dup2(nullDescriptor, STDERR_FILENO);
            if (nullDescriptor > STDERR_FILENO) {
                close(nullDescriptor);
            }
        }
        execl(executablePath, executablePath, "session", "host", nullptr);
        LinuxSessionClient::writeStartupError(
            "Unable to execute the detached Linux session process: "
            + std::string(std::strerror(errno)) + ".");
        _exit(3);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(125);
    while (std::chrono::steady_clock::now() < deadline) {
        if (isRunning()) {
            return {true, LinuxSessionClient::isWaylandSession()
                              ? "Session started. Green XWayland indicator and Wayland portal "
                                "control are running."
                              : "Session started. Green X11 activity indicator is running."};
        }
        int status = 0;
        const pid_t waitResult = waitpid(child, &status, WNOHANG);
        if (waitResult == child) {
            const std::string startupError = LinuxSessionClient::readStartupError();
            return {false, startupError.empty()
                               ? "The Linux background session exited before becoming ready."
                               : startupError};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return {false, "The Linux background session did not become ready within 125 seconds."};
}

bool ActivityIndicatorPrivate::isRunning() const
{
    return LinuxSessionClient::request("PING").success;
}

OperationResult ActivityIndicatorPrivate::recordActivity()
{
    OperationResult result = LinuxSessionClient::request("ACTIVITY");
    if (result.success) {
        result.message.clear();
    }
    return result;
}

OperationResult ActivityIndicatorPrivate::run()
{
    stopSignal_ = 0;
    struct sigaction signalAction{};
    signalAction.sa_handler = &ActivityIndicatorPrivate::handleSignal;
    sigemptyset(&signalAction.sa_mask);
    sigaction(SIGINT, &signalAction, nullptr);
    sigaction(SIGTERM, &signalAction, nullptr);

    linuxOverlay_ = std::make_unique<LinuxActivityOverlay>();
    OperationResult result = linuxOverlay_->start();
    if (!result.success) {
        LinuxSessionClient::writeStartupError(result.message);
        linuxOverlay_.reset();
        return result;
    }

    if (LinuxSessionClient::isWaylandSession()) {
        linuxPortal_ = std::make_unique<LinuxPortalRemoteDesktop>();
        result = linuxPortal_->start();
        if (!result.success) {
            LinuxSessionClient::writeStartupError(result.message);
            linuxPortal_.reset();
            linuxOverlay_.reset();
            return result;
        }
    }

    linuxServer_ = std::make_unique<LinuxSessionServer>();
    result = linuxServer_->open();
    if (!result.success) {
        LinuxSessionClient::writeStartupError(result.message);
        linuxPortal_.reset();
        linuxOverlay_.reset();
        linuxServer_.reset();
        return result;
    }
    LinuxSessionClient::clearStartupError();

    bool stopRequested = false;
    bool idleTimeout = false;
    auto lastActivity = std::chrono::steady_clock::now();
    while (!stopRequested && stopSignal_ == 0) {
        pollfd descriptors[2]{{linuxServer_->descriptor(), POLLIN, 0},
                              {linuxOverlay_->descriptor(), POLLIN, 0}};
        const int descriptorCount = descriptors[1].fd >= 0 ? 2 : 1;
        const int pollResult = poll(descriptors, static_cast<nfds_t>(descriptorCount), 16);
        if (pollResult < 0 && errno != EINTR) {
            result = {false, "The Linux activity-indicator event loop failed: "
                                 + std::string(std::strerror(errno)) + "."};
            break;
        }
        if ((descriptors[0].revents & POLLIN) != 0) {
            std::string request;
            int clientDescriptor = -1;
            OperationResult acceptResult = linuxServer_->acceptRequest(request,
                                                                        clientDescriptor);
            if (acceptResult.success) {
                OperationResult requestResult = handleLinuxRequest(request, stopRequested);
                if (request == "ACTIVITY" || request.rfind("POINTER_", 0) == 0
                    || request.rfind("KEYSYM ", 0) == 0) {
                    lastActivity = std::chrono::steady_clock::now();
                }
                linuxServer_->respond(clientDescriptor, requestResult);
            }
        }
        if (descriptorCount == 2 && (descriptors[1].revents & POLLIN) != 0) {
            linuxOverlay_->processEvents();
        }
        result = linuxOverlay_->update();
        if (!result.success) {
            break;
        }
        while (g_main_context_iteration(nullptr, FALSE) != FALSE) {
        }
        if (std::chrono::steady_clock::now() - lastActivity
            >= std::chrono::seconds(SessionIdleTimeoutSeconds)) {
            idleTimeout = true;
            break;
        }
    }

    linuxServer_.reset();
    linuxPortal_.reset();
    linuxOverlay_.reset();
    if (!result.success) {
        return result;
    }
    if (idleTimeout) {
        return {true, "Session stopped after three minutes of inactivity."};
    }
    return {true, "Green activity indicator stopped."};
}

OperationResult ActivityIndicatorPrivate::requestStop()
{
    OperationResult result = LinuxSessionClient::request("STOP");
    if (!result.success) {
        return result;
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
        if (!isRunning()) {
            return {true, "Session stopped. Green activity indicator closed."};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {false, "The Linux activity indicator did not stop within two seconds."};
}

void ActivityIndicatorPrivate::handleSignal(int)
{
    stopSignal_ = 1;
}

OperationResult ActivityIndicatorPrivate::handleLinuxRequest(const std::string& request,
                                                              bool& stopRequested)
{
    if (request == "PING") {
        return {true, "READY"};
    }
    if (request == "ACTIVITY") {
        return {true, {}};
    }
    if (request == "STOP") {
        stopRequested = true;
        return {true, "Stopping."};
    }
    if (linuxPortal_ == nullptr) {
        return {false, "The command requires a Wayland portal session."};
    }

    std::istringstream stream(request);
    std::string action;
    stream >> action;
    if (action == "POINTER_ABSOLUTE") {
        int x = 0;
        int y = 0;
        if (stream >> x >> y) {
            return linuxPortal_->movePointerAbsolute(x, y);
        }
    } else if (action == "POINTER_RELATIVE") {
        int x = 0;
        int y = 0;
        if (stream >> x >> y) {
            return linuxPortal_->movePointerRelative(x, y);
        }
    } else if (action == "POINTER_BUTTON") {
        int button = 0;
        int pressed = 0;
        if (stream >> button >> pressed && (pressed == 0 || pressed == 1)) {
            return linuxPortal_->setPointerButton(button, pressed != 0);
        }
    } else if (action == "POINTER_SCROLL") {
        unsigned int axis = 0;
        int steps = 0;
        if (stream >> axis >> steps && axis <= 1U) {
            return linuxPortal_->scrollPointer(axis, steps);
        }
    } else if (action == "KEYSYM") {
        std::uint32_t keysym = 0;
        int pressed = 0;
        if (stream >> keysym >> pressed && (pressed == 0 || pressed == 1)) {
            return linuxPortal_->setKeyboardKeysym(keysym, pressed != 0);
        }
    }
    return {false, "The Linux session command is invalid."};
}

} // namespace desktopctl
