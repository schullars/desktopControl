#include "platform/linux/private/linuxImageCodec.h"

#include <jpeglib.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

namespace desktopctl {

OperationResult LinuxImageCodec::readPng(const std::filesystem::path& path,
                                         LinuxRgbImage& image)
{
    image = {};
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_file(&png, path.c_str()) == 0) {
        return {false, "Unable to read the portal PNG: " + std::string(png.message) + "."};
    }
    png.format = PNG_FORMAT_RGB;
    if (png.width > static_cast<png_uint_32>(std::numeric_limits<int>::max())
        || png.height > static_cast<png_uint_32>(std::numeric_limits<int>::max())) {
        png_image_free(&png);
        return {false, "The portal PNG dimensions exceed the supported range."};
    }
    image.width = static_cast<int>(png.width);
    image.height = static_cast<int>(png.height);
    image.pixels.resize(PNG_IMAGE_SIZE(png));
    if (png_image_finish_read(&png, nullptr, image.pixels.data(), 0, nullptr) == 0) {
        const std::string message = png.message;
        png_image_free(&png);
        image = {};
        return {false, "Unable to decode the portal PNG: " + message + "."};
    }
    png_image_free(&png);
    return {true, {}};
}

OperationResult LinuxImageCodec::write(const std::filesystem::path& path,
                                       const LinuxRgbImage& image)
{
    if (image.width <= 0 || image.height <= 0
        || image.pixels.size() != static_cast<std::size_t>(image.width)
                * static_cast<std::size_t>(image.height) * 3U) {
        return {false, "The RGB image dimensions or buffer size are invalid."};
    }
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (extension == ".jpg" || extension == ".jpeg") {
        return writeJpeg(path, image);
    }
    if (extension == ".bmp") {
        return writeBmp(path, image);
    }
    return {false, "Output extension must be .jpg, .jpeg, or .bmp."};
}

OperationResult LinuxImageCodec::writeBmp(const std::filesystem::path& path,
                                           const LinuxRgbImage& image)
{
    const std::uint32_t rowBytes = static_cast<std::uint32_t>(image.width) * 3U;
    const std::uint32_t stride = (rowBytes + 3U) & ~3U;
    const std::uint64_t pixelBytes = static_cast<std::uint64_t>(stride)
        * static_cast<std::uint64_t>(image.height);
    if (pixelBytes > std::numeric_limits<std::uint32_t>::max() - 54U) {
        return {false, "The BMP is too large."};
    }

    std::array<std::uint8_t, 54> header{};
    const auto write16 = [&header](std::size_t offset, std::uint16_t value) {
        header[offset] = static_cast<std::uint8_t>(value & 0xffU);
        header[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    };
    const auto write32 = [&header](std::size_t offset, std::uint32_t value) {
        for (std::size_t byte = 0; byte < 4U; ++byte) {
            header[offset + byte] = static_cast<std::uint8_t>((value >> (byte * 8U)) & 0xffU);
        }
    };
    header[0] = 'B';
    header[1] = 'M';
    write32(2, static_cast<std::uint32_t>(54U + pixelBytes));
    write32(10, 54U);
    write32(14, 40U);
    write32(18, static_cast<std::uint32_t>(image.width));
    write32(22, static_cast<std::uint32_t>(image.height));
    write16(26, 1U);
    write16(28, 24U);
    write32(34, static_cast<std::uint32_t>(pixelBytes));

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return {false, "Unable to create the BMP output file."};
    }
    stream.write(reinterpret_cast<const char*>(header.data()),
                 static_cast<std::streamsize>(header.size()));
    std::vector<std::uint8_t> row(stride, 0U);
    for (int y = image.height - 1; y >= 0; --y) {
        const std::uint8_t* source = image.pixels.data()
            + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) * 3U;
        for (int x = 0; x < image.width; ++x) {
            row[static_cast<std::size_t>(x) * 3U] = source[static_cast<std::size_t>(x) * 3U + 2U];
            row[static_cast<std::size_t>(x) * 3U + 1U]
                = source[static_cast<std::size_t>(x) * 3U + 1U];
            row[static_cast<std::size_t>(x) * 3U + 2U]
                = source[static_cast<std::size_t>(x) * 3U];
        }
        stream.write(reinterpret_cast<const char*>(row.data()),
                     static_cast<std::streamsize>(row.size()));
    }
    if (!stream) {
        return {false, "Unable to write the BMP output file."};
    }
    return {true, {}};
}

OperationResult LinuxImageCodec::writeJpeg(const std::filesystem::path& path,
                                            const LinuxRgbImage& image)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return {false, "Unable to create the JPEG output file."};
    }
    jpeg_compress_struct compressor{};
    JpegError errors;
    compressor.err = jpeg_std_error(&errors.manager);
    errors.manager.error_exit = &LinuxImageCodec::handleJpegError;
    if (setjmp(errors.recovery) != 0) {
        if (compressor.mem != nullptr) {
            jpeg_destroy_compress(&compressor);
        }
        std::fclose(file);
        return {false, "Unable to encode the JPEG output file: "
                           + std::string(errors.message) + "."};
    }
    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, file);
    compressor.image_width = static_cast<JDIMENSION>(image.width);
    compressor.image_height = static_cast<JDIMENSION>(image.height);
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    jpeg_set_quality(&compressor, 95, TRUE);
    jpeg_start_compress(&compressor, TRUE);
    while (compressor.next_scanline < compressor.image_height) {
        JSAMPROW row = const_cast<JSAMPROW>(
            image.pixels.data() + static_cast<std::size_t>(compressor.next_scanline)
                * static_cast<std::size_t>(image.width) * 3U);
        jpeg_write_scanlines(&compressor, &row, 1);
    }
    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);
    const bool closeSucceeded = std::fclose(file) == 0;
    return closeSucceeded ? OperationResult{true, {}}
                          : OperationResult{false, "Unable to finalize the JPEG output file."};
}

void LinuxImageCodec::handleJpegError(j_common_ptr compressor)
{
    auto* error = reinterpret_cast<JpegError*>(compressor->err);
    error->manager.format_message(compressor, error->message);
    longjmp(error->recovery, 1);
}

} // namespace desktopctl
