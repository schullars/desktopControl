#include "mcpServer.h"

#include "private/mcpServerPrivate.h"

#include <memory>

namespace desktopctl {

McpServer::McpServer()
    : _d(std::make_unique<McpServerPrivate>())
{
}

McpServer::~McpServer() = default;

int McpServer::run()
{
    return _d->run();
}

} // namespace desktopctl
