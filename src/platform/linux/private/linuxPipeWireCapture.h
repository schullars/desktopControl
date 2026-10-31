#pragma once

#include "operationResult.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace desktopctl {

struct LinuxPipeWireStream {
    std::uint32_t nodeId = 0;
    bool hasPosition = false;
    int x = 0;
    int y = 0;
    bool hasSize = false;
    int width = 0;
    int height = 0;
};

class LinuxPipeWireCapture final {
public:
    LinuxPipeWireCapture();
    ~LinuxPipeWireCapture();

    LinuxPipeWireCapture(const LinuxPipeWireCapture&) = delete;
    LinuxPipeWireCapture& operator=(const LinuxPipeWireCapture&) = delete;

    OperationResult captureDesktop(int pipeWireDescriptor,
                                   const std::vector<LinuxPipeWireStream>& streams,
                                   const std::filesystem::path& outputPath);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace desktopctl
