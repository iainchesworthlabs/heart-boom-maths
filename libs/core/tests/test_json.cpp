// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/rng.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <iterator>
#include <limits>
#include <string>

using namespace heart_boom_maths;
using heart_boom_maths::json::ErrorCode;
using heart_boom_maths::json::Kind;
using heart_boom_maths::json::Value;

namespace
{

ErrorCode error_of(std::string_view text, const json::Limits& limits = {})
{
    const auto result = json::parse(text, limits);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

std::string repeat(std::string_view piece, std::size_t count)
{
    std::string out;
    for (std::size_t i = 0; i < count; ++i)
    {
        out.append(piece);
    }
    return out;
}

} // namespace

TEST_CASE("json parses each supported kind", "[unit][json]")
{
    const auto parsed = json::parse(R"( { "a": [1, -2, true, false, "x"], "b": {} , "c": 0 } )");
    REQUIRE(parsed.has_value());
    const Value& root = *parsed;
    REQUIRE(root.kind() == Kind::OBJECT);
    const Value* a = root.find("a");
    REQUIRE(a != nullptr);
    REQUIRE(a->get_array()->size() == 5);
    REQUIRE(*(*a->get_array())[0].get_integer() == 1);
    REQUIRE(*(*a->get_array())[1].get_integer() == -2);
    REQUIRE(*(*a->get_array())[2].get_boolean());
    REQUIRE_FALSE(*(*a->get_array())[3].get_boolean());
    REQUIRE(*(*a->get_array())[4].get_string() == "x");
    REQUIRE(root.find("b")->get_object()->empty());
    REQUIRE(*root.find("c")->get_integer() == 0);
    REQUIRE(root.find("missing") == nullptr);
}

TEST_CASE("json carries the full 64-bit integer range", "[unit][json]")
{
    constexpr auto MAX = std::numeric_limits<std::int64_t>::max();
    constexpr auto MIN = std::numeric_limits<std::int64_t>::min();
    const auto parsed = json::parse("[9223372036854775807,-9223372036854775808,-0]");
    REQUIRE(parsed.has_value());
    REQUIRE(*(*parsed->get_array())[0].get_integer() == MAX);
    REQUIRE(*(*parsed->get_array())[1].get_integer() == MIN);
    REQUIRE(*(*parsed->get_array())[2].get_integer() == 0);
    REQUIRE(json::write(*parsed) == "[9223372036854775807,-9223372036854775808,0]");

    REQUIRE(error_of("9223372036854775808") == ErrorCode::NUMBER_OUT_OF_RANGE);
    REQUIRE(error_of("-9223372036854775809") == ErrorCode::NUMBER_OUT_OF_RANGE);
    REQUIRE(error_of("18446744073709551616") == ErrorCode::NUMBER_OUT_OF_RANGE);
    REQUIRE(error_of(repeat("9", 400)) == ErrorCode::NUMBER_OUT_OF_RANGE);
}

TEST_CASE("json refuses every number that is not a plain integer", "[unit][json]")
{
    REQUIRE(error_of("1.5") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("1.0") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("1e3") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("1E3") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("0.5") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("-0e0") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("[2.5]") == ErrorCode::NUMBER_NOT_INTEGER);
    REQUIRE(error_of("01") == ErrorCode::BAD_NUMBER);
    REQUIRE(error_of("-") == ErrorCode::BAD_NUMBER);
    REQUIRE(error_of("-x") == ErrorCode::BAD_NUMBER);
    REQUIRE(error_of("+1") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of(".5") == ErrorCode::UNEXPECTED_CHARACTER);
}

TEST_CASE("json refuses malformed documents", "[unit][json]")
{
    REQUIRE(error_of("") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("   ") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("null") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("tru") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("[1,]") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("[1 2]") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("[1") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("{\"a\":1,}") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("{\"a\" 1}") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("{a:1}") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of("{\"a\":1") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("{\"a\"") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("{\"a\":1,\"a\":2}") == ErrorCode::DUPLICATE_KEY);
    REQUIRE(error_of("1 2") == ErrorCode::TRAILING_CONTENT);
    REQUIRE(error_of("{} x") == ErrorCode::TRAILING_CONTENT);
    REQUIRE(error_of("\"abc") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("// comment\n1") == ErrorCode::UNEXPECTED_CHARACTER);
    REQUIRE(error_of(std::string("\xEF\xBB\xBF{}")) == ErrorCode::UNEXPECTED_CHARACTER); // a byte order mark
}

TEST_CASE("json strings decode the standard escapes", "[unit][json]")
{
    const auto parsed = json::parse(R"("a\"b\\c\/d\b\f\n\r\t A é € 😀")");
    REQUIRE(parsed.has_value());
    REQUIRE(*parsed->get_string() == "a\"b\\c/d\b\f\n\r\t A \xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80");
}

TEST_CASE("json refuses bad escapes and bad text", "[unit][json]")
{
    REQUIRE(error_of(R"("\x41")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\u12zz")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\u12G4")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\ud83d")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\ud83dx")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\ud83dA")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of(R"("\ude00")") == ErrorCode::BAD_ESCAPE);
    REQUIRE(error_of("\"\\") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of("\"\\u12") == ErrorCode::UNEXPECTED_END);
    REQUIRE(error_of(std::string("\"a\nb\"")) == ErrorCode::CONTROL_CHARACTER);
    REQUIRE(error_of(std::string("\"a\tb\"")) == ErrorCode::CONTROL_CHARACTER);
    REQUIRE(error_of(std::string("\"a\0b\"", 5)) == ErrorCode::CONTROL_CHARACTER);
}

