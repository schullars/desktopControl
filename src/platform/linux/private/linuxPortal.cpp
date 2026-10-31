#include "platform/linux/private/linuxPortal.h"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace desktopctl {

LinuxPortal::LinuxPortal() = default;

LinuxPortal::~LinuxPortal()
{
    close();
}

OperationResult LinuxPortal::open()
{
    if (connection_ != nullptr) {
        return {true, {}};
    }
    GError* error = nullptr;
    connection_ = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (connection_ == nullptr) {
        const std::string message = errorMessage("Unable to connect to the session D-Bus", error);
        g_clear_error(&error);
        return {false, message};
    }
    return {true, {}};
}

void LinuxPortal::close()
{
    if (connection_ != nullptr) {
        g_object_unref(connection_);
        connection_ = nullptr;
    }
}

GDBusConnection* LinuxPortal::connection() const
{
    return connection_;
}

std::string LinuxPortal::nextToken(const std::string& purpose)
{
    ++tokenCounter_;
    std::string token = "desktopctl_" + purpose + "_"
        + std::to_string(static_cast<unsigned long>(getpid())) + "_"
        + std::to_string(static_cast<unsigned long long>(tokenCounter_));
    std::replace_if(token.begin(), token.end(), [](char character) {
        return !((character >= 'a' && character <= 'z')
                 || (character >= 'A' && character <= 'Z')
                 || (character >= '0' && character <= '9') || character == '_');
    }, '_');
    return token;
}

std::string LinuxPortal::requestPath(const std::string& token) const
{
    if (connection_ == nullptr) {
        return {};
    }
    const char* uniqueName = g_dbus_connection_get_unique_name(connection_);
    if (uniqueName == nullptr || *uniqueName == '\0') {
        return {};
    }
    std::string sender(uniqueName);
    if (!sender.empty() && sender.front() == ':') {
        sender.erase(sender.begin());
    }
    std::replace(sender.begin(), sender.end(), '.', '_');
    return "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
}

OperationResult LinuxPortal::callRequest(const char* interfaceName, const char* methodName,
                                          GVariant* parameters, const std::string& token,
                                          GVariant*& results, int timeoutMilliseconds)
{
    results = nullptr;
    OperationResult openResult = open();
    if (!openResult.success) {
        return openResult;
    }
    const std::string expectedPath = requestPath(token);
    if (expectedPath.empty()) {
        return {false, "Unable to derive the desktop portal request path."};
    }

    RequestState state;
    const guint subscription = g_dbus_connection_signal_subscribe(
        connection_, "org.freedesktop.portal.Desktop", "org.freedesktop.portal.Request",
        "Response", expectedPath.c_str(), nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
        &LinuxPortal::handleResponse, &state, nullptr);

    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(
        connection_, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        interfaceName, methodName, parameters, G_VARIANT_TYPE("(o)"), G_DBUS_CALL_FLAGS_NONE,
        30000, nullptr, &error);
    if (reply == nullptr) {
        g_dbus_connection_signal_unsubscribe(connection_, subscription);
        const std::string message = errorMessage(
            std::string("Desktop portal ") + methodName + " request failed", error);
        g_clear_error(&error);
        return {false, message};
    }

    const char* actualPath = nullptr;
    g_variant_get(reply, "(&o)", &actualPath);
    const bool pathMatches = actualPath != nullptr && expectedPath == actualPath;
    g_variant_unref(reply);
    if (!pathMatches) {
        g_dbus_connection_signal_unsubscribe(connection_, subscription);
        return {false, "The desktop portal returned an unexpected request path."};
    }

    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(timeoutMilliseconds);
    while (!state.received && std::chrono::steady_clock::now() < deadline) {
        while (g_main_context_iteration(nullptr, FALSE) != FALSE) {
        }
        if (!state.received) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    g_dbus_connection_signal_unsubscribe(connection_, subscription);
    if (!state.received) {
        return {false, "The desktop portal request timed out."};
    }
    if (state.response != 0U) {
        if (state.results != nullptr) {
            g_variant_unref(state.results);
        }
        return {false, state.response == 1U ? "The desktop portal request was cancelled."
                                           : "The desktop portal request was denied."};
    }
    results = state.results;
    return {true, {}};
}

OperationResult LinuxPortal::callMethod(const char* interfaceName, const char* methodName,
                                         GVariant* parameters)
{
    OperationResult openResult = open();
    if (!openResult.success) {
        return openResult;
    }
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_sync(
        connection_, "org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
        interfaceName, methodName, parameters, nullptr, G_DBUS_CALL_FLAGS_NONE, 5000,
        nullptr, &error);
    if (reply == nullptr) {
        const std::string message = errorMessage(
            std::string("Desktop portal ") + methodName + " call failed", error);
        g_clear_error(&error);
        return {false, message};
    }
    g_variant_unref(reply);
    return {true, {}};
}

void LinuxPortal::handleResponse(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                 const gchar*, GVariant* parameters, gpointer userData)
{
    auto* state = static_cast<RequestState*>(userData);
    if (state == nullptr || state->received) {
        return;
    }
    guint response = 2;
    GVariant* results = nullptr;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    state->response = response;
    state->results = results;
    state->received = true;
}

std::string LinuxPortal::errorMessage(const std::string& operation, GError* error)
{
    if (error == nullptr || error->message == nullptr) {
        return operation + ".";
    }
    return operation + ": " + error->message + ".";
}

} // namespace desktopctl
