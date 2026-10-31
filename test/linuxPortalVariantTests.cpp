#include "platform/linux/private/linuxPortalVariant.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

GVariant* makeResults(const char* type, const char* value)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    if (std::string(type) == "object-path") {
        g_variant_builder_add(&builder, "{sv}", "session_handle",
                              g_variant_new_object_path(value));
    } else {
        g_variant_builder_add(&builder, "{sv}", "session_handle",
                              g_variant_new_string(value));
    }
    return g_variant_ref_sink(g_variant_builder_end(&builder));
}

bool expectPath(GVariant* results, bool expectedSuccess, const std::string& expectedPath)
{
    std::string path;
    const bool success = desktopctl::readPortalObjectPath(results, "session_handle", path);
    if (success != expectedSuccess || path != expectedPath) {
        std::cerr << "Unexpected portal object path result.\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    GVariant* objectPath = makeResults(
        "object-path", "/org/freedesktop/portal/desktop/session/test/object");
    GVariant* stringPath = makeResults(
        "string", "/org/freedesktop/portal/desktop/session/test/string");
    GVariant* invalidPath = makeResults("string", "not/an/object/path");

    const bool passed = expectPath(objectPath, true,
                                   "/org/freedesktop/portal/desktop/session/test/object")
        && expectPath(stringPath, true,
                      "/org/freedesktop/portal/desktop/session/test/string")
        && expectPath(invalidPath, false, "")
        && expectPath(nullptr, false, "");

    g_variant_unref(objectPath);
    g_variant_unref(stringPath);
    g_variant_unref(invalidPath);
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
