#include "jsonCodec.h"

#include <cctype>
#include <utility>

namespace desktopctl {

bool JsonCodec::parse(const std::string& text, JsonValue& value, std::string& error)
{
    return Parser(text).parse(value, error);
}

std::string JsonCodec::serialize(const JsonValue& value)
{
    std::string output;
    appendSerialized(value, output);
    return output;
}

JsonCodec::Parser::Parser(std::string_view text)
    : text_(text)
{
}

bool JsonCodec::Parser::parse(JsonValue& value, std::string& error)
{
    skipWhitespace();
    if (!parseValue(value, error, 0)) {
        return false;
    }
    skipWhitespace();
    if (position_ != text_.size()) {
        return fail("Unexpected characters after the JSON value", error);
    }
    return true;
}

bool JsonCodec::Parser::parseValue(JsonValue& value, std::string& error, std::size_t depth)
{
    if (depth > 64) {
        return fail("JSON nesting exceeds 64 levels", error);
    }
    skipWhitespace();
    if (position_ >= text_.size()) {
        return fail("Unexpected end of JSON input", error);
    }

    const char character = text_[position_];
    if (character == '{') {
        return parseObject(value, error, depth + 1);
    }
    if (character == '[') {
        return parseArray(value, error, depth + 1);
    }
    if (character == '"') {
        std::string text;
        if (!parseString(text, error)) {
            return false;
        }
        value = JsonValue(std::move(text));
        return true;
    }
    if (character == 't') {
        return parseLiteral("true", JsonValue(true), value, error);
    }
    if (character == 'f') {
        return parseLiteral("false", JsonValue(false), value, error);
    }
    if (character == 'n') {
        return parseLiteral("null", JsonValue(), value, error);
    }
    if (character == '-' || (character >= '0' && character <= '9')) {
        return parseNumber(value, error);
    }
    return fail("Unexpected character in JSON value", error);
}

bool JsonCodec::Parser::parseObject(JsonValue& value, std::string& error, std::size_t depth)
{
    consume('{');
    JsonValue::Object object;
    skipWhitespace();
    if (consume('}')) {
        value = JsonValue(std::move(object));
        return true;
    }

    while (true) {
        skipWhitespace();
        if (position_ >= text_.size() || text_[position_] != '"') {
            return fail("JSON object key must be a string", error);
        }
        std::string key;
        if (!parseString(key, error)) {
            return false;
        }
        skipWhitespace();
        if (!consume(':')) {
            return fail("JSON object key must be followed by a colon", error);
        }
        JsonValue member;
        if (!parseValue(member, error, depth)) {
            return false;
        }
        object.insert_or_assign(std::move(key), std::move(member));
        skipWhitespace();
        if (consume('}')) {
            value = JsonValue(std::move(object));
            return true;
        }
        if (!consume(',')) {
            return fail("JSON object members must be separated by a comma", error);
        }
    }
}

bool JsonCodec::Parser::parseArray(JsonValue& value, std::string& error, std::size_t depth)
{
    consume('[');
    JsonValue::Array array;
    skipWhitespace();
    if (consume(']')) {
        value = JsonValue(std::move(array));
        return true;
    }

    while (true) {
        JsonValue member;
        if (!parseValue(member, error, depth)) {
            return false;
        }
        array.push_back(std::move(member));
        skipWhitespace();
        if (consume(']')) {
            value = JsonValue(std::move(array));
            return true;
        }
        if (!consume(',')) {
            return fail("JSON array members must be separated by a comma", error);
        }
    }
}

bool JsonCodec::Parser::parseString(std::string& value, std::string& error)
{
    consume('"');
    value.clear();
    while (position_ < text_.size()) {
        const unsigned char character = static_cast<unsigned char>(text_[position_++]);
        if (character == '"') {
            return true;
        }
        if (character < 0x20U) {
            return fail("JSON strings cannot contain unescaped control characters", error);
        }
        if (character != '\\') {
            value.push_back(static_cast<char>(character));
            continue;
        }
        if (position_ >= text_.size()) {
            return fail("JSON string ends after an escape character", error);
        }
        const char escaped = text_[position_++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            value.push_back(escaped);
            break;
        case 'b':
            value.push_back('\b');
            break;
        case 'f':
            value.push_back('\f');
            break;
        case 'n':
            value.push_back('\n');
            break;
        case 'r':
            value.push_back('\r');
            break;
        case 't':
            value.push_back('\t');
            break;
        case 'u': {
            std::uint32_t codePoint = 0;
            if (!parseHexQuad(codePoint, error)) {
                return false;
            }
            if (codePoint >= 0xD800U && codePoint <= 0xDBFFU) {
                if (position_ + 2U > text_.size() || text_[position_] != '\\'
                    || text_[position_ + 1U] != 'u') {
                    return fail("High surrogate must be followed by a low surrogate", error);
                }
                position_ += 2U;
                std::uint32_t lowSurrogate = 0;
                if (!parseHexQuad(lowSurrogate, error)) {
                    return false;
                }
                if (lowSurrogate < 0xDC00U || lowSurrogate > 0xDFFFU) {
                    return fail("Invalid low surrogate in JSON string", error);
                }
                codePoint = 0x10000U + ((codePoint - 0xD800U) << 10U)
                    + (lowSurrogate - 0xDC00U);
            } else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU) {
                return fail("Unexpected low surrogate in JSON string", error);
            }
            appendUtf8(codePoint, value);
            break;
        }
        default:
            return fail("Invalid JSON string escape", error);
        }
    }
    return fail("Unterminated JSON string", error);
}

