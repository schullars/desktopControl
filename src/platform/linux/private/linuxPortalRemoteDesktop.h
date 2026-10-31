#pragma once

#include "operationResult.h"
#include "platform/linux/private/linuxPortal.h"

#include <cstdint>
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

    OperationResult movePointerAbsolute(int x, int y);
    OperationResult movePointerRelative(int deltaX, int deltaY);
    OperationResult setPointerButton(int evdevButton, bool pressed);
    OperationResult scrollPointer(unsigned int axis, int steps);
    OperationResult setKeyboardKeysym(std::uint32_t keysym, bool pressed);

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
    OperationResult createSession();
    OperationResult selectDevices();
    OperationResult selectSources();
    OperationResult startSession();
    OperationResult verifyDevice(std::uint32_t device, const std::string& name) const;
    const Stream* streamForPoint(int x, int y, double& streamX, double& streamY) const;
    void readStreams(GVariant* results);

    LinuxPortal portal_;
    std::string sessionHandle_;
    std::uint32_t selectedDevices_ = 0;
    std::vector<Stream> streams_;
};

} // namespace desktopctl
