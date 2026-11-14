// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/replay.hpp"

#include "heart_boom_maths/json.hpp"
#include "heart_boom_maths/state_hash.hpp"

#include <array>
#include <limits>
#include <utility>

namespace heart_boom_maths
{

namespace
{

constexpr std::array<std::pair<ReplayAction, std::string_view>, 9> ACTION_NAMES{{
    {ReplayAction::START_GAME, "start_game"},
    {ReplayAction::ANSWER, "answer"},
    {ReplayAction::USE_HINT, "use_hint"},
    {ReplayAction::PAUSE, "pause"},
    {ReplayAction::RESUME, "resume"},
    {ReplayAction::QUIT, "quit"},
    {ReplayAction::NEXT_LEVEL, "next_level"},
    {ReplayAction::KEEP_GOING, "keep_going"},
    {ReplayAction::FINISH, "finish"},
}};

// A replay of a long endless run is a few tens of kilobytes. The limit only has to stop hostile input.
constexpr std::size_t MAX_REPLAY_BYTES = std::size_t{4} * 1024 * 1024;
constexpr std::int64_t MAX_TICK = 1'000'000'000;

std::unexpected<std::string> fail(std::string message)
{
    return std::unexpected(std::move(message));
}

std::optional<std::uint64_t> parse_hex(std::string_view text)
{
    if (text.size() != 16)
    {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text)
    {
        int digit = 0;
        if (c >= '0' && c <= '9')
        {
            digit = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            digit = c - 'a' + 10;
        }
        else
        {
            return std::nullopt;
        }
        value = (value << 4) | static_cast<std::uint64_t>(digit);
    }
    return value;
}

std::string describe_json_error(const json::Error& error)
{
    return "not valid replay JSON (error " + std::to_string(static_cast<int>(error.code)) + " at byte " +
           std::to_string(error.offset) + ")";
}

} // namespace

std::string_view replay_action_name(ReplayAction action) noexcept
{
    for (const auto& [value, name] : ACTION_NAMES)
    {
        if (value == action)
        {
            return name;
        }
    }
    return "unknown";
}

std::optional<ReplayAction> replay_action_from_name(std::string_view name) noexcept
{
    for (const auto& [value, text] : ACTION_NAMES)
    {
        if (text == name)
        {
            return value;
        }
    }
    return std::nullopt;
}

const Tuning* replay_tuning(std::string_view name) noexcept
{
    if (name == "defaults")
    {
        return &Tuning::defaults();
    }
    return nullptr;
}

std::string hash_to_hex(std::uint64_t hash)
{
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string text(16, '0');
    for (std::size_t i = 0; i < 16; ++i)
    {
        text[15 - i] = DIGITS[(hash >> (4 * i)) & 0xFU];
    }
    return text;
}

std::expected<Replay, std::string> parse_replay(std::string_view text)
{
    json::Limits limits;
    limits.max_bytes = MAX_REPLAY_BYTES;
    const auto parsed = json::parse(text, limits);
    if (!parsed)
    {
        return fail(describe_json_error(parsed.error()));
    }
    const json::Value& root = *parsed;
    if (root.kind() != json::Kind::OBJECT)
    {
        return fail("the replay must be a JSON object");
    }

    const auto integer_of = [&](const json::Value& object, std::string_view key) -> const std::int64_t*
    {
        const json::Value* member = object.find(key);
        return member != nullptr ? member->get_integer() : nullptr;
    };
    const auto string_of = [&](const json::Value& object, std::string_view key) -> const std::string*
    {
        const json::Value* member = object.find(key);
        return member != nullptr ? member->get_string() : nullptr;
    };

    Replay replay;

    const std::int64_t* format = integer_of(root, "format");
    if (format == nullptr || *format != REPLAY_FORMAT)
    {
        return fail("format: must be the number " + std::to_string(REPLAY_FORMAT));
    }

    const std::int64_t* seed = integer_of(root, "seed");
    if (seed == nullptr || *seed < 0)
    {
        return fail("seed: must be a whole number from 0 to 9223372036854775807");
    }
    replay.seed = static_cast<std::uint64_t>(*seed);

    const std::string* tuning = string_of(root, "tuning");
    if (tuning == nullptr || replay_tuning(*tuning) == nullptr)
    {
        return fail("tuning: must name a tuning this build has (\"defaults\")");
    }
    replay.tuning = *tuning;

    if (const json::Value* member = root.find("wallet"); member != nullptr)
    {
        const std::int64_t* wallet = member->get_integer();
        if (wallet == nullptr || *wallet < 0)
        {
            return fail("wallet: must be a whole number of coins, 0 or more");
        }
        replay.wallet = *wallet;
    }
    if (const json::Value* member = root.find("won_once"); member != nullptr)
    {
        const bool* won_once = member->get_boolean();
        if (won_once == nullptr)
        {
            return fail("won_once: must be true or false");
        }
        replay.won_once = *won_once;
    }
    for (const auto& [key, target] : {std::pair<std::string_view, std::string*>{"note", &replay.note},
                                      std::pair<std::string_view, std::string*>{"recipe", &replay.recipe}})
    {
        if (const json::Value* member = root.find(key); member != nullptr)
        {
            const std::string* value = member->get_string();
            if (value == nullptr)
            {
                return fail(std::string(key) + ": must be text");
            }
            *target = *value;
        }
    }

    const json::Value* commands = root.find("commands");
    const json::Value::Array* list = commands != nullptr ? commands->get_array() : nullptr;
    if (list == nullptr)
    {
        return fail("commands: must be a list");
    }
    std::int64_t last_tick = 0;
    replay.commands.reserve(list->size());
    for (std::size_t i = 0; i < list->size(); ++i)
    {
        const json::Value& item = (*list)[i];
        const std::string where = "commands[" + std::to_string(i) + "]";
        if (item.kind() != json::Kind::OBJECT)
        {
            return fail(where + ": must be an object");
        }
        const std::int64_t* tick = integer_of(item, "tick");
        if (tick == nullptr || *tick < 0 || *tick > MAX_TICK)
        {
            return fail(where + ".tick: must be a whole number from 0 to 1000000000");
        }
        if (*tick < last_tick)
        {
            return fail(where + ".tick: ticks must not go backwards");
        }
        last_tick = *tick;

        const std::string* name = string_of(item, "do");
        const std::optional<ReplayAction> action = name != nullptr ? replay_action_from_name(*name) : std::nullopt;
        if (!action)
        {
            return fail(where + ".do: must be a command name such as \"start_game\" or \"answer\"");
        }

        ReplayCommand command;
        command.tick = static_cast<int>(*tick);
        command.action = *action;
        if (*action == ReplayAction::ANSWER)
        {
            const std::int64_t* tile = integer_of(item, "tile");
            if (tile == nullptr || *tile < std::numeric_limits<int>::min() || *tile > std::numeric_limits<int>::max())
            {
                return fail(where + ".tile: an answer needs a whole number");
            }
            command.tile = static_cast<int>(*tile);
        }
        replay.commands.push_back(command);
    }

    if (const json::Value* member = root.find("end_tick"); member != nullptr)
    {
        const std::int64_t* end_tick = member->get_integer();
        if (end_tick == nullptr || *end_tick < last_tick || *end_tick > MAX_TICK)
        {
            return fail("end_tick: must be a whole number from the last command's tick to 1000000000");
        }
        replay.end_tick = static_cast<int>(*end_tick);
    }

    if (const json::Value* expect = root.find("expect"); expect != nullptr)
    {
        if (expect->kind() != json::Kind::OBJECT)
        {
            return fail("expect: must be an object");
        }
        const std::string* events_hash = string_of(*expect, "events_hash");
        const std::string* final_hash = string_of(*expect, "final_hash");
        const std::int64_t* score = integer_of(*expect, "score");
        const auto events = events_hash != nullptr ? parse_hex(*events_hash) : std::nullopt;
        const auto final_state = final_hash != nullptr ? parse_hex(*final_hash) : std::nullopt;
        if (!events)
        {
            return fail("expect.events_hash: must be 16 lowercase hex digits");
        }
        if (!final_state)
        {
            return fail("expect.final_hash: must be 16 lowercase hex digits");
        }
        if (score == nullptr)
        {
            return fail("expect.score: must be a whole number");
        }
        replay.expect = ReplayExpect{*events, *final_state, *score};
    }
    return replay;
}

std::string write_replay(const Replay& replay)
{
    const auto quoted = [](const std::string& text) { return json::write(json::Value::string(text)); };

    std::string out = "{\n";
    out += "  \"format\": " + std::to_string(replay.format) + ",\n";
    out += "  \"seed\": " + std::to_string(replay.seed) + ",\n";
    out += "  \"tuning\": " + quoted(replay.tuning) + ",\n";
    if (replay.wallet != 0)
    {
        out += "  \"wallet\": " + std::to_string(replay.wallet) + ",\n";
    }
    if (replay.won_once)
    {
        out += "  \"won_once\": true,\n";
    }
    if (!replay.note.empty())
    {
        out += "  \"note\": " + quoted(replay.note) + ",\n";
    }
    if (!replay.recipe.empty())
    {
        out += "  \"recipe\": " + quoted(replay.recipe) + ",\n";
    }
    out += "  \"commands\": [";
    for (std::size_t i = 0; i < replay.commands.size(); ++i)
    {
        const ReplayCommand& c = replay.commands[i];
        out += i == 0 ? "\n    " : ",\n    ";
        out +=
            "{ \"tick\": " + std::to_string(c.tick) + ", \"do\": \"" + std::string(replay_action_name(c.action)) + "\"";
        if (c.action == ReplayAction::ANSWER)
        {
            out += ", \"tile\": " + std::to_string(c.tile);
        }
        out += " }";
    }
    out += replay.commands.empty() ? "]" : "\n  ]";
    if (replay.end_tick != 0)
    {
        out += ",\n  \"end_tick\": " + std::to_string(replay.end_tick);
    }
    if (replay.expect)
    {
        out += ",\n  \"expect\": { \"events_hash\": \"" + hash_to_hex(replay.expect->events_hash) +
               "\", \"final_hash\": \"" + hash_to_hex(replay.expect->final_hash) +
               "\", \"score\": " + std::to_string(replay.expect->score) + " }";
    }
    out += "\n}\n";
    return out;
}

ReplayRunner::ReplayRunner(const Tuning& tuning, std::uint64_t seed, std::int64_t wallet, bool won_once)
    : m_Session(tuning, seed)
{
    m_Session.set_wallet(wallet);
    m_Session.set_won_once(won_once);
}

std::vector<Event> ReplayRunner::collect()
{
    std::vector<Event> events = m_Session.take_events();
    for (const Event& event : events)
    {
        event.append_to(m_Hashed);
    }
    m_EventCount += events.size();
    return events;
}

std::vector<Event> ReplayRunner::tick()
{
    m_Session.tick();
    ++m_Tick;
    return collect();
}

std::vector<Event> ReplayRunner::apply(const ReplayCommand& command)
{
    switch (command.action)
    {
    case ReplayAction::START_GAME:
        m_Session.start_game();
        break;
    case ReplayAction::ANSWER:
        m_Session.answer(command.tile);
        break;
    case ReplayAction::USE_HINT:
        m_Session.use_hint();
        break;
    case ReplayAction::PAUSE:
        m_Session.pause();
        break;
    case ReplayAction::RESUME:
        m_Session.resume();
        break;
    case ReplayAction::QUIT:
        m_Session.quit();
        break;
    case ReplayAction::NEXT_LEVEL:
        m_Session.next_level();
        break;
    case ReplayAction::KEEP_GOING:
        m_Session.keep_going();
        break;
    case ReplayAction::FINISH:
        m_Session.finish();
        break;
    }
    return collect();
}

ReplayResult ReplayRunner::result() const
{
    ReplayResult result;
    result.events_hash = fnv1a_64(m_Hashed);
    result.final_hash = m_Session.state_hash();
    result.score = m_Session.view().points;
    result.level = m_Session.view().level;
    result.state = m_Session.view().state;
    result.event_count = m_EventCount;
    return result;
}

std::expected<ReplayResult, std::string> run_replay(const Replay& replay, const ReplayObserver& observer)
{
    const Tuning* tuning = replay_tuning(replay.tuning);
    if (tuning == nullptr)
    {
        return fail("tuning: \"" + replay.tuning + "\" is not a tuning this build has");
    }
    ReplayRunner runner(*tuning, replay.seed, replay.wallet, replay.won_once);
    for (const ReplayCommand& command : replay.commands)
    {
        while (runner.tick_count() < command.tick)
        {
            const std::vector<Event> events = runner.tick();
            if (observer)
            {
                observer(nullptr, events, runner);
            }
        }
        const std::vector<Event> events = runner.apply(command);
        if (observer)
        {
            observer(&command, events, runner);
        }
    }
    while (runner.tick_count() < replay.end_tick)
    {
        const std::vector<Event> events = runner.tick();
        if (observer)
        {
            observer(nullptr, events, runner);
        }
    }
    return runner.result();
}

} // namespace heart_boom_maths
