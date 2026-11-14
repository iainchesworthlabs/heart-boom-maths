// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/rng.hpp"
#include "heart_boom_maths/save_codec.hpp"
#include "save_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace heart_boom_maths;

namespace
{

// A fixed seed and a fixed count keep the test repeatable; a failure prints the seed and the case number.
constexpr std::uint64_t SEED = 0x5AFE5AFE;
constexpr int CASES = 4000;

// Whatever came out of the parser must be something the game could have written itself.
void check_result(const ParseResult& result)
{
    switch (result.status)
    {
    case LoadStatus::OK:
        REQUIRE(validate(result.data).has_value());
        REQUIRE_FALSE(result.damage.has_value());
        // A save that reads back is a save that writes back, to text that reads back as the same data.
        {
            const auto text = serialise(result.data);
            REQUIRE(text.has_value());
            REQUIRE(parse_save(*text).data == result.data);
        }
        break;
    case LoadStatus::DAMAGED:
        REQUIRE(result.damage.has_value());
        REQUIRE(result.data == SaveData{});
        break;
    case LoadStatus::NEWER:
        REQUIRE(result.data == SaveData{});
        break;
    }
}

std::string random_bytes(Rng& rng, std::size_t length)
{
    std::string bytes;
    bytes.reserve(length);
    for (std::size_t i = 0; i < length; ++i)
    {
        bytes.push_back(static_cast<char>(rng.below(256)));
    }
    return bytes;
}

// Bytes drawn from the characters a save is made of, so the garbage gets past the first character more often.
std::string random_jsonish(Rng& rng, std::size_t length)
{
    static constexpr std::string_view PIECES[] = {"{",
                                                  "}",
                                                  "[",
                                                  "]",
                                                  ",",
                                                  ":",
                                                  "\"",
                                                  "\\",
                                                  "\\u",
                                                  "0",
                                                  "1",
                                                  "-",
                                                  "9",
                                                  "e",
                                                  ".",
                                                  "true",
                                                  "false",
                                                  "null",
                                                  "version",
                                                  "coins",
                                                  "top10",
                                                  "name",
                                                  "score",
                                                  "run",
                                                  "sound",
                                                  "music",
                                                  "won_once",
                                                  "game_center",
                                                  "haptics",
                                                  " ",
                                                  "\n",
                                                  "\xC3",
                                                  "\xA9",
                                                  "99999999999999999999",
                                                  "9223372036854775807",
                                                  "-9223372036854775808"};
    std::string text;
    for (std::size_t i = 0; i < length; ++i)
    {
        text.append(PIECES[rng.below(std::size(PIECES))]);
    }
    return text;
}

std::string mutate(Rng& rng, std::string text)
{
    const auto edits = 1 + rng.below(4);
    for (std::uint64_t e = 0; e < edits; ++e)
    {
        if (text.empty())
        {
            text.push_back(static_cast<char>(rng.below(256)));
            continue;
        }
        const auto at = static_cast<std::size_t>(rng.below(text.size()));
        switch (rng.below(6))
        {
        case 0: // flip one bit
            text[at] = static_cast<char>(static_cast<unsigned char>(text[at]) ^ (1U << rng.below(8)));
            break;
        case 1: // replace a byte
            text[at] = static_cast<char>(rng.below(256));
            break;
        case 2: // delete a byte
            text.erase(at, 1);
            break;
        case 3: // insert a byte
            text.insert(at, 1, static_cast<char>(rng.below(256)));
            break;
        case 4: // truncate
            text.resize(at);
            break;
        default: // duplicate a slice
        {
            const auto length = static_cast<std::size_t>(rng.below(16));
            text.insert(at, text.substr(at, length));
            break;
        }
        }
    }
    return text;
}

// The text of a save with plenty in it, for the mutations to start from.
std::string rich_save()
{
    SaveData data;
    data.coins = 1234;
    data.won_once = true;
    data.game_center = true;
    for (int i = 0; i < 10; ++i)
    {
        data.top10.push_back({"PLAYER " + std::string(1, static_cast<char>('A' + i)), 10000 - i * 7});
    }
    data.run = std::vector<std::int64_t>{3, 1, 400, -9, 9223372036854775807, 0, 12, 12, 12};
    return serialise(data).value();
}

} // namespace

TEST_CASE("random garbage never crashes the reader", "[property][fuzz][save]")
{
    Rng rng(SEED);
    for (int i = 0; i < CASES; ++i)
    {
        CAPTURE(SEED, i);
        const std::string text = random_bytes(rng, static_cast<std::size_t>(rng.below(300)));
        check_result(parse_save(text));
        (void)json::parse(text);
    }
}

TEST_CASE("random JSON-shaped garbage never crashes the reader", "[property][fuzz][save]")
{
    Rng rng(SEED + 1);
    for (int i = 0; i < CASES; ++i)
    {
        CAPTURE(SEED, i);
        const std::string text = random_jsonish(rng, static_cast<std::size_t>(rng.below(60)));
        check_result(parse_save(text));
    }
}

TEST_CASE("mutated saves never crash the reader or yield invalid data", "[property][fuzz][save]")
{
    Rng rng(SEED + 2);
    const std::string seeds[] = {rich_save(), std::string(SAVE_V1_FIXTURE), serialise(SaveData{}).value()};
    int still_ok = 0;
    for (int i = 0; i < CASES; ++i)
    {
        CAPTURE(SEED, i);
        const std::string& base = seeds[rng.below(std::size(seeds))];
        const std::string text = mutate(rng, base);
        const ParseResult result = parse_save(text);
        check_result(result);
        still_ok += result.status == LoadStatus::OK ? 1 : 0;
    }
    // Some mutations hit whitespace or a digit and leave a valid save, so the loop must see both outcomes.
    REQUIRE(still_ok > 0);
    REQUIRE(still_ok < CASES);
}

TEST_CASE("every prefix of a save is handled", "[property][fuzz][save]")
{
    const std::string text = rich_save();
    for (std::size_t length = 0; length < text.size(); ++length)
    {
        CAPTURE(length);
        const ParseResult result = parse_save(std::string_view(text).substr(0, length));
        REQUIRE(result.status == LoadStatus::DAMAGED);
        check_result(result);
    }
    REQUIRE(parse_save(text).status == LoadStatus::OK);
}

TEST_CASE("hostile shapes are refused quickly", "[unit][fuzz][save]")
{
    const auto damaged = [](const std::string& text)
    {
        const ParseResult result = parse_save(text);
        REQUIRE(result.status == LoadStatus::DAMAGED);
        check_result(result);
    };
    damaged(std::string(200000, '['));
    damaged(std::string(200000, '{'));
    damaged(std::string(60000, '[') + std::string(60000, ']'));
    {
        std::string deep;
        for (int i = 0; i < 20000; ++i)
        {
            deep += "{\"top10\":";
        }
        damaged(deep);
    }
    damaged("{\"version\":1,\"run\":[" + std::string(300000, '1') + "]}");
    damaged("{\"version\":1,\"coins\":0,\"name\":\"" + std::string(100000, 'A') + "\"}");
    {
        std::string wide = "{\"version\":1,\"top10\":[";
        for (int i = 0; i < 20000; ++i)
        {
            wide += i == 0 ? "{}" : ",{}";
        }
        damaged(wide + "]}");
    }
    {
        // Many small members: well inside the size limit, and must not take quadratic time or crash.
        std::string many = "{";
        for (int i = 0; i < 20000; ++i)
        {
            many += (i == 0 ? "\"k" : ",\"k") + std::to_string(i) + "\":1";
        }
        damaged(many + "}"); // valid JSON, but no version
    }
}
