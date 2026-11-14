// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

/// A small JSON reader and writer for the save file. It handles objects, arrays, strings, 64-bit integers and booleans
/// and nothing else: a number with a fraction or an exponent is an error, and so is `null`, because nothing the core
/// saves is a floating-point number or an absent value. The reader is built to take hostile bytes: every limit below
/// is checked before the memory or the stack it protects is used.
namespace heart_boom_maths::json
{

/// Hard limits on the input. The defaults are far above a real save (a few kilobytes) and far below anything that could
/// exhaust memory or the stack.
struct Limits
{
    std::size_t max_bytes = 256 * 1024;
    int max_depth = 32;
    std::size_t max_string_bytes = 1024;
};

enum class ErrorCode
{
    TOO_LARGE,
    TOO_DEEP,
    STRING_TOO_LONG,
    UNEXPECTED_END,
    UNEXPECTED_CHARACTER,
    TRAILING_CONTENT,
    BAD_NUMBER,
    NUMBER_NOT_INTEGER,
    NUMBER_OUT_OF_RANGE,
    BAD_ESCAPE,
    BAD_UTF8,
    CONTROL_CHARACTER,
    DUPLICATE_KEY,
};

struct Error
{
    ErrorCode code;
    std::size_t offset; // byte position in the input where the problem was found
};

enum class Kind
{
    BOOLEAN,
    INTEGER,
    STRING,
    ARRAY,
    OBJECT,
};

/// One JSON value. An object keeps its members sorted by key with no repeats, so the writer's output depends only on
/// the content and never on the order members were added.
class Value
{
public:
    using Array = std::vector<Value>;
    using Member = std::pair<std::string, Value>;
    using Object = std::vector<Member>;

    Value() = default; // false

    [[nodiscard]] static Value boolean(bool value);
    [[nodiscard]] static Value integer(std::int64_t value);
    [[nodiscard]] static Value string(std::string value);
    [[nodiscard]] static Value array(Array items = {});
    [[nodiscard]] static Value object();

    /// An object from members in any order. Empty when a key repeats.
    [[nodiscard]] static std::optional<Value> object_from(Object members);

    [[nodiscard]] Kind kind() const noexcept;

    /// Each returns null when the value has another kind.
    [[nodiscard]] const bool* get_boolean() const noexcept;
    [[nodiscard]] const std::int64_t* get_integer() const noexcept;
    [[nodiscard]] const std::string* get_string() const noexcept;
    [[nodiscard]] const Array* get_array() const noexcept;
    [[nodiscard]] Array* get_array() noexcept;
    [[nodiscard]] const Object* get_object() const noexcept;

    /// Object only: the member with this key, or null.
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;

    /// Object only: adds or replaces a member. Returns false when this is not an object.
    bool set(std::string key, Value value);

    /// Object only: removes a member. Returns whether one was removed.
    bool erase(std::string_view key);

    friend bool operator==(const Value&, const Value&) = default;

private:
    std::variant<bool, std::int64_t, std::string, Array, Object> m_Data;
};

/// Reads one JSON document. The whole input must be that one value.
[[nodiscard]] std::expected<Value, Error> parse(std::string_view text, const Limits& limits = {});

/// Compact text with members in key order. A byte that is not valid UTF-8 in a string is written as U+FFFD, so the
/// output is always valid JSON and `parse` accepts it.
[[nodiscard]] std::string write(const Value& value);

} // namespace heart_boom_maths::json
