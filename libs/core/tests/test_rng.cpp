// SPDX-License-Identifier: GPL-3.0-or-later
#include "heart_boom_maths/rng.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <vector>

using namespace heart_boom_maths;

namespace
{

// The expected numbers below were produced by a separate Python implementation of SplitMix64 and its rejection
// sampling, so they check the algorithm itself and not only that the code agrees with itself. A change to any of them
// changes every replay and every saved run, so it must be deliberate.

template <std::size_t N>
std::array<std::uint64_t, N> first_outputs(std::uint64_t seed)
{
    Rng rng(seed);
    std::array<std::uint64_t, N> out{};
    for (auto& value : out)
    {
        value = rng.next_u64();
    }
    return out;
}

} // namespace

TEST_CASE("Rng SplitMix64 sequence is fixed for known seeds", "[unit][rng]")
{
    // Seed 0 is the reference vector published with the SplitMix64 algorithm.
    REQUIRE(first_outputs<5>(0) == std::array<std::uint64_t, 5>{0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL,
                                                                0x06C45D188009454FULL, 0xF88BB8A8724C81ECULL,
                                                                0x1B39896A51A8749BULL});
    REQUIRE(first_outputs<5>(1) == std::array<std::uint64_t, 5>{0x910A2DEC89025CC1ULL, 0xBEEB8DA1658EEC67ULL,
                                                                0xF893A2EEFB32555EULL, 0x71C18690EE42C90BULL,
                                                                0x71BB54D8D101B5B9ULL});
    REQUIRE(first_outputs<5>(42) == std::array<std::uint64_t, 5>{0xBDD732262FEB6E95ULL, 0x28EFE333B266F103ULL,
                                                                 0x47526757130F9F52ULL, 0x581CE1FF0E4AE394ULL,
                                                                 0x09BC585A244823F2ULL});
    REQUIRE(first_outputs<5>(0xFFFFFFFFFFFFFFFFULL) ==
            std::array<std::uint64_t, 5>{0xE4D971771B652C20ULL, 0xE99FF867DBF682C9ULL, 0x382FF84CB27281E9ULL,
                                         0x6D1DB36CCBA982D2ULL, 0xB4A0472E578069AEULL});
}

TEST_CASE("Rng state is the seed plus one step per output", "[unit][rng]")
{
    Rng rng(10);
    REQUIRE(rng.state() == 10);
    (void)rng.next_u64();
    REQUIRE(rng.state() == 10 + 0x9E3779B97F4A7C15ULL);

    // Restoring the state replays the same outputs, which is what a saved run relies on.
    Rng copy(0);
    copy.set_state(rng.state());
    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(copy.next_u64() == rng.next_u64());
    }
}

TEST_CASE("Rng below and between give fixed values for fixed seeds", "[unit][rng]")
{
    {
        Rng rng(1);
        std::array<std::uint64_t, 10> got{};
        for (auto& value : got)
        {
            value = rng.below(6);
        }
        REQUIRE(got == std::array<std::uint64_t, 10>{5, 1, 0, 5, 3, 2, 3, 3, 0, 4});
    }
    {
        Rng rng(42);
        std::array<std::uint64_t, 10> got{};
        for (auto& value : got)
        {
            value = rng.below(6);
        }
        REQUIRE(got == std::array<std::uint64_t, 10>{1, 1, 0, 0, 4, 0, 1, 2, 1, 2});
    }
    {
        Rng rng(1);
        std::array<std::uint64_t, 5> got{};
        for (auto& value : got)
        {
            value = rng.below(1000000007);
        }
        REQUIRE(got == std::array<std::uint64_t, 5>{42308323, 765712721, 900016442, 442915363, 760308508});
    }
    {
        // A bound above 2^63 makes the rejection step matter.
        Rng rng(1);
        std::array<std::uint64_t, 4> got{};
        for (auto& value : got)
        {
            value = rng.below(0xC000000000000000ULL);
        }
        REQUIRE(got == std::array<std::uint64_t, 4>{10451216379200822465ULL, 13757245211066428519ULL,
                                                    4076781235000726878ULL, 8196980753821780235ULL});
    }
    {
        Rng rng(1);
        std::array<int, 10> got{};
        for (auto& value : got)
        {
            value = rng.between(-10, 10);
        }
        REQUIRE(got == std::array<int, 10>{-8, -3, 5, 4, 2, -8, -10, -7, 5, -6});
    }
    {
        Rng rng(42);
        std::array<int, 10> got{};
        for (auto& value : got)
        {
            value = rng.between(-10, 10);
        }
        REQUIRE(got == std::array<int, 10>{9, 9, -10, -1, 3, 8, 6, 10, 3, -5});
    }
}

TEST_CASE("Rng below is roughly uniform", "[unit][rng]")
{
    // The seed is fixed, so these checks are exact repeats and cannot flake. The tolerance is wide enough for any
    // reasonable seed and still fails for a modulo-biased or broken bound.
    constexpr int DRAWS = 60000;
    for (const std::uint64_t bound : {1ULL, 2ULL, 3ULL, 10ULL})
    {
        Rng rng(2026);
        std::vector<int> counts(bound, 0);
        for (int i = 0; i < DRAWS; ++i)
        {
            const std::uint64_t v = rng.below(bound);
            REQUIRE(v < bound);
            ++counts[v];
        }
        const int expected = DRAWS / static_cast<int>(bound);
        for (const int count : counts)
        {
            REQUIRE(std::abs(count - expected) < expected / 10 + 1);
        }
    }
}

