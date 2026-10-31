#include "platform/linux/private/linuxPortalRemoteDesktop.h"
#include "platform/linux/private/linuxPortalVariant.h"
#include "platform/linux/private/linuxPipeWireCapture.h"

#include <gio/gunixfdlist.h>

#include <algorithm>

namespace desktopctl {

LinuxPortalRemoteDesktop::LinuxPortalRemoteDesktop() = default;

LinuxPortalRemoteDesktop::~LinuxPortalRemoteDesktop()
{
    stop();
}

OperationResult LinuxPortalRemoteDesktop::start()
{
    if (isStarted()) {
        return {true, "Wayland remote-desktop portal session is already active."};
    }
    OperationResult result = portal_.open();
    if (!result.success) {
        return result;
    }
    portalNameWatch_ = g_bus_watch_name_on_connection(
        portal_.connection(), "org.freedesktop.portal.Desktop",
        G_BUS_NAME_WATCHER_FLAGS_NONE, &LinuxPortalRemoteDesktop::handlePortalAppeared,
        &LinuxPortalRemoteDesktop::handlePortalVanished, this, nullptr);
    result = createSession();
    if (!result.success) {
        stop();
        return result;
    }
    result = selectDevices();
    if (!result.success) {
        stop();
        return result;
    }
    result = selectSources();
    if (!result.success) {
        stop();
        return result;
    }
    result = startSession();
    if (!result.success) {
        stop();
        return result;
    }
    if ((selectedDevices_ & 3U) == 0U) {
        stop();
        return {false, "The Wayland portal did not grant keyboard or pointer control."};
    }
    return {true, "Wayland remote-desktop portal session started."};
}

void LinuxPortalRemoteDesktop::stop()
{
    if (sessionClosedSubscription_ != 0 && portal_.connection() != nullptr) {
        g_dbus_connection_signal_unsubscribe(portal_.connection(),
                                             sessionClosedSubscription_);
        sessionClosedSubscription_ = 0;
    }
    if (portalNameWatch_ != 0) {
        g_bus_unwatch_name(portalNameWatch_);
        portalNameWatch_ = 0;
    }
    if (!sessionHandle_.empty() && portal_.connection() != nullptr) {
        GError* error = nullptr;
        GVariant* reply = g_dbus_connection_call_sync(
            portal_.connection(), "org.freedesktop.portal.Desktop", sessionHandle_.c_str(),
            "org.freedesktop.portal.Session", "Close", nullptr, nullptr,
            G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &error);
        if (reply != nullptr) {
            g_variant_unref(reply);
        }
        g_clear_error(&error);
    }
    streams_.clear();
    selectedDevices_ = 0;
    sessionHandle_.clear();
    portalNameSeen_ = false;
    sessionClosed_ = false;
    failureMessage_.clear();
    portal_.close();
}

bool LinuxPortalRemoteDesktop::isStarted() const
{
    return !sessionHandle_.empty();
}

bool LinuxPortalRemoteDesktop::isHealthy() const
{
    return isStarted() && !sessionClosed_;
}

std::string LinuxPortalRemoteDesktop::failureMessage() const
{
    return failureMessage_.empty()
        ? "The Wayland remote-desktop portal session closed unexpectedly."
        : failureMessage_;
}

OperationResult LinuxPortalRemoteDesktop::movePointerAbsolute(int x, int y)
{
    OperationResult deviceResult = verifyDevice(2U, "pointer");
    if (!deviceResult.success) {
        return deviceResult;
    }
    double streamX = 0.0;
    double streamY = 0.0;
    const Stream* stream = streamForPoint(x, y, streamX, streamY);
    if (stream == nullptr) {
        return {false, "The coordinates are outside the monitors granted by the Wayland portal."};
    }
    return portal_.callMethod(
        "org.freedesktop.portal.RemoteDesktop", "NotifyPointerMotionAbsolute",
        g_variant_new("(o@a{sv}udd)", sessionHandle_.c_str(), emptyOptions(),
                      stream->nodeId, streamX, streamY));
}

OperationResult LinuxPortalRemoteDesktop::movePointerRelative(int deltaX, int deltaY)
{
    OperationResult deviceResult = verifyDevice(2U, "pointer");
    if (!deviceResult.success) {
        return deviceResult;
    }
    return portal_.callMethod(
        "org.freedesktop.portal.RemoteDesktop", "NotifyPointerMotion",
        g_variant_new("(o@a{sv}dd)", sessionHandle_.c_str(), emptyOptions(),
                      static_cast<double>(deltaX), static_cast<double>(deltaY)));
}

OperationResult LinuxPortalRemoteDesktop::setPointerButton(int evdevButton, bool pressed)
{
    OperationResult deviceResult = verifyDevice(2U, "pointer");
    if (!deviceResult.success) {
        return deviceResult;
    }
    return portal_.callMethod(
        "org.freedesktop.portal.RemoteDesktop", "NotifyPointerButton",
        g_variant_new("(o@a{sv}iu)", sessionHandle_.c_str(), emptyOptions(), evdevButton,
                      pressed ? 1U : 0U));
}

OperationResult LinuxPortalRemoteDesktop::scrollPointer(unsigned int axis, int steps)
{
    OperationResult deviceResult = verifyDevice(2U, "pointer");
    if (!deviceResult.success) {
        return deviceResult;
    }
    return portal_.callMethod(
        "org.freedesktop.portal.RemoteDesktop", "NotifyPointerAxisDiscrete",
        g_variant_new("(o@a{sv}ui)", sessionHandle_.c_str(), emptyOptions(), axis, steps));
}

OperationResult LinuxPortalRemoteDesktop::setKeyboardKeysym(std::uint32_t keysym, bool pressed)
{
    OperationResult deviceResult = verifyDevice(1U, "keyboard");
    if (!deviceResult.success) {
        return deviceResult;
    }
    return portal_.callMethod(
        "org.freedesktop.portal.RemoteDesktop", "NotifyKeyboardKeysym",
        g_variant_new("(o@a{sv}iu)", sessionHandle_.c_str(), emptyOptions(),
                      static_cast<int>(keysym), pressed ? 1U : 0U));
}

OperationResult LinuxPortalRemoteDesktop::captureDesktop(
    const std::filesystem::path& outputPath)
{
    if (!isStarted()) {
        return {false, "The Wayland remote-desktop portal session is not active."};
    }
    int descriptor = -1;
    OperationResult result = openPipeWireRemote(descriptor);
    if (!result.success) {
        return result;
    }
    std::vector<LinuxPipeWireStream> captureStreams;
    captureStreams.reserve(streams_.size());
    for (const Stream& stream : streams_) {
        captureStreams.push_back({stream.nodeId, stream.hasPosition, stream.x, stream.y,
                                  stream.hasSize, stream.width, stream.height});
    }
    LinuxPipeWireCapture capture;
    return capture.captureDesktop(descriptor, captureStreams, outputPath);
}

GVariant* LinuxPortalRemoteDesktop::emptyOptions()
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    return g_variant_builder_end(&builder);
}

