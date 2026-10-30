#pragma once

#include "jsonValue.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace desktopctl {

class JsonCodec final {
public:
    static bool parse(const std::string& text, JsonValue& value, std::string& error);
    static std::string serialize(const JsonValue& value);

private:
    class Parser final {
    public:
        explicit Parser(std::string_view text);
        bool parse(JsonValue& value, std::string& error);

    private:
        bool parseValue(JsonValue& value, std::string& error, std::size_t depth);
        bool parseObject(JsonValue& value, std::string& error, std::size_t depth);
        bool parseArray(JsonValue& value, std::string& error, std::size_t depth);
        bool parseString(std::string& value, std::string& error);
        bool parseNumber(JsonValue& value, std::string& error);
        bool parseLiteral(std::string_view literal, JsonValue value, JsonValue& output,
                          std::string& error);
        bool parseHexQuad(std::uint32_t& value, std::string& error);
        void skipWhitespace();
        bool consume(char character);
        bool fail(const std::string& message, std::string& error) const;
        static int hexValue(char character);
        static void appendUtf8(std::uint32_t codePoint, std::string& output);

        std::string_view text_;
        std::size_t position_ = 0;
    };

    static void appendSerialized(const JsonValue& value, std::string& output);
    static void appendString(const std::string& value, std::string& output);
};

} // namespace desktopctl
