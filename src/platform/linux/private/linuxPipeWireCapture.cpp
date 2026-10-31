#include "platform/linux/private/linuxPipeWireCapture.h"

#include "platform/linux/private/linuxImageCodec.h"

#include <pipewire/pipewire.h>

#include <spa/buffer/buffer.h>
#include <spa/param/format-utils.h>
#include <spa/param/param.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

namespace desktopctl {

struct LinuxPipeWireCapture::Impl final {
    struct CapturedFrame {
        LinuxPipeWireStream stream;
        LinuxRgbImage image;
    };

    ~Impl()
    {
        releaseStream();
        if (coreListenerAttached) {
            spa_hook_remove(&coreListener);
            coreListenerAttached = false;
        }
        if (core != nullptr) {
            pw_core_disconnect(core);
            core = nullptr;
        }
        if (context != nullptr) {
            pw_context_destroy(context);
            context = nullptr;
        }
        if (mainLoop != nullptr) {
            pw_main_loop_destroy(mainLoop);
            mainLoop = nullptr;
        }
        if (pipeWireInitialized) {
            pw_deinit();
        }
    }

    OperationResult captureDesktop(int descriptor,
                                   const std::vector<LinuxPipeWireStream>& streams,
                                   const std::filesystem::path& outputPath)
    {
        if (descriptor < 0) {
            return {false, "The PipeWire remote descriptor is invalid."};
        }
        if (streams.empty()) {
            close(descriptor);
            return {false, "The Wayland portal did not provide a monitor stream."};
        }
        OperationResult result = initialize(descriptor);
        if (!result.success) {
            return result;
        }

        std::vector<CapturedFrame> frames;
        frames.reserve(streams.size());
        for (const LinuxPipeWireStream& streamInfo : streams) {
            CapturedFrame frame;
            frame.stream = streamInfo;
            result = captureFrame(streamInfo.nodeId, frame.image);
            if (!result.success) {
                return result;
            }
            frames.push_back(std::move(frame));
        }

        LinuxRgbImage desktop;
        result = compose(frames, desktop);
        if (!result.success) {
            return result;
        }
        return LinuxImageCodec::write(outputPath, desktop);
    }

    OperationResult initialize(int descriptor)
    {
        pw_init(nullptr, nullptr);
        pipeWireInitialized = true;
        mainLoop = pw_main_loop_new(nullptr);
        if (mainLoop == nullptr) {
            close(descriptor);
            return {false, "Unable to create the PipeWire main loop."};
        }
        context = pw_context_new(pw_main_loop_get_loop(mainLoop), nullptr, 0U);
        if (context == nullptr) {
            close(descriptor);
            return {false, "Unable to create the PipeWire context."};
        }
        core = pw_context_connect_fd(context, descriptor, nullptr, 0U);
        if (core == nullptr) {
            return {false, "Unable to connect to the authorized PipeWire remote: "
                               + std::string(std::strerror(errno)) + "."};
        }
        coreEvents = {};
        coreEvents.version = PW_VERSION_CORE_EVENTS;
        coreEvents.done = &Impl::handleCoreDone;
        coreEvents.error = &Impl::handleCoreError;
        pw_core_add_listener(core, &coreListener, &coreEvents, this);
        coreListenerAttached = true;
        coreReady = false;
        coreError.clear();
        timedOut = false;
        coreSyncSequence = pw_core_sync(core, PW_ID_CORE, 0);
        if (coreSyncSequence < 0) {
            return {false, "Unable to synchronize the authorized PipeWire remote."};
        }

        timeoutSource = pw_loop_add_timer(pw_main_loop_get_loop(mainLoop),
                                          &Impl::handleTimeout, this);
        if (timeoutSource == nullptr) {
            return {false, "Unable to create the PipeWire connection timeout."};
        }
        timespec timeout{5, 0};
        timespec interval{0, 0};
        pw_loop_update_timer(pw_main_loop_get_loop(mainLoop), timeoutSource,
                             &timeout, &interval, false);
        pw_main_loop_run(mainLoop);
        pw_loop_destroy_source(pw_main_loop_get_loop(mainLoop), timeoutSource);
        timeoutSource = nullptr;
        if (!coreError.empty()) {
            return {false, coreError};
        }
        if (timedOut || !coreReady) {
            return {false, "The authorized PipeWire remote did not synchronize within five "
                           "seconds."};
        }
        return {true, {}};
    }