TEST_CASE("json refuses invalid UTF-8", "[unit][json]")
{
    const auto bad = [](std::string_view bytes)
    { return error_of("\"" + std::string(bytes) + "\"") == ErrorCode::BAD_UTF8; };
    REQUIRE(bad("\x80"));         // a continuation byte alone
    REQUIRE(bad("\xC0\x80"));     // overlong NUL
    REQUIRE(bad("\xC1\xBF"));     // overlong
    REQUIRE(bad("\xE0\x80\x80")); // overlong
    REQUIRE(bad("\xED\xA0\x80")); // a surrogate
    REQUIRE(bad("\xF0\x80\x80\x80"));
    REQUIRE(bad("\xF4\x90\x80\x80")); // above U+10FFFF
    REQUIRE(bad("\xF5\x80\x80\x80"));
    REQUIRE(bad("\xFF"));
    REQUIRE(bad("\xC3"));     // cut short by the closing quote
    REQUIRE(bad("\xE2\x82")); // cut short
}

TEST_CASE("json enforces its limits", "[unit][json]")
{
    SECTION("nesting")
    {
        json::Limits limits;
        limits.max_depth = 4;
        REQUIRE(json::parse("[[[[]]]]", limits).has_value());
        REQUIRE(error_of("[[[[[]]]]]", limits) == ErrorCode::TOO_DEEP);
        REQUIRE(error_of("{\"a\":{\"a\":{\"a\":{\"a\":{}}}}}", limits) == ErrorCode::TOO_DEEP);
    }
    SECTION("a nesting bomb is refused without recursing to the bottom")
    {
        REQUIRE(error_of(repeat("[", 100000)) == ErrorCode::TOO_DEEP);
        REQUIRE(error_of(repeat("{\"a\":", 40000)) == ErrorCode::TOO_DEEP);
        REQUIRE(error_of(repeat("[", 300000)) == ErrorCode::TOO_LARGE);
    }
    SECTION("total size")
    {
        json::Limits limits;
        limits.max_bytes = 10;
        REQUIRE(json::parse("[1,2,3,4,5]", limits).has_value() == false);
        REQUIRE(error_of("[1,2,3,4,5]", limits) == ErrorCode::TOO_LARGE);
        REQUIRE(json::parse("[1,2,3,4]", limits).has_value());
    }
    SECTION("string length")
    {
        json::Limits limits;
        limits.max_string_bytes = 5;
        REQUIRE(json::parse("\"12345\"", limits).has_value());
        REQUIRE(error_of("\"123456\"", limits) == ErrorCode::STRING_TOO_LONG);
        REQUIRE(error_of("\"\\u0041\\u0041\\u0041\\u0041\\u0041\\u0041\"", limits) == ErrorCode::STRING_TOO_LONG);
        REQUIRE(error_of("{\"123456\":1}", limits) == ErrorCode::STRING_TOO_LONG);
    }
}

