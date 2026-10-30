#pragma once

#include "jsonValue.h"
#include "mcpToolDispatcher.h"

#include <optional>
#include <string>

namespace desktopctl {

class McpServerPrivate final {
public:
    int run();

private:
    std::optional<JsonValue> handleRequest(const JsonValue& request, bool& shouldStop);
    JsonValue handleMethod(const std::string& method, const JsonValue& params, bool& shouldStop,
                           bool& methodFound);
    static JsonValue response(const JsonValue& id, JsonValue result);
    static JsonValue errorResponse(const JsonValue& id, std::int64_t code,
                                   const std::string& message);
    static JsonValue emptyObject();

    McpToolDispatcher toolDispatcher_;
};

} // namespace desktopctl
