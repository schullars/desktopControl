#include "jsonValue.h"

#include <charconv>
#include <utility>

namespace desktopctl {

JsonValue::JsonValue()
    : storage_(nullptr)
{
}

JsonValue::JsonValue(bool value)
    : storage_(value)
{
}

JsonValue::JsonValue(std::int64_t value)
    : storage_(JsonNumber{std::to_string(value)})
{
}

JsonValue::JsonValue(std::uint64_t value)
    : storage_(JsonNumber{std::to_string(value)})
{
}

JsonValue::JsonValue(const char* value)
    : storage_(std::string(value))
{
}

JsonValue::JsonValue(std::string value)
    : storage_(std::move(value))
{
}

JsonValue::JsonValue(Array value)
    : storage_(std::move(value))
{
}

JsonValue::JsonValue(Object value)
    : storage_(std::move(value))
{
}

JsonValue JsonValue::number(std::string value)
{
    JsonValue result;
    result.storage_ = JsonNumber{std::move(value)};
    return result;
}

bool JsonValue::isNull() const
{
    return std::holds_alternative<std::nullptr_t>(storage_);
}

bool JsonValue::isBoolean() const
{
    return std::holds_alternative<bool>(storage_);
}

bool JsonValue::isNumber() const
{
    return std::holds_alternative<JsonNumber>(storage_);
}

bool JsonValue::isString() const
{
    return std::holds_alternative<std::string>(storage_);
}

bool JsonValue::isArray() const
{
    return std::holds_alternative<Array>(storage_);
}

bool JsonValue::isObject() const
{
    return std::holds_alternative<Object>(storage_);
}

bool JsonValue::boolean() const
{
    return std::get<bool>(storage_);
}

const std::string& JsonValue::numberText() const
{
    return std::get<JsonNumber>(storage_).text;
}

const std::string& JsonValue::string() const
{
    return std::get<std::string>(storage_);
}

const JsonValue::Array& JsonValue::array() const
{
    return std::get<Array>(storage_);
}

const JsonValue::Object& JsonValue::object() const
{
    return std::get<Object>(storage_);
}

JsonValue::Array& JsonValue::array()
{
    return std::get<Array>(storage_);
}

JsonValue::Object& JsonValue::object()
{
    return std::get<Object>(storage_);
}

const JsonValue* JsonValue::find(const std::string& key) const
{
    if (!isObject()) {
        return nullptr;
    }
    const auto iterator = object().find(key);
    return iterator == object().end() ? nullptr : &iterator->second;
}

std::optional<std::int64_t> JsonValue::integer() const
{
    if (!isNumber()) {
        return std::nullopt;
    }
    const std::string& text = numberText();
    std::int64_t value = 0;
    const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
    if (conversion.ec != std::errc() || conversion.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace desktopctl
