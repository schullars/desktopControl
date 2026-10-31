#pragma once

#include "operationResult.h"

#include <gio/gio.h>

#include <cstdint>
#include <string>

namespace desktopctl {

class LinuxPortal final {
public:
    LinuxPortal();
    ~LinuxPortal();

    LinuxPortal(const LinuxPortal&) = delete;
    LinuxPortal& operator=(const LinuxPortal&) = delete;

    OperationResult open();
    void close();
    GDBusConnection* connection() const;
    std::string nextToken(const std::string& purpose);
    std::string requestPath(const std::string& token) const;
    OperationResult callRequest(const char* interfaceName, const char* methodName,
                                GVariant* parameters, const std::string& token,
                                GVariant*& results, int timeoutMilliseconds = 90000);
    OperationResult callMethod(const char* interfaceName, const char* methodName,
                               GVariant* parameters);

private:
    struct RequestState {
        bool received = false;
        std::uint32_t response = 2;
        GVariant* results = nullptr;
    };

    static void handleResponse(GDBusConnection* connection, const gchar* senderName,
                               const gchar* objectPath, const gchar* interfaceName,
                               const gchar* signalName, GVariant* parameters,
                               gpointer userData);
    static std::string errorMessage(const std::string& operation, GError* error);

    GDBusConnection* connection_ = nullptr;
    std::uint64_t tokenCounter_ = 0;
};

} // namespace desktopctl
