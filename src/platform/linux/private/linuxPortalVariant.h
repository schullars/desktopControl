#pragma once

#include <gio/gio.h>

#include <string>

namespace desktopctl {

bool readPortalObjectPath(GVariant* results, const char* key, std::string& objectPath);

} // namespace desktopctl