    static void handleCoreDone(void* data, std::uint32_t id, int sequence)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr || id != PW_ID_CORE || sequence != self->coreSyncSequence) {
            return;
        }
        self->coreReady = true;
        pw_main_loop_quit(self->mainLoop);
    }

    static void handleCoreError(void* data, std::uint32_t, int, int result,
                                const char* message)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr) {
            return;
        }
        self->coreError = "The authorized PipeWire remote failed";
        if (message != nullptr && *message != '\0') {
            self->coreError += ": " + std::string(message);
        } else if (result < 0) {
            self->coreError += ": " + std::string(std::strerror(-result));
        }
        self->coreError += ".";
        pw_main_loop_quit(self->mainLoop);
    }

    OperationResult captureFrame(std::uint32_t nodeId, LinuxRgbImage& image)
    {
        releaseStream();
        frame = {};
        frameReady = false;
        timedOut = false;
        streamError.clear();
        streamState = "unconnected";
        negotiatedFormat = SPA_VIDEO_FORMAT_UNKNOWN;
        negotiatedWidth = 0;
        negotiatedHeight = 0;

        streamEvents = {};
        streamEvents.version = PW_VERSION_STREAM_EVENTS;
        streamEvents.state_changed = &Impl::handleStateChanged;
        streamEvents.param_changed = &Impl::handleParamChanged;
        streamEvents.process = &Impl::handleProcess;
        stream = pw_stream_new(
            core, "DesktopCTL screenshot",
            pw_properties_new("pipewire.client.reuse", "1", nullptr));
        if (stream == nullptr) {
            return {false, "Unable to create the PipeWire capture stream."};
        }
        pw_stream_add_listener(stream, &streamListener, &streamEvents, this);

        std::uint8_t podBuffer[2048]{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        const spa_rectangle minimumSize = SPA_RECTANGLE(1, 1);
        const spa_rectangle maximumSize = SPA_RECTANGLE(UINT32_MAX, UINT32_MAX);
        const spa_video_format acceptedFormats[] = {
            SPA_VIDEO_FORMAT_BGRA,
            SPA_VIDEO_FORMAT_RGBA,
            SPA_VIDEO_FORMAT_BGRx,
            SPA_VIDEO_FORMAT_RGBx,
        };
        const spa_pod* parameters[4]{};
        for (std::size_t index = 0; index < 4U; ++index) {
            parameters[index] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
                &builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
                SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video),
                SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
                SPA_FORMAT_VIDEO_format, SPA_POD_Id(acceptedFormats[index]),
                SPA_FORMAT_VIDEO_size,
                SPA_POD_CHOICE_RANGE_Rectangle(&minimumSize, &minimumSize, &maximumSize)));
        }
        const int connectResult = pw_stream_connect(
            stream, PW_DIRECTION_INPUT, nodeId,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
                                         | PW_STREAM_FLAG_MAP_BUFFERS),
            parameters, 4U);
        if (connectResult < 0) {
            releaseStream();
            return {false, "Unable to connect to the PipeWire monitor stream: "
                               + std::string(std::strerror(-connectResult)) + "."};
        }

        timeoutSource = pw_loop_add_timer(pw_main_loop_get_loop(mainLoop),
                                          &Impl::handleTimeout, this);
        if (timeoutSource == nullptr) {
            releaseStream();
            return {false, "Unable to create the PipeWire capture timeout."};
        }
        timespec timeout{10, 0};
        timespec interval{0, 0};
        pw_loop_update_timer(pw_main_loop_get_loop(mainLoop), timeoutSource,
                             &timeout, &interval, false);
        pw_main_loop_run(mainLoop);
        if (timeoutSource != nullptr) {
            pw_loop_destroy_source(pw_main_loop_get_loop(mainLoop), timeoutSource);
            timeoutSource = nullptr;
        }
        releaseStream();

        if (timedOut) {
            return {false, "The authorized PipeWire stream did not provide a frame within ten "
                           "seconds (stream state: " + streamState + ")."};
        }
        if (!streamError.empty()) {
            return {false, streamError};
        }
        if (!coreError.empty()) {
            return {false, coreError};
        }
        if (!frameReady) {
            return {false, "The PipeWire stream stopped without providing a frame."};
        }
        image = std::move(frame);
        return {true, {}};
    }

    void releaseStream()
    {
        if (stream != nullptr) {
            pw_stream_disconnect(stream);
            spa_hook_remove(&streamListener);
            pw_stream_destroy(stream);
            stream = nullptr;
        }
    }

    static void handleStateChanged(void* data, pw_stream_state, pw_stream_state state,
                                   const char* error)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr) {
            return;
        }
        self->streamState = pw_stream_state_as_string(state);
        if (state != PW_STREAM_STATE_ERROR) {
            return;
        }
        self->streamError = "The PipeWire capture stream failed";
        if (error != nullptr && *error != '\0') {
            self->streamError += ": " + std::string(error);
        }
        self->streamError += ".";
        pw_main_loop_quit(self->mainLoop);
    }

    static void handleParamChanged(void* data, std::uint32_t id, const spa_pod* parameter)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr || id != SPA_PARAM_Format || parameter == nullptr) {
            return;
        }
        spa_video_info_raw info{};
        if (spa_format_video_raw_parse(parameter, &info) < 0) {
            return;
        }
        self->negotiatedFormat = info.format;
        self->negotiatedWidth = static_cast<int>(info.size.width);
        self->negotiatedHeight = static_cast<int>(info.size.height);
        const std::uint64_t stride = static_cast<std::uint64_t>(info.size.width) * 4U;
        const std::uint64_t size = stride * static_cast<std::uint64_t>(info.size.height);
        if (stride > static_cast<std::uint64_t>(std::numeric_limits<int>::max())
            || size > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            self->streamError = "The negotiated PipeWire frame is too large.";
            pw_main_loop_quit(self->mainLoop);
            return;
        }

        std::uint8_t podBuffer[512]{};
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        const int dataTypes = (1 << SPA_DATA_MemFd) | (1 << SPA_DATA_MemPtr);
        const spa_pod* parameters[1]{};
        parameters[0] = static_cast<const spa_pod*>(spa_pod_builder_add_object(
            &builder, SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            SPA_PARAM_BUFFERS_size, SPA_POD_Int(static_cast<int>(size)),
            SPA_PARAM_BUFFERS_stride, SPA_POD_Int(static_cast<int>(stride)),
            SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
            SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(dataTypes)));
        const int updateResult = pw_stream_update_params(self->stream, parameters, 1U);
        if (updateResult < 0) {
            self->streamError = "Unable to configure PipeWire capture buffers: "
                + std::string(std::strerror(-updateResult)) + ".";
            pw_main_loop_quit(self->mainLoop);
        }
    }

    static void handleProcess(void* data)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr || self->stream == nullptr) {
            return;
        }
        pw_buffer* pipeWireBuffer = pw_stream_dequeue_buffer(self->stream);
        if (pipeWireBuffer == nullptr) {
            return;
        }
        spa_buffer* buffer = pipeWireBuffer->buffer;
        if (!self->frameReady && buffer != nullptr && buffer->n_datas > 0U
            && self->negotiatedWidth > 0 && self->negotiatedHeight > 0) {
            self->copyFrame(buffer->datas[0]);
        }
        pw_stream_queue_buffer(self->stream, pipeWireBuffer);
        if (self->frameReady || !self->streamError.empty()) {
            pw_main_loop_quit(self->mainLoop);
        }
    }

    void copyFrame(const spa_data& data)
    {
        if (data.data == nullptr || data.chunk == nullptr) {
            return;
        }
        const int bytesPerPixel = 4;
        const int stride = data.chunk->stride != 0
            ? data.chunk->stride
            : negotiatedWidth * bytesPerPixel;
        if (stride <= 0 || stride < negotiatedWidth * bytesPerPixel) {
            streamError = "The PipeWire frame stride is unsupported.";
            return;
        }
        const std::uint64_t requiredPayload =
            static_cast<std::uint64_t>(negotiatedHeight - 1)
                * static_cast<unsigned int>(stride)
            + static_cast<std::uint64_t>(negotiatedWidth * bytesPerPixel);
        const std::uint64_t requiredBuffer = static_cast<std::uint64_t>(data.chunk->offset)
            + requiredPayload;
        if (requiredBuffer > data.maxsize
            || requiredPayload > data.chunk->size) {
            streamError = "The PipeWire frame buffer is smaller than the negotiated image.";
            return;
        }
        if (negotiatedFormat != SPA_VIDEO_FORMAT_BGRx
            && negotiatedFormat != SPA_VIDEO_FORMAT_BGRA
            && negotiatedFormat != SPA_VIDEO_FORMAT_RGBx
            && negotiatedFormat != SPA_VIDEO_FORMAT_RGBA) {
            streamError = "The PipeWire stream selected an unsupported pixel format.";
            return;
        }

        frame.width = negotiatedWidth;
        frame.height = negotiatedHeight;
        frame.pixels.resize(static_cast<std::size_t>(frame.width)
                            * static_cast<std::size_t>(frame.height) * 3U);
        const auto* base = static_cast<const std::uint8_t*>(data.data) + data.chunk->offset;
        const bool blueFirst = negotiatedFormat == SPA_VIDEO_FORMAT_BGRx
            || negotiatedFormat == SPA_VIDEO_FORMAT_BGRA;
        for (int y = 0; y < frame.height; ++y) {
            const std::uint8_t* source = base + static_cast<std::size_t>(y)
                * static_cast<std::size_t>(stride);
            std::uint8_t* target = frame.pixels.data() + static_cast<std::size_t>(y)
                * static_cast<std::size_t>(frame.width) * 3U;
            for (int x = 0; x < frame.width; ++x) {
                const std::uint8_t* pixel = source + static_cast<std::size_t>(x) * 4U;
                target[static_cast<std::size_t>(x) * 3U] = blueFirst ? pixel[2] : pixel[0];
                target[static_cast<std::size_t>(x) * 3U + 1U] = pixel[1];
                target[static_cast<std::size_t>(x) * 3U + 2U] = blueFirst ? pixel[0] : pixel[2];
            }
        }
        frameReady = true;
    }

    static void handleTimeout(void* data, std::uint64_t)
    {
        auto* self = static_cast<Impl*>(data);
        if (self == nullptr) {
            return;
        }
        self->timedOut = true;
        pw_main_loop_quit(self->mainLoop);
    }

    OperationResult compose(std::vector<CapturedFrame>& frames, LinuxRgbImage& desktop) const
    {
        bool allPositioned = true;
        int fallbackX = 0;
        for (CapturedFrame& frameInfo : frames) {
            const int targetWidth = frameInfo.stream.hasSize ? frameInfo.stream.width
                                                             : frameInfo.image.width;
            const int targetHeight = frameInfo.stream.hasSize ? frameInfo.stream.height
                                                              : frameInfo.image.height;
            if (targetWidth <= 0 || targetHeight <= 0) {
                return {false, "The Wayland portal returned invalid monitor dimensions."};
            }
            if (frameInfo.image.width != targetWidth || frameInfo.image.height != targetHeight) {
                frameInfo.image = resize(frameInfo.image, targetWidth, targetHeight);
            }
            if (!frameInfo.stream.hasPosition) {
                allPositioned = false;
            }
        }

        int minimumX = 0;
        int minimumY = 0;
        int maximumX = 0;
        int maximumY = 0;
        bool first = true;
        for (CapturedFrame& frameInfo : frames) {
            if (!allPositioned) {
                frameInfo.stream.x = fallbackX;
                frameInfo.stream.y = 0;
                fallbackX += frameInfo.image.width;
            }
            const int right = frameInfo.stream.x + frameInfo.image.width;
            const int bottom = frameInfo.stream.y + frameInfo.image.height;
            if (first) {
                minimumX = frameInfo.stream.x;
                minimumY = frameInfo.stream.y;
                maximumX = right;
                maximumY = bottom;
                first = false;
            } else {
                minimumX = std::min(minimumX, frameInfo.stream.x);
                minimumY = std::min(minimumY, frameInfo.stream.y);
                maximumX = std::max(maximumX, right);
                maximumY = std::max(maximumY, bottom);
            }
        }
        const long long width = static_cast<long long>(maximumX) - minimumX;
        const long long height = static_cast<long long>(maximumY) - minimumY;
        if (width <= 0 || height <= 0 || width > std::numeric_limits<int>::max()
            || height > std::numeric_limits<int>::max()) {
            return {false, "The composed Wayland desktop dimensions are invalid."};
        }
        const std::uint64_t pixelBytes = static_cast<std::uint64_t>(width)
            * static_cast<std::uint64_t>(height) * 3U;
        if (pixelBytes > std::numeric_limits<std::size_t>::max()) {
            return {false, "The composed Wayland desktop image is too large."};
        }
        desktop.width = static_cast<int>(width);
        desktop.height = static_cast<int>(height);
        desktop.pixels.assign(static_cast<std::size_t>(pixelBytes), 0U);
        for (const CapturedFrame& frameInfo : frames) {
            const int destinationX = frameInfo.stream.x - minimumX;
            const int destinationY = frameInfo.stream.y - minimumY;
            for (int y = 0; y < frameInfo.image.height; ++y) {
                const std::uint8_t* source = frameInfo.image.pixels.data()
                    + static_cast<std::size_t>(y) * static_cast<std::size_t>(frameInfo.image.width)
                        * 3U;
                std::uint8_t* target = desktop.pixels.data()
                    + (static_cast<std::size_t>(destinationY + y)
                       * static_cast<std::size_t>(desktop.width)
                       + static_cast<std::size_t>(destinationX)) * 3U;
                std::memcpy(target, source,
                            static_cast<std::size_t>(frameInfo.image.width) * 3U);
            }
        }
        return {true, {}};
    }

    LinuxRgbImage resize(const LinuxRgbImage& source, int width, int height) const
    {
        LinuxRgbImage result;
        result.width = width;
        result.height = height;
        result.pixels.resize(static_cast<std::size_t>(width)
                             * static_cast<std::size_t>(height) * 3U);
        for (int y = 0; y < height; ++y) {
            const int sourceY = static_cast<int>(static_cast<long long>(y) * source.height
                                                 / height);
            for (int x = 0; x < width; ++x) {
                const int sourceX = static_cast<int>(static_cast<long long>(x) * source.width
                                                     / width);
                const std::size_t sourceOffset = (static_cast<std::size_t>(sourceY)
                                                  * static_cast<std::size_t>(source.width)
                                                  + static_cast<std::size_t>(sourceX)) * 3U;
                const std::size_t targetOffset = (static_cast<std::size_t>(y)
                                                  * static_cast<std::size_t>(width)
                                                  + static_cast<std::size_t>(x)) * 3U;
                std::copy_n(source.pixels.data() + sourceOffset, 3U,
                            result.pixels.data() + targetOffset);
            }
        }
        return result;
    }

    bool pipeWireInitialized = false;
    pw_main_loop* mainLoop = nullptr;
    pw_context* context = nullptr;
    pw_core* core = nullptr;
    spa_hook coreListener{};
    pw_core_events coreEvents{};
    bool coreListenerAttached = false;
    bool coreReady = false;
    int coreSyncSequence = -1;
    std::string coreError;
    pw_stream* stream = nullptr;
    spa_hook streamListener{};
    pw_stream_events streamEvents{};
    spa_source* timeoutSource = nullptr;
    spa_video_format negotiatedFormat = SPA_VIDEO_FORMAT_UNKNOWN;
    int negotiatedWidth = 0;
    int negotiatedHeight = 0;
    LinuxRgbImage frame;
    bool frameReady = false;
    bool timedOut = false;
    std::string streamError;
    std::string streamState = "unconnected";
};

LinuxPipeWireCapture::LinuxPipeWireCapture()
    : _impl(std::make_unique<Impl>())
{
}

LinuxPipeWireCapture::~LinuxPipeWireCapture() = default;

OperationResult LinuxPipeWireCapture::captureDesktop(
    int pipeWireDescriptor, const std::vector<LinuxPipeWireStream>& streams,
    const std::filesystem::path& outputPath)
{
    return _impl->captureDesktop(pipeWireDescriptor, streams, outputPath);
}

} // namespace desktopctl
