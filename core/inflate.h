// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

// A small, dependency-free DEFLATE decoder (RFC 1951), with the zlib
// (RFC 1950) wrapper. Enough for the boot images this tree unpacks; not
// tuned for speed.
namespace Inflate {

// Raw DEFLATE stream. Returns false on a malformed stream.
bool raw(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out, size_t *consumed = nullptr);
// zlib stream: 2-byte header, DEFLATE data, Adler-32 (checked).
bool zlib(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out);
// gzip member (RFC 1952): header, DEFLATE data, CRC-32 and size (checked).
bool gzip(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out);
inline bool isGzip(const uint8_t *src, size_t srcLen) {
	return srcLen >= 18 && src[0] == 0x1f && src[1] == 0x8b && src[2] == 8;
}

} // namespace Inflate
