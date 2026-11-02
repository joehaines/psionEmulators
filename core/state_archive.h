// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// A flat byte stream for saving and restoring a running machine. Each chip
// lists its state once, in a serializeState(StateArchive &) that both saves
// and loads, so the two directions can't drift apart:
//
//     void Chip::serializeState(StateArchive &a) { a(m_count); a(m_regs); }
//
// Only state the emulated machine can observe belongs there: not callbacks
// wired at construction, host settings or diagnostic counters. The stream
// carries no layout information, so it is only good for the build that
// wrote it.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

class StateArchive {
public:
    // Saving appends to `out`.
    explicit StateArchive(std::vector<uint8_t> &out) : m_out(&out) {}
    // Loading reads from [data, data + size).
    StateArchive(const uint8_t *data, size_t size) : m_in(data), m_left(size) {}

    bool loading() const { return m_out == nullptr; }
    // False once a load has run past the end of its data.
    bool ok() const { return m_ok; }
    // Bytes a load left unread: a well-formed stream leaves none.
    size_t left() const { return m_left; }
    // Mark a load as failed: what it read doesn't fit this machine.
    void fail() { m_ok = false; }

    template <class T> void operator()(T &v) {
        static_assert(std::is_trivially_copyable<T>::value, "plain data only");
        bytes(&v, sizeof v);
    }

    // A vector of plain values, its length included.
    template <class T> void operator()(std::vector<T> &v) {
        static_assert(std::is_trivially_copyable<T>::value, "plain data only");
        uint32_t n = uint32_t(v.size());
        (*this)(n);
        if (loading()) {
            if (!m_ok || n * sizeof(T) > m_left) { m_ok = false; return; }
            v.resize(n);
        }
        bytes(v.data(), n * sizeof(T));
    }

    void bytes(void *p, size_t n) {
        if (m_out) {
            const uint8_t *b = static_cast<const uint8_t *>(p);
            m_out->insert(m_out->end(), b, b + n);
        } else if (m_ok && n <= m_left) {
            std::memcpy(p, m_in, n);
            m_in += n;
            m_left -= n;
        } else {
            m_ok = false;
        }
    }

private:
    std::vector<uint8_t> *m_out = nullptr;
    const uint8_t *m_in = nullptr;
    size_t m_left = 0;
    bool m_ok = true;
};
