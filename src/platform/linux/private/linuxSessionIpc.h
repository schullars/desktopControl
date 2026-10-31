#pragma once

#include "operationResult.h"

#include <string>

namespace desktopctl {

class LinuxSessionClient final {
public:
    static bool isWaylandSession();
    static OperationResult request(const std::string& command, std::string* response = nullptr);
    static OperationResult writeStartupError(const std::string& message);
    static std::string readStartupError();
    static void clearStartupError();

private:
    friend class LinuxSessionServer;

    static OperationResult ensureRuntimeDirectory(std::string& directory);
    static OperationResult socketPath(std::string& path);
    static OperationResult startupErrorPath(std::string& path);
};

class LinuxSessionServer final {
public:
    LinuxSessionServer();
    ~LinuxSessionServer();

    LinuxSessionServer(const LinuxSessionServer&) = delete;
    LinuxSessionServer& operator=(const LinuxSessionServer&) = delete;

    OperationResult open();
    void close();
    int descriptor() const;
    OperationResult acceptRequest(std::string& request, int& clientDescriptor);
    OperationResult respond(int clientDescriptor, const OperationResult& result);

private:
    int lockDescriptor_ = -1;
    int socketDescriptor_ = -1;
    std::string socketPath_;
};

} // namespace desktopctl
