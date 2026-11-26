// MIT License - see LICENSE
#pragma once

// Bounded lock-free queues between the control threads and the audio thread. Both allocate
// their storage once, in the constructor; push() and pop() never allocate, lock or block.

#include <QtCore/qglobal.h>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

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

// An eight-byte value that one thread at a time stores and any thread loads, such as the frame counter that the
// audio thread advances. Where eight-byte atomics are not lock-free, as on a 32-bit Cortex-M, whose toolchains have
// no libatomic to fall back on, it is a sequence lock over two four-byte words: load() never sees a torn value, and
// neither side locks or allocates.
// The tests choose the sequence lock with LockFree = false.
template <typename T, bool LockFree = std::atomic<T>::is_always_lock_free> class SingleWriterAtomic {
    static_assert(sizeof(T) == 8 && std::is_trivially_copyable_v<T>, "an eight-byte value");

public:
    explicit SingleWriterAtomic(T value = T()) { store(value); }
    SingleWriterAtomic(const SingleWriterAtomic &) = delete;
    SingleWriterAtomic &operator=(const SingleWriterAtomic &) = delete;

    void store(T value) {
        if constexpr (kLockFree) {
            m_value.store(value, std::memory_order_release);
        } else {
            quint32 words[2];
            std::memcpy(words, &value, sizeof(words));
            const quint32 sequence = m_sequence.load(std::memory_order_relaxed);
            m_sequence.store(sequence + 1, std::memory_order_relaxed); // odd: a store is under way
            std::atomic_thread_fence(std::memory_order_release);
            m_words[0].store(words[0], std::memory_order_relaxed);
            m_words[1].store(words[1], std::memory_order_relaxed);
            m_sequence.store(sequence + 2, std::memory_order_release);
        }
    }

    T load() const {
        if constexpr (kLockFree) {
            return m_value.load(std::memory_order_acquire);
        } else {
            quint32 words[2];
            for (;;) {
                const quint32 before = m_sequence.load(std::memory_order_acquire);
                words[0] = m_words[0].load(std::memory_order_relaxed);
                words[1] = m_words[1].load(std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_acquire);
                if ((before & 1U) == 0 && m_sequence.load(std::memory_order_relaxed) == before)
                    break;
            }
            T value;
            std::memcpy(&value, words, sizeof(value));
            return value;
        }
    }

private:
    static constexpr bool kLockFree = LockFree;
    // Only the members of one implementation are used.
    std::conditional_t<kLockFree, std::atomic<T>, char> m_value{};
    std::atomic<quint32> m_sequence{0};
    std::atomic<quint32> m_words[2] = {{0}, {0}};
};

} // namespace QtFM::Private
