#pragma once

#include "operationResult.h"
#include "platform/linux/private/linuxPortal.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace desktopctl {

class LinuxPortalRemoteDesktop final {
public:
    LinuxPortalRemoteDesktop();
    ~LinuxPortalRemoteDesktop();

    LinuxPortalRemoteDesktop(const LinuxPortalRemoteDesktop&) = delete;
    LinuxPortalRemoteDesktop& operator=(const LinuxPortalRemoteDesktop&) = delete;

    OperationResult start();
    void stop();
    bool isStarted() const;
    bool isHealthy() const;
    std::string failureMessage() const;

    OperationResult movePointerAbsolute(int x, int y);
    OperationResult movePointerRelative(int deltaX, int deltaY);
    OperationResult setPointerButton(int evdevButton, bool pressed);
    OperationResult scrollPointer(unsigned int axis, int steps);
    OperationResult setKeyboardKeysym(std::uint32_t keysym, bool pressed);
    OperationResult captureDesktop(const std::filesystem::path& outputPath);

private:
    struct Stream {
        std::uint32_t nodeId = 0;
        bool hasPosition = false;
        int x = 0;
        int y = 0;
        bool hasSize = false;
        int width = 0;
        int height = 0;
    };

    static GVariant* emptyOptions();
    static void handleSessionClosed(GDBusConnection* connection, const gchar* senderName,
                                    const gchar* objectPath, const gchar* interfaceName,
                                    const gchar* signalName, GVariant* parameters,
                                    gpointer userData);
    static void handlePortalAppeared(GDBusConnection* connection, const gchar* name,
                                     const gchar* owner, gpointer userData);
    static void handlePortalVanished(GDBusConnection* connection, const gchar* name,
                                     gpointer userData);
    OperationResult createSession();
    OperationResult selectDevices();
    OperationResult selectSources();
    OperationResult startSession();
    OperationResult openPipeWireRemote(int& descriptor);
    OperationResult verifyDevice(std::uint32_t device, const std::string& name) const;
    const Stream* streamForPoint(int x, int y, double& streamX, double& streamY) const;
    void readStreams(GVariant* results);

    LinuxPortal portal_;
    std::string sessionHandle_;
    std::uint32_t selectedDevices_ = 0;
    std::vector<Stream> streams_;
    guint sessionClosedSubscription_ = 0;
    guint portalNameWatch_ = 0;
    bool portalNameSeen_ = false;
    bool sessionClosed_ = false;
    std::string failureMessage_;
};

} // namespace desktopctl