bool JsonCodec::Parser::parseNumber(JsonValue& value, std::string& error)
{
    const std::size_t start = position_;
    consume('-');
    if (position_ >= text_.size()) {
        return fail("Incomplete JSON number", error);
    }
    if (text_[position_] == '0') {
        ++position_;
    } else if (text_[position_] >= '1' && text_[position_] <= '9') {
        while (position_ < text_.size() && std::isdigit(
                   static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    } else {
        return fail("Invalid JSON number", error);
    }
    if (position_ < text_.size() && text_[position_] == '.') {
        ++position_;
        const std::size_t fractionStart = position_;
        while (position_ < text_.size() && std::isdigit(
                   static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
        if (position_ == fractionStart) {
            return fail("JSON number fraction requires a digit", error);
        }
    }
    if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
        ++position_;
        if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
            ++position_;
        }
        const std::size_t exponentStart = position_;
        while (position_ < text_.size() && std::isdigit(
                   static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
        if (position_ == exponentStart) {
            return fail("JSON number exponent requires a digit", error);
        }
    }
    value = JsonValue::number(std::string(text_.substr(start, position_ - start)));
    return true;
}

bool JsonCodec::Parser::parseLiteral(std::string_view literal, JsonValue value,
                                     JsonValue& output, std::string& error)
{
    if (text_.substr(position_, literal.size()) != literal) {
        return fail("Invalid JSON literal", error);
    }
    position_ += literal.size();
    output = std::move(value);
    return true;
}

bool JsonCodec::Parser::parseHexQuad(std::uint32_t& value, std::string& error)
{
    if (position_ + 4U > text_.size()) {
        return fail("Incomplete Unicode escape in JSON string", error);
    }
    value = 0;
    for (std::size_t index = 0; index < 4U; ++index) {
        const int digit = hexValue(text_[position_++]);
        if (digit < 0) {
            return fail("Invalid Unicode escape in JSON string", error);
        }
        value = (value << 4U) | static_cast<std::uint32_t>(digit);
    }
    return true;
}

void JsonCodec::Parser::skipWhitespace()
{
    while (position_ < text_.size()) {
        const char character = text_[position_];
        if (character != ' ' && character != '\t' && character != '\r' && character != '\n') {
            return;
        }
        ++position_;
    }
}

bool JsonCodec::Parser::consume(char character)
{
    if (position_ >= text_.size() || text_[position_] != character) {
        return false;
    }
    ++position_;
    return true;
}

bool JsonCodec::Parser::fail(const std::string& message, std::string& error) const
{
    error = message + " at byte " + std::to_string(position_);
    return false;
}

int JsonCodec::Parser::hexValue(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

void JsonCodec::Parser::appendUtf8(std::uint32_t codePoint, std::string& output)
{
    if (codePoint <= 0x7FU) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else if (codePoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
}

void JsonCodec::appendSerialized(const JsonValue& value, std::string& output)
{
    if (value.isNull()) {
        output += "null";
    } else if (value.isBoolean()) {
        output += value.boolean() ? "true" : "false";
    } else if (value.isNumber()) {
        output += value.numberText();
    } else if (value.isString()) {
        appendString(value.string(), output);
    } else if (value.isArray()) {
        output.push_back('[');
        for (std::size_t index = 0; index < value.array().size(); ++index) {
            if (index != 0U) {
                output.push_back(',');
            }
            appendSerialized(value.array()[index], output);
        }
        output.push_back(']');
    } else {
        output.push_back('{');
        std::size_t index = 0;
        for (const auto& member : value.object()) {
            if (index++ != 0U) {
                output.push_back(',');
            }
            appendString(member.first, output);
            output.push_back(':');
            appendSerialized(member.second, output);
        }
        output.push_back('}');
    }
}

void JsonCodec::appendString(const std::string& value, std::string& output)
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20U) {
                output += "\\u00";
                output.push_back(HexDigits[(character >> 4U) & 0x0FU]);
                output.push_back(HexDigits[character & 0x0FU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

} // namespace desktopctl
