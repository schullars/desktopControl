#include "commandResultFile.h"

#include "jsonCodec.h"

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <random>
#include <system_error>

#if defined(_WIN32)
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace desktopctl {

CommandResultFile::~CommandResultFile()
{
    restoreStreams();
    removeTemporary();
}

void CommandResultFile::FileCloser::operator()(std::FILE* file) const
{
    std::fclose(file);
}

bool CommandResultFile::begin(const std::string& path, std::string& error)
{
    if (path.empty() || path.find('\0') != std::string::npos || started_) {
        error = "Invalid result-file path or result transport already active.";
        return false;
    }
    try {
        destination_ = std::filesystem::absolute(std::filesystem::u8path(path));
        std::error_code statusError;
        const auto status = std::filesystem::symlink_status(destination_, statusError);
        // symlink_status also rejects dangling links: never replace an existing entry.
        if (std::filesystem::exists(status)
            || (statusError && statusError != std::errc::no_such_file_or_directory)) {
            error = "Result-file destination already exists or cannot be inspected.";
            return false;
        }
        if (!reserveTemporary(error)) {
            return false;
        }
        std::cout.flush();
        std::cerr.flush();
        previousStdout_ = std::cout.rdbuf(stdout_.rdbuf());
        previousStderr_ = std::cerr.rdbuf(stderr_.rdbuf());
        started_ = true;
        return true;
    } catch (const std::exception&) {
        error = "Cannot prepare result-file destination.";
        restoreStreams();
        removeTemporary();
        return false;
    }
}

bool CommandResultFile::reserveTemporary(std::string& error)
{
    std::random_device random;
    for (int attempt = 0; attempt < 16; ++attempt) {
        auto candidate = destination_;
        candidate += ".pending-" + std::to_string(random()) + "-" + std::to_string(random());
        int descriptor = -1;
#if defined(_WIN32)
        const auto openError = _wsopen_s(&descriptor, candidate.c_str(),
            _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY | _O_NOINHERIT,
            _SH_DENYRW, _S_IREAD | _S_IWRITE);
        if (openError != 0) {
            if (openError == EEXIST) {
                continue;
            }
            error = "Cannot create result file in the destination directory.";
            return false;
        }
#else
        descriptor = ::open(candidate.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600);
        if (descriptor < 0) {
            if (errno == EEXIST) {
                continue;
            }
            error = "Cannot create result file in the destination directory.";
            return false;
        }
#endif
        temporary_ = std::move(candidate);
#if defined(_WIN32)
        file_.reset(_fdopen(descriptor, "wb"));
        if (!file_) {
            _close(descriptor);
        }
#else
        file_.reset(::fdopen(descriptor, "wb"));
        if (!file_) {
            ::close(descriptor);
        }
#endif
        if (!file_) {
            error = "Cannot open result-file output stream.";
            removeTemporary();
            return false;
        }
        return true;
    }
    error = "Cannot reserve a unique result file.";
    return false;
}

int CommandResultFile::finish(int exitCode)
{
    restoreStreams();
    if (!file_) {
        std::cerr << "Result-file transport is not active.\n";
        return 3;
    }
    const std::string output = stdout_.str();
    const std::string errors = stderr_.str();
    // Shell-launched UIAccess processes may have no usable console. Publication does not
    // depend on console handles, while direct CLI callers still receive their usual bytes.
    std::cout << output;
    std::cerr << errors;
    try {
        const std::string payload = JsonCodec::serialize(JsonValue(JsonValue::Object{
            {"schemaVersion", JsonValue(std::int64_t{1})},
            {"exitCode", JsonValue(static_cast<std::int64_t>(exitCode))},
            {"stdout", JsonValue(output)},
            {"stderr", JsonValue(errors)},
        })) + "\n";
        if (std::fwrite(payload.data(), 1, payload.size(), file_.get()) != payload.size()
            || std::fflush(file_.get()) != 0) {
            std::cerr << "Cannot write result file.\n";
            removeTemporary();
            return 3;
        }
        if (std::fclose(file_.release()) != 0) {
            std::cerr << "Cannot close result file.\n";
            removeTemporary();
            return 3;
        }
        std::string error;
        if (!publish(error)) {
            std::cerr << error << '\n';
            removeTemporary();
            return 3;
        }
        return exitCode;
    } catch (const std::exception&) {
        std::cerr << "Cannot serialize or publish result file.\n";
        removeTemporary();
        return 3;
    }
}

bool CommandResultFile::publish(std::string& error)
{
    // Both operations fail if the destination appeared while the command was running.
    // Readers see either no file or the complete JSON, never a partially written result.
#if defined(_WIN32)
    if (!MoveFileExW(temporary_.c_str(), destination_.c_str(), MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot publish result file without replacing an existing destination.";
        return false;
    }
    temporary_.clear();
#else
    if (::link(temporary_.c_str(), destination_.c_str()) != 0) {
        error = "Cannot publish result file without replacing an existing destination.";
        return false;
    }
    removeTemporary();
#endif
    return true;
}

void CommandResultFile::restoreStreams()
{
    if (previousStderr_) {
        std::cerr.rdbuf(previousStderr_);
        previousStderr_ = nullptr;
    }
    if (previousStdout_) {
        std::cout.rdbuf(previousStdout_);
        previousStdout_ = nullptr;
    }
}

void CommandResultFile::removeTemporary()
{
    file_.reset();
    if (!temporary_.empty()) {
        std::error_code ignored;
        std::filesystem::remove(temporary_, ignored);
        if (!ignored) {
            temporary_.clear();
        }
    }
}

} // namespace desktopctl