void LinuxPortalRemoteDesktop::handleSessionClosed(
    GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant*,
    gpointer userData)
{
    auto* self = static_cast<LinuxPortalRemoteDesktop*>(userData);
    if (self == nullptr) {
        return;
    }
    self->sessionClosed_ = true;
    self->failureMessage_ = "The Wayland remote-desktop portal session was closed.";
}

void LinuxPortalRemoteDesktop::handlePortalAppeared(GDBusConnection*, const gchar*,
                                                      const gchar*, gpointer userData)
{
    auto* self = static_cast<LinuxPortalRemoteDesktop*>(userData);
    if (self != nullptr) {
        self->portalNameSeen_ = true;
    }
}

void LinuxPortalRemoteDesktop::handlePortalVanished(GDBusConnection*, const gchar*,
                                                      gpointer userData)
{
    auto* self = static_cast<LinuxPortalRemoteDesktop*>(userData);
    if (self == nullptr || !self->portalNameSeen_ || !self->isStarted()) {
        return;
    }
    self->sessionClosed_ = true;
    self->failureMessage_ = "The Wayland desktop portal service stopped.";
}

OperationResult LinuxPortalRemoteDesktop::createSession()
{
    const std::string token = portal_.nextToken("create");
    const std::string sessionToken = portal_.nextToken("session");
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));
    g_variant_builder_add(&options, "{sv}", "session_handle_token",
                          g_variant_new_string(sessionToken.c_str()));

    GVariant* results = nullptr;
    OperationResult result = portal_.callRequest(
        "org.freedesktop.portal.RemoteDesktop", "CreateSession",
        g_variant_new("(@a{sv})", g_variant_builder_end(&options)), token, results);
    if (!result.success) {
        return result;
    }
    readPortalObjectPath(results, "session_handle", sessionHandle_);
    g_variant_unref(results);
    if (sessionHandle_.empty()) {
        return {false, "The Wayland portal did not return a remote-desktop session handle."};
    }
    sessionClosedSubscription_ = g_dbus_connection_signal_subscribe(
        portal_.connection(), "org.freedesktop.portal.Desktop",
        "org.freedesktop.portal.Session", "Closed", sessionHandle_.c_str(), nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE, &LinuxPortalRemoteDesktop::handleSessionClosed,
        this, nullptr);
    return {true, {}};
}