TEST_CASE("Rng below is uniform over a large bound that is not a power of two", "[unit][rng]")
{
    constexpr int DRAWS = 60000;
    constexpr int BUCKETS = 10;

    // 1,000,003 split into ten equal deciles.
    {
        constexpr std::uint64_t BOUND = 1000003;
        Rng rng(7);
        std::array<int, BUCKETS> counts{};
        for (int i = 0; i < DRAWS; ++i)
        {
            const std::uint64_t v = rng.below(BOUND);
            REQUIRE(v < BOUND);
            ++counts[v * BUCKETS / BOUND];
        }
        for (const int count : counts)
        {
            REQUIRE(std::abs(count - DRAWS / BUCKETS) < DRAWS / BUCKETS / 10);
        }
    }

    // Three quarters of the 64-bit range. A plain `r % bound` would put half of the draws in the first third.
    {
        constexpr std::uint64_t BOUND = 0xC000000000000000ULL;
        constexpr std::uint64_t THIRD = BOUND / 3;
        Rng rng(7);
        std::array<int, 3> counts{};
        for (int i = 0; i < DRAWS; ++i)
        {
            const std::uint64_t v = rng.below(BOUND);
            REQUIRE(v < BOUND);
            ++counts[std::min<std::uint64_t>(v / THIRD, 2)];
        }
        for (const int count : counts)
        {
            REQUIRE(std::abs(count - DRAWS / 3) < DRAWS / 3 / 10);
        }
    }
}

TEST_CASE("Rng below of the largest bound stays in range", "[unit][rng]")
{
    Rng rng(3);
    for (int i = 0; i < 1000; ++i)
    {
        REQUIRE(rng.below(std::numeric_limits<std::uint64_t>::max()) < std::numeric_limits<std::uint64_t>::max());
    }
}

TEST_CASE("Rng between handles negative ranges and single values", "[unit][rng]")
{
    Rng rng(11);
    std::map<int, int> seen;
    for (int i = 0; i < 2000; ++i)
    {
        const int v = rng.between(-5, -1);
        REQUIRE(v >= -5);
        REQUIRE(v <= -1);
        ++seen[v];
    }
    REQUIRE(seen.size() == 5); // every value of the range turns up

    seen.clear();
    for (int i = 0; i < 2000; ++i)
    {
        ++seen[rng.between(-3, 3)];
    }
    REQUIRE(seen.size() == 7);
    REQUIRE(seen.begin()->first == -3);
    REQUIRE(seen.rbegin()->first == 3);

    REQUIRE(rng.between(4, 4) == 4);
    REQUIRE(rng.between(-9, -9) == -9);
    REQUIRE(rng.between(0, 0) == 0);
}

TEST_CASE("Rng between covers the whole int range without overflow", "[unit][rng]")
{
    constexpr int LOWEST = std::numeric_limits<int>::min();
    constexpr int HIGHEST = std::numeric_limits<int>::max();

    Rng rng(5);
    bool saw_negative = false;
    bool saw_positive = false;
    for (int i = 0; i < 2000; ++i)
    {
        const int v = rng.between(LOWEST, HIGHEST);
        saw_negative = saw_negative || v < 0;
        saw_positive = saw_positive || v > 0;
    }
    REQUIRE(saw_negative);
    REQUIRE(saw_positive);

    // Fixed values for seed 1, computed independently.
    Rng fixed(1);
    std::array<int, 5> got{};
    for (auto& value : got)
    {
        value = fixed.between(LOWEST, HIGHEST);
    }
    REQUIRE(got == std::array<int, 5>{151149761, -443618201, 2066896222, 1849870603, 1359066553});

    // Ranges that touch either end of int must stay on their side.
    for (int i = 0; i < 200; ++i)
    {
        const int low_end = rng.between(LOWEST, LOWEST + 1);
        REQUIRE((low_end == LOWEST || low_end == LOWEST + 1));
        const int high_end = rng.between(HIGHEST - 1, HIGHEST);
        REQUIRE((high_end == HIGHEST - 1 || high_end == HIGHEST));
    }
}

TEST_CASE("Rng shuffle is a permutation", "[unit][rng]")
{
    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{10}, std::size_t{52}})
    {
        for (std::uint64_t seed = 1; seed <= 20; ++seed)
        {
            Rng rng(seed);
            std::vector<int> items(size);
            std::iota(items.begin(), items.end(), 0);
            rng.shuffle(std::span<int>(items));

            std::vector<int> sorted = items;
            std::sort(sorted.begin(), sorted.end());
            std::vector<int> expected(size);
            std::iota(expected.begin(), expected.end(), 0);
            REQUIRE(sorted == expected);
        }
    }
}

TEST_CASE("Rng shuffle is fixed for a fixed seed", "[unit][rng]")
{
    {
        Rng rng(1);
        std::array<int, 10> items{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        rng.shuffle(std::span<int>(items));
        REQUIRE(items == std::array<int, 10>{4, 2, 8, 1, 9, 3, 0, 6, 7, 5});
    }
    {
        Rng rng(42);
        std::array<int, 10> items{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        rng.shuffle(std::span<int>(items));
        REQUIRE(items == std::array<int, 10>{0, 9, 5, 8, 6, 4, 7, 2, 1, 3});
    }
}

TEST_CASE("Rng shuffle reaches every ordering of three items about equally", "[unit][rng]")
{
    constexpr int SHUFFLES = 6000;
    Rng rng(99);
    std::map<std::array<int, 3>, int> counts;
    for (int i = 0; i < SHUFFLES; ++i)
    {
        std::array<int, 3> items{0, 1, 2};
        rng.shuffle(std::span<int>(items));
        ++counts[items];
    }
    REQUIRE(counts.size() == 6);
    for (const auto& entry : counts)
    {
        REQUIRE(std::abs(entry.second - SHUFFLES / 6) < SHUFFLES / 6 / 5);
    }
}