TEST_CASE("json writes compact text with members in key order", "[unit][json]")
{
    Value root = Value::object();
    root.set("zebra", Value::integer(1));
    root.set("apple", Value::boolean(true));
    root.set("mango", Value::array({Value::string("x"), Value::integer(-7)}));
    REQUIRE(json::write(root) == R"({"apple":true,"mango":["x",-7],"zebra":1})");

    // Replacing a member keeps the order, and erasing removes it.
    root.set("apple", Value::boolean(false));
    REQUIRE(json::write(root) == R"({"apple":false,"mango":["x",-7],"zebra":1})");
    REQUIRE(root.erase("mango"));
    REQUIRE_FALSE(root.erase("mango"));
    REQUIRE(json::write(root) == R"({"apple":false,"zebra":1})");
    REQUIRE(json::write(Value::object()) == "{}");
    REQUIRE(json::write(Value::array()) == "[]");
}

TEST_CASE("json output does not depend on the order of the input", "[unit][json]")
{
    const auto one = json::parse(R"({"b":1,"a":2,"c":[{"y":1,"x":2}]})");
    const auto two = json::parse(R"({ "c" : [ {"x":2,"y":1} ], "a":2, "b":1 })");
    REQUIRE(one.has_value());
    REQUIRE(two.has_value());
    REQUIRE(*one == *two);
    REQUIRE(json::write(*one) == json::write(*two));
}

TEST_CASE("json writer escapes control characters and survives bad bytes", "[unit][json]")
{
    const Value text = Value::string(std::string("a\"\\\n\r\t\b\f\x01\x1f z \xC3\xA9"));
    const std::string written = json::write(text);
    REQUIRE(written == "\"a\\\"\\\\\\n\\r\\t\\b\\f\\u0001\\u001f z \xC3\xA9\"");
    const auto back = json::parse(written);
    REQUIRE(back.has_value());
    REQUIRE(*back == text);

    // A string that is not UTF-8 is written with U+FFFD in place of each bad byte, and still parses.
    const std::string broken = json::write(Value::string("a\xFF\xC3z"));
    REQUIRE(broken == "\"a\xEF\xBF\xBD\xEF\xBF\xBDz\"");
    REQUIRE(json::parse(broken).has_value());
}

TEST_CASE("json round-trips random documents", "[property][json]")
{
    Rng rng(0xC0FFEE);
    const auto make = [&rng](auto&& self, int depth) -> Value
    {
        const auto kind = rng.below(depth >= 4 ? 3 : 5);
        switch (kind)
        {
        case 0:
            return Value::boolean(rng.below(2) == 1);
        case 1:
            return Value::integer(static_cast<std::int64_t>(rng.next_u64()));
        case 2:
        {
            std::string text;
            for (auto n = rng.below(8); n > 0; --n)
            {
                static constexpr std::string_view PIECES[] = {
                    "a", "Z", " ", "\"", "\\", "\n", "\x01", "\xC3\xA9", "\xF0\x9F\x98\x80"};
                text.append(PIECES[rng.below(std::size(PIECES))]);
            }
            return Value::string(text);
        }
        case 3:
        {
            Value::Array items;
            for (auto n = rng.below(4); n > 0; --n)
            {
                items.push_back(self(self, depth + 1));
            }
            return Value::array(std::move(items));
        }
        default:
        {
            Value object = Value::object();
            for (auto n = rng.below(4); n > 0; --n)
            {
                object.set("k" + std::to_string(rng.below(6)), self(self, depth + 1));
            }
            return object;
        }
        }
    };
    for (int i = 0; i < 500; ++i)
    {
        const Value original = make(make, 0);
        const std::string text = json::write(original);
        const auto back = json::parse(text);
        REQUIRE(back.has_value());
        REQUIRE(*back == original);
        REQUIRE(json::write(*back) == text);
    }
}
