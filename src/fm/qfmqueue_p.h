// MIT License - see LICENSE
#pragma once

// Bounded lock-free queues between the control threads and the audio thread. Both allocate
// their storage once, in the constructor; push() and pop() never allocate, lock or block.

#include <QtCore/qglobal.h>

#include <atomic>
#include <cstddef>
#include <memory>

namespace QtFM::Private {

// Many producers, one consumer (D. Vyukov's bounded queue). A full queue rejects the push.
template <typename T> class MpscQueue {
public:
    explicit MpscQueue(std::size_t capacity) {
        std::size_t size = 2;
        while (size < capacity)
            size <<= 1;
        m_mask = size - 1;
        m_cells.reset(new Cell[size]);
        for (std::size_t i = 0; i < size; ++i)
            m_cells[i].sequence.store(i, std::memory_order_relaxed);
    }

    bool push(const T &value) {
        std::size_t pos = m_tail.load(std::memory_order_relaxed);
        for (;;) {
            Cell &cell = m_cells[pos & m_mask];
            const std::size_t seq = cell.sequence.load(std::memory_order_acquire);
            const auto diff = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos);
            if (diff == 0) {
                if (m_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.value = value;
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                pos = m_tail.load(std::memory_order_relaxed);
            }
        }
    }

    bool pop(T &value) {
        Cell &cell = m_cells[m_head & m_mask];
        const std::size_t seq = cell.sequence.load(std::memory_order_acquire);
        if (static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(m_head + 1) < 0)
            return false;
        value = cell.value;
        cell.sequence.store(m_head + m_mask + 1, std::memory_order_release);
        ++m_head;
        return true;
    }

private:
    struct Cell {
        std::atomic<std::size_t> sequence{0};
        T value{};
    };
    std::unique_ptr<Cell[]> m_cells;
    std::size_t m_mask = 0;
    alignas(64) std::atomic<std::size_t> m_tail{0};
    alignas(64) std::size_t m_head = 0;
};

// One producer, one consumer.
template <typename T> class SpscQueue {
public:
    explicit SpscQueue(std::size_t capacity) : m_size(capacity + 1), m_items(new T[capacity + 1]) {}

    bool push(const T &value) {
        const std::size_t tail = m_tail.load(std::memory_order_relaxed);
        const std::size_t next = tail + 1 == m_size ? 0 : tail + 1;
        if (next == m_head.load(std::memory_order_acquire))
            return false;
        m_items[tail] = value;
        m_tail.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T &value) {
        const std::size_t head = m_head.load(std::memory_order_relaxed);
        if (head == m_tail.load(std::memory_order_acquire))
            return false;
        value = m_items[head];
        m_head.store(head + 1 == m_size ? 0 : head + 1, std::memory_order_release);
        return true;
    }

private:
    std::size_t m_size;
    std::unique_ptr<T[]> m_items;
    alignas(64) std::atomic<std::size_t> m_head{0};
    alignas(64) std::atomic<std::size_t> m_tail{0};
};

} // namespace QtFM::Private
