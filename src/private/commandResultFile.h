#pragma once

#include <cstdio>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

namespace desktopctl {

// Module-private, single-invocation result transport. The CLI owns its lifetime.
class CommandResultFile final {
public:
    CommandResultFile() = default;
    ~CommandResultFile();

    CommandResultFile(const CommandResultFile&) = delete;
    CommandResultFile& operator=(const CommandResultFile&) = delete;

    // Reserves a private sibling file before any command runs, then captures C++ output.
    // The UTF-8 destination must not exist; its parent directory must already exist.
    bool begin(const std::string& path, std::string& error);

    // Restores/replays console output and atomically publishes schema version 1.
    // Returns the command exit code, or 3 if the result cannot be published.
    int finish(int exitCode);

private:
    struct FileCloser {
        void operator()(std::FILE* file) const;
    };

    bool reserveTemporary(std::string& error);
    bool publish(std::string& error);
    void restoreStreams();
    void removeTemporary();

    std::filesystem::path destination_;
    std::filesystem::path temporary_;
    std::unique_ptr<std::FILE, FileCloser> file_;
    std::ostringstream stdout_;
    std::ostringstream stderr_;
    std::streambuf* previousStdout_ = nullptr;
    std::streambuf* previousStderr_ = nullptr;
    bool started_ = false;
};

} // namespace desktopctl
