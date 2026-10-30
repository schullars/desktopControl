#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace desktopctl {

struct JsonNumber {
    std::string text;
};

class JsonValue final {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue();
    explicit JsonValue(bool value);
    explicit JsonValue(std::int64_t value);
    explicit JsonValue(std::uint64_t value);
    explicit JsonValue(const char* value);
    explicit JsonValue(std::string value);
    explicit JsonValue(Array value);
    explicit JsonValue(Object value);

    static JsonValue number(std::string value);

    bool isNull() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    bool boolean() const;
    const std::string& numberText() const;
    const std::string& string() const;
    const Array& array() const;
    const Object& object() const;
    Array& array();
    Object& object();

    const JsonValue* find(const std::string& key) const;
    std::optional<std::int64_t> integer() const;

private:
    using Storage = std::variant<std::nullptr_t, bool, JsonNumber, std::string, Array, Object>;
    Storage storage_;
};

} // namespace desktopctl
