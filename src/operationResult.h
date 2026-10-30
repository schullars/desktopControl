#pragma once

#include <string>

namespace desktopctl {

struct OperationResult {
    bool success = false;
    std::string message;
};

} // namespace desktopctl
