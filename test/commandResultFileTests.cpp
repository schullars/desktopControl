#include "private/commandResultFile.h"
#include "private/jsonCodec.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

class ResultFileTests final {
public:
    ResultFileTests()
    {
        root_ = std::filesystem::temp_directory_path()
            / ("desktopctl-result-tests-" + std::to_string(std::random_device{}()));
        require(std::filesystem::create_directory(root_), "create isolated test directory");
        previousOutput_ = std::cout.rdbuf(output_.rdbuf());
        previousErrors_ = std::cerr.rdbuf(errors_.rdbuf());
    }

    ~ResultFileTests()
    {
        std::cout.rdbuf(previousOutput_);
        std::cerr.rdbuf(previousErrors_);
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    void run()
    {
        for (const int code : {0, 2, 3}) {
            roundTrip(code);
        }
        existingDestination();
        invalidDestination();
        interruptedCapture();
        publicationRace();
        symbolicLink();
    }

private:
    static void require(bool condition, const std::string& message)
    {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    static std::string read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    void roundTrip(int code)
    {
        const auto path = root_ / std::filesystem::u8path(
            u8"result \u6d4b\u8bd5 \U0001f600-" + std::to_string(code) + ".json");
        std::string error;
        desktopctl::CommandResultFile result;
        require(result.begin(path.u8string(), error), "begin: " + error);
        require(!std::filesystem::exists(path), "no partial result is visible");
        const std::string out = std::string(u8"quote\" slash\\ newline\n\u6d4b\u8bd5\U0001f600\t")
            + std::string("nul\0byte", 8);
        const std::string err = "failure\r\n\b\f\x01";
        const auto outputOffset = output_.str().size();
        const auto errorOffset = errors_.str().size();
        std::cout << out;
        std::cerr << err;
        require(result.finish(code) == code, "preserve command exit code");
        require(output_.str().substr(outputOffset) == out, "preserve console stdout");
        require(errors_.str().substr(errorOffset) == err, "preserve console stderr");
        desktopctl::JsonValue envelope;
        const auto bytes = read(path);
        require(bytes.substr(0, 3) != "\xef\xbb\xbf", "UTF-8 without a BOM");
        require(desktopctl::JsonCodec::parse(bytes, envelope, error), "parse envelope: " + error);
        require(envelope.object().size() == 4U, "exact schema fields");
        require(envelope.find("schemaVersion")->integer() == 1, "schema version");
        require(envelope.find("exitCode")->integer() == code, "envelope exit code");
        require(envelope.find("stdout")->string() == out, "stdout UTF-8 and JSON escaping");
        require(envelope.find("stderr")->string() == err, "stderr JSON escaping");
        require(!result.begin((root_ / "reuse.json").u8string(), error),
                "single invocation cannot be reused");
        const auto repeatedOffset = output_.str().size();
        require(result.finish(0) == 3 && output_.str().size() == repeatedOffset,
                "repeated finish cannot replay output or republish a stale result");
#if !defined(_WIN32)
        struct stat status{};
        require(::stat(path.c_str(), &status) == 0 && (status.st_mode & 0777) == 0600,
                "result is private to its owner");
#endif
    }

    void existingDestination()
    {
        const auto path = root_ / "existing.json";
        std::ofstream(path) << "do not replace";
        desktopctl::CommandResultFile result;
        std::string error;
        require(!result.begin(path.u8string(), error), "reject existing file");
        require(read(path) == "do not replace", "existing file remains unchanged");
    }

    void invalidDestination()
    {
        for (const auto& path : {std::string(), (root_ / "missing" / "result.json").u8string(),
                                 root_.u8string(), std::string("bad\0path", 8)}) {
            desktopctl::CommandResultFile result;
            std::string error;
            require(!result.begin(path, error) && !error.empty(), "reject invalid destination");
        }
    }

    void interruptedCapture()
    {
        const auto path = root_ / "interrupted.json";
        const auto entries = std::distance(std::filesystem::directory_iterator(root_),
                                           std::filesystem::directory_iterator());
        {
            desktopctl::CommandResultFile result;
            std::string error;
            require(result.begin(path.u8string(), error), "begin abandoned invocation");
            std::cout << "never published";
        }
        require(!std::filesystem::exists(path), "abandoned invocation has no final file");
        require(entries == std::distance(std::filesystem::directory_iterator(root_),
                                         std::filesystem::directory_iterator()),
                "abandoned invocation removes its own pending file");
        const auto offset = output_.str().size();
        std::cout << "restored";
        require(output_.str().substr(offset) == "restored", "destructor restores stream buffers");
    }

    void publicationRace()
    {
        const auto path = root_ / "race.json";
        desktopctl::CommandResultFile result;
        std::string error;
        require(result.begin(path.u8string(), error), "begin race test");
        std::ofstream(path) << "concurrent owner";
        std::cout << "result";
        require(result.finish(0) == 3, "publication collision fails closed");
        require(read(path) == "concurrent owner", "publication never overwrites a raced file");
        for (const auto& entry : std::filesystem::directory_iterator(root_)) {
            require(entry.path().filename().u8string().find(".pending-") == std::string::npos,
                    "failed publication removes pending output");
        }
    }

    void symbolicLink()
    {
        const auto path = root_ / "dangling.json";
        std::error_code errorCode;
        std::filesystem::create_symlink(root_ / "absent-target.json", path, errorCode);
        if (errorCode) {
#if defined(_WIN32)
            // Windows may require Developer Mode or elevation to create a symlink.
            return;
#else
            require(false, "create symlink fixture");
#endif
        }
        desktopctl::CommandResultFile result;
        std::string error;
        require(!result.begin(path.u8string(), error), "reject dangling symlink");
        require(!std::filesystem::exists(root_ / "absent-target.json"), "never follow symlink");
    }

    std::filesystem::path root_;
    std::ostringstream output_;
    std::ostringstream errors_;
    std::streambuf* previousOutput_ = nullptr;
    std::streambuf* previousErrors_ = nullptr;
};

int main()
{
    try {
        ResultFileTests tests;
        tests.run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Result-file component tests passed.\n";
    return 0;
}
