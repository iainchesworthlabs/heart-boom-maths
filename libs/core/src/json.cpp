// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/json.hpp"

#include "heart_boom_maths/profile.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <iterator>
#include <limits>

namespace heart_boom_maths::json
{

namespace
{

constexpr std::string_view REPLACEMENT_CHARACTER = "\xEF\xBF\xBD";

// How many items or members the parser makes room for on its first array or object: more than a save holds at once,
// so a save is read without the stack growing. It is only the start; a larger document grows it as a vector does.
constexpr std::size_t STACK_START = 48;

// How many items or members one array or object keeps on that stack before it moves to a vector of its own.
constexpr std::size_t SPILL_AT = 128;

// The number of bytes of the strictly valid UTF-8 sequence that starts at `pos`, or 0 when there is none. Overlong
// forms, surrogates and anything above U+10FFFF are refused, so a string that passes can be handed to any text API.
std::size_t utf8_sequence_length(std::string_view text, std::size_t pos) noexcept
{
    const auto at = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const auto continuation = [&](std::size_t i, unsigned low, unsigned high)
    { return i < text.size() && at(i) >= low && at(i) <= high; };

    const unsigned lead = at(pos);
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        return continuation(pos + 1, 0x80, 0xBF) ? 2 : 0;
    }
    if (lead >= 0xE0 && lead <= 0xEF)
    {
        const unsigned low = lead == 0xE0 ? 0xA0 : 0x80;
        const unsigned high = lead == 0xED ? 0x9F : 0xBF;
        return continuation(pos + 1, low, high) && continuation(pos + 2, 0x80, 0xBF) ? 3 : 0;
    }
    if (lead >= 0xF0 && lead <= 0xF4)
    {
        const unsigned low = lead == 0xF0 ? 0x90 : 0x80;
        const unsigned high = lead == 0xF4 ? 0x8F : 0xBF;
        return continuation(pos + 1, low, high) && continuation(pos + 2, 0x80, 0xBF) &&
                       continuation(pos + 3, 0x80, 0xBF)
                   ? 4
                   : 0;
    }
    return 0;
}

void append_code_point(std::string& out, std::uint32_t code_point)
{
    if (code_point < 0x80)
    {
        out.push_back(static_cast<char>(code_point));
    }
    else if (code_point < 0x800)
    {
        out.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else if (code_point < 0x10000)
    {
        out.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

bool key_less(const Value::Member& a, const Value::Member& b) noexcept
{
    return a.first < b.first;
}

// A character that stands for itself in a string and needs no decoding, escaping or checking: printable ASCII except
// the quote and the backslash.
bool is_plain(char c) noexcept
{
    const auto byte = static_cast<unsigned char>(c);
    return byte >= 0x20 && byte < 0x80 && c != '"' && c != '\\';
}

// The items or members of one array or object while it is being read. They wait on a stack the whole parse shares and
// move into a vector of exactly their number at the end, so a small array costs one allocation and not one for each
// time it would have grown. Past SPILL_AT they move to a vector of their own, which grows as a vector does: for a very
// long array the stack would only add a second pass of moves. An error ends the parse, so the stack is not tidied on
// the way out.
template <typename Vector>
class Pending
{
public:
    explicit Pending(Vector& stack) : m_Stack(stack), m_Base(stack.size()) {}

    template <typename... Args>
    void add(Args&&... args)
    {
        if (m_Spilled)
        {
            m_Own.emplace_back(std::forward<Args>(args)...);
            return;
        }
        if (m_Stack.capacity() == 0)
        {
            m_Stack.reserve(STACK_START); // a save fits, so the stack grows no more
        }
        m_Stack.emplace_back(std::forward<Args>(args)...);
        if (m_Stack.size() - m_Base >= SPILL_AT)
        {
            const auto first = m_Stack.begin() + static_cast<std::ptrdiff_t>(m_Base);
            m_Own.assign(std::make_move_iterator(first), std::make_move_iterator(m_Stack.end()));
            m_Stack.erase(first, m_Stack.end());
            m_Spilled = true;
        }
    }

    [[nodiscard]] Vector take()
    {
        if (m_Spilled)
        {
            return std::move(m_Own);
        }
        const auto first = m_Stack.begin() + static_cast<std::ptrdiff_t>(m_Base);
        Vector result(std::make_move_iterator(first), std::make_move_iterator(m_Stack.end()));
        m_Stack.erase(first, m_Stack.end());
        return result;
    }

private:
    Vector& m_Stack;
    std::size_t m_Base;
    Vector m_Own;
    bool m_Spilled = false;
};

class Parser
{
public:
    Parser(std::string_view text, const Limits& limits) : m_Text(text), m_Limits(limits) {}

    std::expected<Value, Error> run()
    {
        if (m_Text.size() > m_Limits.max_bytes)
        {
            return fail(ErrorCode::TOO_LARGE);
        }
        auto value = parse_value(0);
        if (!value)
        {
            return value;
        }
        skip_whitespace();
        if (m_Pos != m_Text.size())
        {
            return fail(ErrorCode::TRAILING_CONTENT);
        }
        return value;
    }

private:
    [[nodiscard]] std::unexpected<Error> fail(ErrorCode code) const
    {
        return std::unexpected(Error{code, m_Pos});
    }

    [[nodiscard]] bool at_end() const noexcept
    {
        return m_Pos >= m_Text.size();
    }

    [[nodiscard]] char peek() const noexcept
    {
        return m_Text[m_Pos];
    }

    void skip_whitespace() noexcept
    {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
        {
            ++m_Pos;
        }
    }

    std::expected<Value, Error> parse_value(int depth)
    {
        skip_whitespace();
        if (at_end())
        {
            return fail(ErrorCode::UNEXPECTED_END);
        }
        const char c = peek();
        if (c == '{')
        {
            return parse_object(depth + 1);
        }
        if (c == '[')
        {
            return parse_array(depth + 1);
        }
        if (c == '"')
        {
            auto text = parse_string();
            if (!text)
            {
                return std::unexpected(text.error());
            }
            return Value::string(std::move(*text));
        }
        if (c == 't')
        {
            return parse_literal("true", Value::boolean(true));
        }
        if (c == 'f')
        {
            return parse_literal("false", Value::boolean(false));
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            return parse_number();
        }
        return fail(ErrorCode::UNEXPECTED_CHARACTER);
    }

    std::expected<Value, Error> parse_literal(std::string_view word, Value value)
    {
        if (m_Text.substr(m_Pos, word.size()) != word)
        {
            return fail(ErrorCode::UNEXPECTED_CHARACTER);
        }
        m_Pos += word.size();
        return value;
    }

    std::expected<Value, Error> parse_number()
    {
        const bool negative = peek() == '-';
        if (negative)
        {
            ++m_Pos;
        }
        if (at_end() || peek() < '0' || peek() > '9')
        {
            return fail(ErrorCode::BAD_NUMBER);
        }
        std::uint64_t magnitude = 0;
        if (peek() == '0')
        {
            ++m_Pos;
            if (!at_end() && peek() >= '0' && peek() <= '9')
            {
                return fail(ErrorCode::BAD_NUMBER); // leading zero
            }
        }
        else
        {
            while (!at_end() && peek() >= '0' && peek() <= '9')
            {
                const auto digit = static_cast<std::uint64_t>(peek() - '0');
                if (magnitude > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                {
                    return fail(ErrorCode::NUMBER_OUT_OF_RANGE);
                }
                magnitude = magnitude * 10 + digit;
                ++m_Pos;
            }
        }
        if (!at_end() && (peek() == '.' || peek() == 'e' || peek() == 'E'))
        {
            return fail(ErrorCode::NUMBER_NOT_INTEGER);
        }
        constexpr auto MAX_POSITIVE = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (negative)
        {
            if (magnitude > MAX_POSITIVE + 1)
            {
                return fail(ErrorCode::NUMBER_OUT_OF_RANGE);
            }
            return Value::integer(static_cast<std::int64_t>(0 - magnitude));
        }
        if (magnitude > MAX_POSITIVE)
        {
            return fail(ErrorCode::NUMBER_OUT_OF_RANGE);
        }
        return Value::integer(static_cast<std::int64_t>(magnitude));
    }

    std::expected<std::uint32_t, Error> parse_hex4()
    {
        if (m_Pos + 4 > m_Text.size())
        {
            return fail(ErrorCode::UNEXPECTED_END);
        }
        std::uint32_t result = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char c = m_Text[m_Pos + static_cast<std::size_t>(i)];
            std::uint32_t digit = 0;
            if (c >= '0' && c <= '9')
            {
                digit = static_cast<std::uint32_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = static_cast<std::uint32_t>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = static_cast<std::uint32_t>(c - 'A' + 10);
            }
            else
            {
                return fail(ErrorCode::BAD_ESCAPE);
            }
            result = result * 16 + digit;
        }
        m_Pos += 4;
        return result;
    }

    // At the backslash. Appends the character it stands for.
    std::expected<void, Error> parse_escape(std::string& out)
    {
        ++m_Pos;
        if (at_end())
        {
            return fail(ErrorCode::UNEXPECTED_END);
        }
        const char c = peek();
        ++m_Pos;
        switch (c)
        {
        case '"':
            out.push_back('"');
            return {};
        case '\\':
            out.push_back('\\');
            return {};
        case '/':
            out.push_back('/');
            return {};
        case 'b':
            out.push_back('\b');
            return {};
        case 'f':
            out.push_back('\f');
            return {};
        case 'n':
            out.push_back('\n');
            return {};
        case 'r':
            out.push_back('\r');
            return {};
        case 't':
            out.push_back('\t');
            return {};
        case 'u':
            break;
        default:
            --m_Pos;
            return fail(ErrorCode::BAD_ESCAPE);
        }

        auto first = parse_hex4();
        if (!first)
        {
            return std::unexpected(first.error());
        }
        std::uint32_t code_point = *first;
        if (code_point >= 0xDC00 && code_point <= 0xDFFF)
        {
            return fail(ErrorCode::BAD_ESCAPE); // a low surrogate with no high one before it
        }
        if (code_point >= 0xD800 && code_point <= 0xDBFF)
        {
            if (m_Text.substr(m_Pos, 2) != "\\u")
            {
                return fail(ErrorCode::BAD_ESCAPE);
            }
            m_Pos += 2;
            auto second = parse_hex4();
            if (!second)
            {
                return std::unexpected(second.error());
            }
            if (*second < 0xDC00 || *second > 0xDFFF)
            {
                return fail(ErrorCode::BAD_ESCAPE);
            }
            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (*second - 0xDC00);
        }
        append_code_point(out, code_point);
        return {};
    }

    std::expected<std::string, Error> parse_string()
    {
        ++m_Pos; // the opening quote
        std::string out;
        for (;;)
        {
            // A run of plain characters is taken in one piece. The limit is checked as it was one character at a time:
            // the error is at the character that makes the string one byte too long, and the scan never goes past it,
            // so a string of a quarter of a megabyte costs a kilobyte.
            const std::size_t run_start = m_Pos;
            const std::size_t room = m_Limits.max_string_bytes - out.size();
            const std::size_t available = m_Text.size() - run_start;
            const std::size_t scan_end = run_start + (room < available ? room + 1 : available);
            std::size_t run_end = run_start;
            while (run_end < scan_end && is_plain(m_Text[run_end]))
            {
                ++run_end;
            }
            if (run_end > run_start)
            {
                if (run_end - run_start > room)
                {
                    m_Pos = run_start + room + 1;
                    return fail(ErrorCode::STRING_TOO_LONG);
                }
                m_Pos = run_end;
                if (out.empty() && !at_end() && peek() == '"')
                {
                    ++m_Pos; // the common case, a string with nothing to decode: one copy, no appending
                    return std::string(m_Text.substr(run_start, run_end - run_start));
                }
                out.append(m_Text.substr(run_start, run_end - run_start));
            }

            if (at_end())
            {
                return fail(ErrorCode::UNEXPECTED_END);
            }
            const char c = peek();
            if (c == '"')
            {
                ++m_Pos;
                return out;
            }
            if (c == '\\')
            {
                if (auto escaped = parse_escape(out); !escaped)
                {
                    return std::unexpected(escaped.error());
                }
            }
            else if (static_cast<unsigned char>(c) < 0x20)
            {
                return fail(ErrorCode::CONTROL_CHARACTER);
            }
            else
            {
                const std::size_t length = utf8_sequence_length(m_Text, m_Pos);
                if (length == 0)
                {
                    return fail(ErrorCode::BAD_UTF8);
                }
                out.append(m_Text.substr(m_Pos, length));
                m_Pos += length;
            }
            if (out.size() > m_Limits.max_string_bytes)
            {
                return fail(ErrorCode::STRING_TOO_LONG);
            }
        }
    }

    std::expected<Value, Error> parse_array(int depth)
    {
        if (depth > m_Limits.max_depth)
        {
            return fail(ErrorCode::TOO_DEEP);
        }
        ++m_Pos; // [
        skip_whitespace();
        if (!at_end() && peek() == ']')
        {
            ++m_Pos;
            return Value::array();
        }
        Pending<Value::Array> items(m_Items);
        for (;;)
        {
            auto item = parse_value(depth);
            if (!item)
            {
                return item;
            }
            items.add(std::move(*item));
            skip_whitespace();
            if (at_end())
            {
                return fail(ErrorCode::UNEXPECTED_END);
            }
            const char c = peek();
            ++m_Pos;
            if (c == ']')
            {
                break;
            }
            if (c != ',')
            {
                --m_Pos;
                return fail(ErrorCode::UNEXPECTED_CHARACTER);
            }
        }
        return Value::array(items.take());
    }

    std::expected<Value, Error> parse_object(int depth)
    {
        if (depth > m_Limits.max_depth)
        {
            return fail(ErrorCode::TOO_DEEP);
        }
        ++m_Pos; // {
        skip_whitespace();
        if (!at_end() && peek() == '}')
        {
            ++m_Pos;
            return Value::object();
        }
        Pending<Value::Object> members(m_Members);
        for (;;)
        {
            skip_whitespace();
            if (at_end())
            {
                return fail(ErrorCode::UNEXPECTED_END);
            }
            if (peek() != '"')
            {
                return fail(ErrorCode::UNEXPECTED_CHARACTER);
            }
            auto key = parse_string();
            if (!key)
            {
                return std::unexpected(key.error());
            }
            skip_whitespace();
            if (at_end())
            {
                return fail(ErrorCode::UNEXPECTED_END);
            }
            if (peek() != ':')
            {
                return fail(ErrorCode::UNEXPECTED_CHARACTER);
            }
            ++m_Pos;
            auto value = parse_value(depth);
            if (!value)
            {
                return value;
            }
            members.add(std::move(*key), std::move(*value));
            skip_whitespace();
            if (at_end())
            {
                return fail(ErrorCode::UNEXPECTED_END);
            }
            const char c = peek();
            ++m_Pos;
            if (c == '}')
            {
                break;
            }
            if (c != ',')
            {
                --m_Pos;
                return fail(ErrorCode::UNEXPECTED_CHARACTER);
            }
        }
        auto object = Value::object_from(members.take());
        if (!object)
        {
            return fail(ErrorCode::DUPLICATE_KEY);
        }
        return std::move(*object);
    }

    std::string_view m_Text;
    Limits m_Limits;
    std::size_t m_Pos = 0;
    Value::Array m_Items;    // the items of the arrays being read, innermost last
    Value::Object m_Members; // the same for the members of the objects
};

} // namespace

void append_string(std::string& out, std::string_view text)
{
    static constexpr std::string_view HEX = "0123456789abcdef";
    out.push_back('"');
    for (std::size_t i = 0; i < text.size();)
    {
        // Plain characters are copied in one piece.
        const std::size_t run_start = i;
        while (i < text.size() && is_plain(text[i]))
        {
            ++i;
        }
        out.append(text.data() + run_start, i - run_start);
        if (i == text.size())
        {
            break;
        }

        const char c = text[i];
        const auto byte = static_cast<unsigned char>(c);
        if (c == '"')
        {
            out.append("\\\"");
        }
        else if (c == '\\')
        {
            out.append("\\\\");
        }
        else if (c == '\b')
        {
            out.append("\\b");
        }
        else if (c == '\f')
        {
            out.append("\\f");
        }
        else if (c == '\n')
        {
            out.append("\\n");
        }
        else if (c == '\r')
        {
            out.append("\\r");
        }
        else if (c == '\t')
        {
            out.append("\\t");
        }
        else if (byte < 0x20)
        {
            out.append("\\u00");
            out.push_back(HEX[byte >> 4]);
            out.push_back(HEX[byte & 0x0F]);
        }
        else
        {
            const std::size_t length = utf8_sequence_length(text, i);
            if (length == 0)
            {
                out.append(REPLACEMENT_CHARACTER);
                ++i;
                continue;
            }
            out.append(text.substr(i, length));
            i += length;
            continue;
        }
        ++i;
    }
    out.push_back('"');
}

void append_integer(std::string& out, std::int64_t value)
{
    std::array<char, 24> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    out.append(buffer.data(), result.ptr);
}

namespace
{

void write_value(std::string& out, const Value& value)
{
    switch (value.kind())
    {
    case Kind::BOOLEAN:
        out.append(*value.get_boolean() ? "true" : "false");
        break;
    case Kind::INTEGER:
        append_integer(out, *value.get_integer());
        break;
    case Kind::STRING:
        append_string(out, *value.get_string());
        break;
    case Kind::ARRAY:
    {
        out.push_back('[');
        bool first = true;
        for (const Value& item : *value.get_array())
        {
            if (!first)
            {
                out.push_back(',');
            }
            first = false;
            write_value(out, item);
        }
        out.push_back(']');
        break;
    }
    case Kind::OBJECT:
    {
        out.push_back('{');
        bool first = true;
        for (const auto& [key, member] : *value.get_object())
        {
            if (!first)
            {
                out.push_back(',');
            }
            first = false;
            append_string(out, key);
            out.push_back(':');
            write_value(out, member);
        }
        out.push_back('}');
        break;
    }
    }
}

} // namespace

Value Value::boolean(bool value)
{
    Value result;
    result.m_Data = value;
    return result;
}

Value Value::integer(std::int64_t value)
{
    Value result;
    result.m_Data = value;
    return result;
}

Value Value::string(std::string value)
{
    Value result;
    result.m_Data = std::move(value);
    return result;
}

Value Value::array(Array items)
{
    Value result;
    result.m_Data = std::move(items);
    return result;
}

Value Value::object()
{
    Value result;
    result.m_Data = Object{};
    return result;
}

std::optional<Value> Value::object_from(Object members)
{
    // Members that already ascend, which is how the writer puts them, have no repeat and need no sort.
    const bool ascending = std::adjacent_find(members.begin(), members.end(), [](const Member& a, const Member& b)
                                              { return !key_less(a, b); }) == members.end();
    if (!ascending)
    {
        std::stable_sort(members.begin(), members.end(), key_less);
        const auto repeat = std::adjacent_find(members.begin(), members.end(),
                                               [](const Member& a, const Member& b) { return a.first == b.first; });
        if (repeat != members.end())
        {
            return std::nullopt;
        }
    }
    Value result;
    result.m_Data = std::move(members);
    return result;
}

Kind Value::kind() const noexcept
{
    return static_cast<Kind>(m_Data.index());
}

const bool* Value::get_boolean() const noexcept
{
    return std::get_if<bool>(&m_Data);
}

const std::int64_t* Value::get_integer() const noexcept
{
    return std::get_if<std::int64_t>(&m_Data);
}

const std::string* Value::get_string() const noexcept
{
    return std::get_if<std::string>(&m_Data);
}

const Value::Array* Value::get_array() const noexcept
{
    return std::get_if<Array>(&m_Data);
}

Value::Array* Value::get_array() noexcept
{
    return std::get_if<Array>(&m_Data);
}

const Value::Object* Value::get_object() const noexcept
{
    return std::get_if<Object>(&m_Data);
}

const Value* Value::find(std::string_view key) const noexcept
{
    const Object* members = get_object();
    if (members == nullptr)
    {
        return nullptr;
    }
    const auto it = std::lower_bound(members->begin(), members->end(), key,
                                     [](const Member& member, std::string_view k) { return member.first < k; });
    return it != members->end() && it->first == key ? &it->second : nullptr;
}

bool Value::set(std::string key, Value value)
{
    auto* members = std::get_if<Object>(&m_Data);
    if (members == nullptr)
    {
        return false;
    }
    const auto it = std::lower_bound(members->begin(), members->end(), key,
                                     [](const Member& member, const std::string& k) { return member.first < k; });
    if (it != members->end() && it->first == key)
    {
        it->second = std::move(value);
    }
    else
    {
        members->emplace(it, std::move(key), std::move(value));
    }
    return true;
}

bool Value::erase(std::string_view key)
{
    auto* members = std::get_if<Object>(&m_Data);
    if (members == nullptr)
    {
        return false;
    }
    const auto it = std::lower_bound(members->begin(), members->end(), key,
                                     [](const Member& member, std::string_view k) { return member.first < k; });
    if (it == members->end() || it->first != key)
    {
        return false;
    }
    members->erase(it);
    return true;
}

std::expected<Value, Error> parse(std::string_view text, const Limits& limits)
{
    HBM_ZONE("json.parse");
    return Parser(text, limits).run();
}

std::string write(const Value& value)
{
    HBM_ZONE("json.write");
    std::string out;
    out.reserve(1024); // a save is a kilobyte or so; this skips the dozen small growths on the way there
    write_value(out, value);
    return out;
}

} // namespace heart_boom_maths::json
