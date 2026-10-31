#include "platform/linux/private/linuxImageCodec.h"

#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    using desktopctl::LinuxImageCodec;
    using desktopctl::LinuxRgbImage;

    std::error_code error;
    const std::filesystem::path directory = std::filesystem::temp_directory_path(error)
        / ("desktopctl-codec-test-" + std::to_string(static_cast<unsigned long>(getpid())));
    if (error || !std::filesystem::create_directories(directory, error) || error) {
        std::cerr << "Unable to create the image codec test directory.\n";
        return 1;
    }

    LinuxRgbImage image;
    image.width = 2;
    image.height = 2;
    image.pixels = {
        255, 0, 0, 0, 255, 0,
        0, 0, 255, 255, 255, 255
    };

    const std::filesystem::path jpegPath = directory / "test.jpg";
    const std::filesystem::path bmpPath = directory / "test.bmp";
    if (!LinuxImageCodec::write(jpegPath, image).success
        || !LinuxImageCodec::write(bmpPath, image).success) {
        std::cerr << "Image codec write failed.\n";
        std::filesystem::remove_all(directory, error);
        return 1;
    }

    std::array<unsigned char, 2> signature{};
    std::ifstream jpeg(jpegPath, std::ios::binary);
    jpeg.read(reinterpret_cast<char*>(signature.data()),
              static_cast<std::streamsize>(signature.size()));
    if (!jpeg || signature[0] != 0xffU || signature[1] != 0xd8U) {
        std::cerr << "JPEG signature validation failed.\n";
        std::filesystem::remove_all(directory, error);
        return 1;
    }

    std::ifstream bmp(bmpPath, std::ios::binary);
    bmp.read(reinterpret_cast<char*>(signature.data()),
             static_cast<std::streamsize>(signature.size()));
    if (!bmp || signature[0] != 'B' || signature[1] != 'M') {
        std::cerr << "BMP signature validation failed.\n";
        std::filesystem::remove_all(directory, error);
        return 1;
    }

    std::filesystem::remove_all(directory, error);
    if (error) {
        std::cerr << "Unable to clean the image codec test directory.\n";
        return 1;
    }
    return 0;
}
