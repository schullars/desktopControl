#include "platform/linux/private/linuxPortalVariant.h"

namespace desktopctl {

bool readPortalObjectPath(GVariant* results, const char* key, std::string& objectPath)
{
    objectPath.clear();
    if (results == nullptr || key == nullptr) {
        return false;
    }

    GVariant* value = g_variant_lookup_value(results, key, nullptr);
    if (value == nullptr) {
        return false;
    }
    const bool supportedType = g_variant_is_of_type(value, G_VARIANT_TYPE_OBJECT_PATH)
        || g_variant_is_of_type(value, G_VARIANT_TYPE_STRING);
    const char* candidate = supportedType ? g_variant_get_string(value, nullptr) : nullptr;
    const bool valid = candidate != nullptr && g_variant_is_object_path(candidate) != FALSE;
    if (valid) {
        objectPath = candidate;
    }
    g_variant_unref(value);
    return valid;
}

} // namespace desktopctl
