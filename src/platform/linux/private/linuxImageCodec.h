#pragma once

#include "operationResult.h"

#include <jpeglib.h>

#include <csetjmp>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace desktopctl {

struct LinuxRgbImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

class LinuxImageCodec final {
public:
    static OperationResult readPng(const std::filesystem::path& path, LinuxRgbImage& image);
    static OperationResult write(const std::filesystem::path& path, const LinuxRgbImage& image);

private:
    struct JpegError {
        jpeg_error_mgr manager{};
        std::jmp_buf recovery{};
        char message[JMSG_LENGTH_MAX]{};
    };

    static void handleJpegError(j_common_ptr compressor);
    static OperationResult writeBmp(const std::filesystem::path& path,
                                    const LinuxRgbImage& image);
    static OperationResult writeJpeg(const std::filesystem::path& path,
                                     const LinuxRgbImage& image);
};

} // namespace desktopctl
