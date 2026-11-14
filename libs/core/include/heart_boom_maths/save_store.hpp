// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace heart_boom_maths
{

/// Where the save text lives. Each platform supplies one; the core only ever sees the text. A real store writes
/// atomically (a temporary file renamed over the old one), which is the platform's part of the job.
class SaveStore
{
public:
    virtual ~SaveStore() = default;

    /// The stored text, or nothing when there is none.
    virtual std::optional<std::string> load() = 0;

    /// Replaces the stored text. Returns false when it could not be written, and then the old text must still be there.
    virtual bool store(std::string_view text) = 0;
};

/// A store that keeps the text in memory, for tests and for a session whose storage is blocked.
class MemorySaveStore final : public SaveStore
{
public:
    MemorySaveStore() = default;
    explicit MemorySaveStore(std::string text);

    std::optional<std::string> load() override;
    bool store(std::string_view text) override;

    /// For tests: make every following store fail, as a full disk or blocked browser storage would.
    void set_store_fails(bool fails) noexcept
    {
        m_StoreFails = fails;
    }

    [[nodiscard]] const std::optional<std::string>& text() const noexcept
    {
        return m_Text;
    }

    [[nodiscard]] int store_count() const noexcept
    {
        return m_StoreCount;
    }

private:
    std::optional<std::string> m_Text;
    bool m_StoreFails = false;
    int m_StoreCount = 0;
};

} // namespace heart_boom_maths