OperationResult LinuxPortalRemoteDesktop::selectDevices()
{
    const std::string token = portal_.nextToken("devices");
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));
    g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(3U));

    GVariant* results = nullptr;
    OperationResult result = portal_.callRequest(
        "org.freedesktop.portal.RemoteDesktop", "SelectDevices",
        g_variant_new("(o@a{sv})", sessionHandle_.c_str(),
                      g_variant_builder_end(&options)), token, results);
    if (results != nullptr) {
        g_variant_unref(results);
    }
    return result;
}

OperationResult LinuxPortalRemoteDesktop::selectSources()
{
    const std::string token = portal_.nextToken("sources");
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));
    g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(1U));
    g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(TRUE));

    GVariant* results = nullptr;
    OperationResult result = portal_.callRequest(
        "org.freedesktop.portal.ScreenCast", "SelectSources",
        g_variant_new("(o@a{sv})", sessionHandle_.c_str(),
                      g_variant_builder_end(&options)), token, results);
    if (results != nullptr) {
        g_variant_unref(results);
    }
    return result;
}

OperationResult LinuxPortalRemoteDesktop::startSession()
{
    const std::string token = portal_.nextToken("start");
    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "handle_token",
                          g_variant_new_string(token.c_str()));

    GVariant* results = nullptr;
    OperationResult result = portal_.callRequest(
        "org.freedesktop.portal.RemoteDesktop", "Start",
        g_variant_new("(os@a{sv})", sessionHandle_.c_str(), "",
                      g_variant_builder_end(&options)), token, results, 60000);
    if (!result.success) {
        if (result.message == "The desktop portal request timed out.") {
            return {false, "authorization_timeout: The Wayland remote-desktop authorization was "
                           "not confirmed within 60 seconds."};
        }
        if (result.message == "The desktop portal request was cancelled."
            || result.message == "The desktop portal request was denied.") {
            return {false, "authorization_denied: The Wayland remote-desktop authorization was "
                           "cancelled or denied."};
        }
        return result;
    }
    if (g_variant_lookup(results, "devices", "u", &selectedDevices_) == FALSE) {
        selectedDevices_ = 0;
    }
    readStreams(results);
    g_variant_unref(results);
    if (streams_.empty()) {
        return {false, "The Wayland portal did not return a monitor stream for absolute input."};
    }
    return {true, {}};
}

