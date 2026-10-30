#pragma once

#include <memory>

namespace desktopctl {

class McpServerPrivate;

class McpServer final {
public:
    /// <summary>
    /// Creates the local Model Context Protocol stdio server.
    /// </summary>
    McpServer();
    ~McpServer();

    McpServer(const McpServer&) = delete;
    McpServer& operator=(const McpServer&) = delete;

    /// <summary>
    /// Processes newline-delimited JSON-RPC messages until standard input closes or exit arrives.
    /// </summary>
    /// <returns>Zero after a normal shutdown, or one after an unrecoverable server error.</returns>
    int run();

private:
    std::unique_ptr<McpServerPrivate> _d;
};

} // namespace desktopctl
