#include "mcpServerPrivate.h"

#include "jsonCodec.h"

#include <exception>
#include <iostream>
#include <utility>

namespace desktopctl {

int McpServerPrivate::run()
{
    constexpr std::size_t MaximumMessageSize = 4U * 1024U * 1024U;
    try {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            JsonValue request;
            std::string parseError;
            if (line.size() > MaximumMessageSize) {
                const JsonValue output = errorResponse(
                    JsonValue(), -32700, "JSON-RPC message exceeds the 4 MiB limit.");
                std::cout << JsonCodec::serialize(output) << '\n' << std::flush;
                continue;
            }
            if (!JsonCodec::parse(line, request, parseError)) {
                const JsonValue output = errorResponse(
                    JsonValue(), -32700, "Invalid JSON: " + parseError);
                std::cout << JsonCodec::serialize(output) << '\n' << std::flush;
                continue;
            }

            bool shouldStop = false;
            const std::optional<JsonValue> output = handleRequest(request, shouldStop);
            if (output) {
                std::cout << JsonCodec::serialize(*output) << '\n' << std::flush;
            }
            if (shouldStop) {
                break;
            }
        }
        toolDispatcher_.shutdown();
        return 0;
    } catch (const std::exception& exception) {
        toolDispatcher_.shutdown();
        std::cerr << "MCP server failure: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        toolDispatcher_.shutdown();
        std::cerr << "MCP server failure: unknown exception.\n";
        return 1;
    }
}

std::optional<JsonValue> McpServerPrivate::handleRequest(const JsonValue& request,
                                                         bool& shouldStop)
{
    shouldStop = false;
    if (!request.isObject()) {
        return errorResponse(JsonValue(), -32600, "JSON-RPC request must be an object.");
    }

    const JsonValue* version = request.find("jsonrpc");
    const JsonValue* method = request.find("method");
    const JsonValue* id = request.find("id");
    const bool notification = id == nullptr;
    if (version == nullptr || !version->isString() || version->string() != "2.0"
        || method == nullptr || !method->isString()) {
        if (notification) {
            return std::nullopt;
        }
        return errorResponse(*id, -32600, "Invalid JSON-RPC 2.0 request.");
    }

    JsonValue params(JsonValue::Object{});
    if (const JsonValue* requestParams = request.find("params")) {
        params = *requestParams;
    }
    bool methodFound = true;
    JsonValue result = handleMethod(method->string(), params, shouldStop, methodFound);
    if (notification) {
        return std::nullopt;
    }
    if (!methodFound) {
        return errorResponse(*id, -32601, "Method not found: " + method->string());
    }
    return response(*id, std::move(result));
}

JsonValue McpServerPrivate::handleMethod(const std::string& method, const JsonValue& params,
                                         bool& shouldStop, bool& methodFound)
{
    methodFound = true;
    if (method == "server/discover") {
        return JsonValue(JsonValue::Object{
            {"resultType", JsonValue("complete")},
            {"supportedVersions", JsonValue(JsonValue::Array{
                JsonValue("2026-07-28"), JsonValue("2025-06-18")
            })},
            {"capabilities", JsonValue(JsonValue::Object{
                {"tools", JsonValue(JsonValue::Object{})}
            })}
        });
    }
    if (method == "initialize") {
        std::string protocolVersion = "2025-06-18";
        if (params.isObject()) {
            const JsonValue* requestedVersion = params.find("protocolVersion");
            if (requestedVersion != nullptr && requestedVersion->isString()) {
                protocolVersion = requestedVersion->string();
            }
        }
        return JsonValue(JsonValue::Object{
            {"protocolVersion", JsonValue(protocolVersion)},
            {"capabilities", JsonValue(JsonValue::Object{
                {"tools", JsonValue(JsonValue::Object{{"listChanged", JsonValue(false)}})}
            })},
            {"serverInfo", JsonValue(JsonValue::Object{
                {"name", JsonValue("desktopctl")},
                {"version", JsonValue(DESKTOPCTL_VERSION)}
            })},
            {"instructions", JsonValue(
                "Call session_start before every other desktop tool. Activate the intended window "
                "before keyboard input. Stop the session when work completes; inactivity also "
                "closes the green indicator after three minutes.")}
        });
    }
    if (method == "notifications/initialized" || method == "notifications/cancelled"
        || method == "ping" || method == "shutdown") {
        return emptyObject();
    }
    if (method == "exit") {
        shouldStop = true;
        return emptyObject();
    }
    if (method == "tools/list") {
        return JsonValue(JsonValue::Object{
            {"tools", toolDispatcher_.toolDefinitions()}
        });
    }
    if (method == "tools/call") {
        if (!params.isObject()) {
            return toolDispatcher_.call("", JsonValue(JsonValue::Object{}));
        }
        const JsonValue* name = params.find("name");
        if (name == nullptr || !name->isString() || name->string().empty()) {
            return toolDispatcher_.call("", JsonValue(JsonValue::Object{}));
        }
        JsonValue arguments(JsonValue::Object{});
        if (const JsonValue* suppliedArguments = params.find("arguments")) {
            arguments = *suppliedArguments;
        }
        return toolDispatcher_.call(name->string(), arguments);
    }
    if (method == "resources/list") {
        return JsonValue(JsonValue::Object{{"resources", JsonValue(JsonValue::Array{})}});
    }
    if (method == "prompts/list") {
        return JsonValue(JsonValue::Object{{"prompts", JsonValue(JsonValue::Array{})}});
    }

    methodFound = false;
    return emptyObject();
}

JsonValue McpServerPrivate::response(const JsonValue& id, JsonValue result)
{
    return JsonValue(JsonValue::Object{
        {"jsonrpc", JsonValue("2.0")},
        {"id", id},
        {"result", std::move(result)}
    });
}

JsonValue McpServerPrivate::errorResponse(const JsonValue& id, std::int64_t code,
                                          const std::string& message)
{
    return JsonValue(JsonValue::Object{
        {"jsonrpc", JsonValue("2.0")},
        {"id", id},
        {"error", JsonValue(JsonValue::Object{
            {"code", JsonValue(code)},
            {"message", JsonValue(message)}
        })}
    });
}

JsonValue McpServerPrivate::emptyObject()
{
    return JsonValue(JsonValue::Object{});
}

} // namespace desktopctl