OperationResult LinuxPortalRemoteDesktop::openPipeWireRemote(int& descriptor)
{
    descriptor = -1;
    GUnixFDList* descriptors = nullptr;
    GError* error = nullptr;
    GVariant* reply = g_dbus_connection_call_with_unix_fd_list_sync(
        portal_.connection(), "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop", "org.freedesktop.portal.ScreenCast",
        "OpenPipeWireRemote",
        g_variant_new("(o@a{sv})", sessionHandle_.c_str(), emptyOptions()),
        G_VARIANT_TYPE("(h)"), G_DBUS_CALL_FLAGS_NONE, 5000, nullptr, &descriptors,
        nullptr, &error);
    if (reply == nullptr) {
        const std::string detail = error != nullptr && error->message != nullptr
            ? error->message
            : "unknown error";
        g_clear_error(&error);
        if (descriptors != nullptr) {
            g_object_unref(descriptors);
        }
        return {false, "Unable to open the authorized PipeWire remote: " + detail + "."};
    }
    gint32 descriptorIndex = -1;
    g_variant_get(reply, "(h)", &descriptorIndex);
    g_variant_unref(reply);
    descriptor = descriptors == nullptr
        ? -1
        : g_unix_fd_list_get(descriptors, descriptorIndex, &error);
    if (descriptors != nullptr) {
        g_object_unref(descriptors);
    }
    if (descriptor < 0) {
        const std::string detail = error != nullptr && error->message != nullptr
            ? error->message
            : "the portal did not return a valid descriptor";
        g_clear_error(&error);
        return {false, "Unable to read the authorized PipeWire descriptor: " + detail + "."};
    }
    g_clear_error(&error);
    return {true, {}};
}

OperationResult LinuxPortalRemoteDesktop::verifyDevice(std::uint32_t device,
                                                         const std::string& name) const
{
    if (!isStarted()) {
        return {false, "The Wayland remote-desktop portal session is not active."};
    }
    if ((selectedDevices_ & device) == 0U) {
        return {false, "The Wayland portal did not grant " + name + " control."};
    }
    return {true, {}};
}

const LinuxPortalRemoteDesktop::Stream* LinuxPortalRemoteDesktop::streamForPoint(
    int x, int y, double& streamX, double& streamY) const
{
    for (const Stream& stream : streams_) {
        if (stream.hasPosition && stream.hasSize && x >= stream.x && y >= stream.y
            && x < stream.x + stream.width && y < stream.y + stream.height) {
            streamX = static_cast<double>(x - stream.x);
            streamY = static_cast<double>(y - stream.y);
            return &stream;
        }
    }
    if (streams_.size() == 1U) {
        const Stream& stream = streams_.front();
        streamX = static_cast<double>(stream.hasPosition ? x - stream.x : x);
        streamY = static_cast<double>(stream.hasPosition ? y - stream.y : y);
        if (!stream.hasSize || (streamX >= 0.0 && streamY >= 0.0
                                && streamX < stream.width && streamY < stream.height)) {
            return &stream;
        }
    }
    return nullptr;
}

void LinuxPortalRemoteDesktop::readStreams(GVariant* results)
{
    streams_.clear();
    GVariant* streams = g_variant_lookup_value(results, "streams",
                                                G_VARIANT_TYPE("a(ua{sv})"));
    if (streams == nullptr) {
        return;
    }
    const gsize count = g_variant_n_children(streams);
    streams_.reserve(static_cast<std::size_t>(count));
    for (gsize index = 0; index < count; ++index) {
        GVariant* child = g_variant_get_child_value(streams, index);
        guint nodeId = 0;
        GVariant* properties = nullptr;
        g_variant_get(child, "(u@a{sv})", &nodeId, &properties);
        Stream stream;
        stream.nodeId = nodeId;

        GVariant* position = g_variant_lookup_value(properties, "position",
                                                    G_VARIANT_TYPE("(ii)"));
        if (position != nullptr) {
            g_variant_get(position, "(ii)", &stream.x, &stream.y);
            stream.hasPosition = true;
            g_variant_unref(position);
        }
        GVariant* size = g_variant_lookup_value(properties, "size",
                                                G_VARIANT_TYPE("(ii)"));
        if (size != nullptr) {
            g_variant_get(size, "(ii)", &stream.width, &stream.height);
            stream.hasSize = stream.width > 0 && stream.height > 0;
            g_variant_unref(size);
        }
        streams_.push_back(stream);
        g_variant_unref(properties);
        g_variant_unref(child);
    }
    g_variant_unref(streams);
}

} // namespace desktopctl
