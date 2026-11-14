// SPDX-License-Identifier: GPL-3.0-or-later
#include "perf_alloc.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace
{

// Relaxed counters: a test reads them on the thread that did the work, and a small error from another thread does not
// matter to a budget.
std::atomic<std::int64_t> g_Allocations{0};
std::atomic<std::int64_t> g_LiveAllocations{0};
std::atomic<std::int64_t> g_LiveBytes{0};

// Each block carries its size in a header the size of the strictest fundamental alignment, so delete can take it off
// the live total without the sized form being used.
constexpr std::size_t HEADER = alignof(std::max_align_t);

void* counted_allocate(std::size_t size)
{
    void* raw = std::malloc(size + HEADER);
    if (raw == nullptr)
    {
        throw std::bad_alloc();
    }
    *static_cast<std::size_t*>(raw) = size;
    g_Allocations.fetch_add(1, std::memory_order_relaxed);
    g_LiveAllocations.fetch_add(1, std::memory_order_relaxed);
    g_LiveBytes.fetch_add(static_cast<std::int64_t>(size), std::memory_order_relaxed);
    return static_cast<char*>(raw) + HEADER;
}

void counted_free(void* pointer) noexcept
{
    if (pointer == nullptr)
    {
        return;
    }
    void* raw = static_cast<char*>(pointer) - HEADER;
    const std::size_t size = *static_cast<std::size_t*>(raw);
    g_LiveAllocations.fetch_sub(1, std::memory_order_relaxed);
    g_LiveBytes.fetch_sub(static_cast<std::int64_t>(size), std::memory_order_relaxed);
    std::free(raw);
}

} // namespace

void* operator new(std::size_t size)
{
    return counted_allocate(size);
}

void* operator new[](std::size_t size)
{
    return counted_allocate(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return counted_allocate(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return counted_allocate(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* pointer) noexcept
{
    counted_free(pointer);
}

void operator delete[](void* pointer) noexcept
{
    counted_free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    counted_free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept
{
    counted_free(pointer);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
    counted_free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
    counted_free(pointer);
}

namespace hbm_perf
{

AllocCounters alloc_counters() noexcept
{
    return {g_Allocations.load(std::memory_order_relaxed), g_LiveAllocations.load(std::memory_order_relaxed),
            g_LiveBytes.load(std::memory_order_relaxed)};
}

} // namespace hbm_perf
