#include "commandLineApplication.h"
#include "mcpServer.h"

#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>

int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(std::filesystem::path(argv[index]).u8string());
    }

    if (arguments.size() == 3U && arguments[1] == "mcp" && arguments[2] == "serve") {
        desktopctl::McpServer server;
        return server.run();
    }

    desktopctl::CommandLineApplication application;
    return application.run(arguments);
}
#else
int main(int argc, char* argv[])
{
    std::vector<std::string> arguments;
    arguments.reserve(static_cast<std::size_t>(argc));
    for (int index = 0; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }

    if (arguments.size() == 3U && arguments[1] == "mcp" && arguments[2] == "serve") {
        desktopctl::McpServer server;
        return server.run();
    }

    desktopctl::CommandLineApplication application;
    return application.run(arguments);
}
#endif
