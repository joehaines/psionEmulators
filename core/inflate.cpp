// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "inflate.h"
#include <cstring>

namespace Inflate {

namespace {

struct Huffman {
	uint16_t count[16];     // codes of each length
	uint16_t symbol[320];   // symbols ordered by code
};

struct Reader {
	const uint8_t *src;
	size_t len, pos = 0;
	uint32_t bitBuf = 0;
	int bitCnt = 0;
	bool error = false;

	int bits(int n) {
		uint32_t v = bitBuf;
		while (bitCnt < n) {
			if (pos >= len) { error = true; return 0; }
			v |= (uint32_t)src[pos++] << bitCnt;
			bitCnt += 8;
		}
		bitBuf = v >> n;
		bitCnt -= n;
		return (int)(v & ((1u << n) - 1));
	}
};

bool build(Huffman &h, const uint8_t *lengths, int n) {
	std::memset(h.count, 0, sizeof(h.count));
	for (int i = 0; i < n; i++) h.count[lengths[i]]++;
	if (h.count[0] == n) return true;          // no codes: fine if never used
	int left = 1;
	for (int len = 1; len < 16; len++) {
		left <<= 1;
		left -= h.count[len];
		if (left < 0) return false;            // over-subscribed
	}
	uint16_t offs[16];
	offs[1] = 0;
	for (int len = 1; len < 15; len++) offs[len + 1] = offs[len] + h.count[len];
	for (int i = 0; i < n; i++)
		if (lengths[i]) h.symbol[offs[lengths[i]]++] = (uint16_t)i;
	return true;
}

int decode(Reader &r, const Huffman &h) {
	int code = 0, first = 0, index = 0;
	for (int len = 1; len < 16; len++) {
		code |= r.bits(1);
		int count = h.count[len];
		if (code - count < first) return h.symbol[index + (code - first)];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
		if (r.error) return -1;
	}
	return -1;
}

const uint16_t kLenBase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
const uint8_t kLenExtra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
const uint16_t kDistBase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                 8193, 12289, 16385, 24577 };
const uint8_t kDistExtra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

bool codes(Reader &r, std::vector<uint8_t> &out, const Huffman &lit, const Huffman &dist) {
	for (;;) {
		int sym = decode(r, lit);
		if (sym < 0) return false;
		if (sym < 256) { out.push_back((uint8_t)sym); continue; }
		if (sym == 256) return true;
		sym -= 257;
		if (sym >= 29) return false;
		size_t len = kLenBase[sym] + r.bits(kLenExtra[sym]);
		int ds = decode(r, dist);
		if (ds < 0 || ds >= 30) return false;
		size_t d = kDistBase[ds] + r.bits(kDistExtra[ds]);
		if (r.error || d > out.size()) return false;
		size_t from = out.size() - d;
		for (size_t i = 0; i < len; i++) out.push_back(out[from + i]);
	}
}

} // namespace

bool raw(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out, size_t *consumed) {
	Reader r{ src, srcLen };
	Huffman fixedLit, fixedDist;
	{
		uint8_t l[288];
		int i = 0;
		for (; i < 144; i++) l[i] = 8;
		for (; i < 256; i++) l[i] = 9;
		for (; i < 280; i++) l[i] = 7;
		for (; i < 288; i++) l[i] = 8;
		build(fixedLit, l, 288);
		for (i = 0; i < 30; i++) l[i] = 5;
		build(fixedDist, l, 30);
	}
	int last;
	do {
		last = r.bits(1);
		int type = r.bits(2);
		if (r.error) return false;
		if (type == 0) {
			r.bitBuf = 0; r.bitCnt = 0;           // to a byte boundary
			if (r.pos + 4 > r.len) return false;
			unsigned n = r.src[r.pos] | (r.src[r.pos + 1] << 8);
			unsigned nn = r.src[r.pos + 2] | (r.src[r.pos + 3] << 8);
			r.pos += 4;
			if ((n ^ 0xffff) != nn || r.pos + n > r.len) return false;
			out.insert(out.end(), r.src + r.pos, r.src + r.pos + n);
			r.pos += n;
		} else if (type == 1) {
			if (!codes(r, out, fixedLit, fixedDist)) return false;
		} else if (type == 2) {
			int nlen = r.bits(5) + 257, ndist = r.bits(5) + 1, ncode = r.bits(4) + 4;
			static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
			uint8_t lengths[320] = {};
			for (int i = 0; i < ncode; i++) lengths[order[i]] = (uint8_t)r.bits(3);
			Huffman lencode;
			if (r.error || !build(lencode, lengths, 19)) return false;
			int idx = 0;
			while (idx < nlen + ndist) {
				int sym = decode(r, lencode);
				if (sym < 0) return false;
				if (sym < 16) { lengths[idx++] = (uint8_t)sym; continue; }
				int len = 0, rep;
				if (sym == 16) { if (idx == 0) return false; len = lengths[idx - 1]; rep = 3 + r.bits(2); }
				else if (sym == 17) rep = 3 + r.bits(3);
				else rep = 11 + r.bits(7);
				if (idx + rep > nlen + ndist) return false;
				while (rep--) lengths[idx++] = (uint8_t)len;
			}
			Huffman lit, dist;
			if (!build(lit, lengths, nlen) || !build(dist, lengths + nlen, ndist)) return false;
			if (!codes(r, out, lit, dist)) return false;
		} else {
			return false;
		}
	} while (!last);
	if (consumed) *consumed = r.pos;
	return !r.error;
}

bool zlib(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out) {
	if (srcLen < 6) return false;
	if ((src[0] & 0x0f) != 8 || ((src[0] << 8) | src[1]) % 31 != 0 || (src[1] & 0x20)) return false;
	size_t used = 0;
	if (!raw(src + 2, srcLen - 2, out, &used)) return false;
	size_t p = 2 + used;
	if (p + 4 > srcLen) return false;
	uint32_t want = ((uint32_t)src[p] << 24) | ((uint32_t)src[p + 1] << 16) | ((uint32_t)src[p + 2] << 8) | src[p + 3];
	uint32_t a = 1, b = 0;
	for (uint8_t c : out) { a = (a + c) % 65521; b = (b + a) % 65521; }
	return ((b << 16) | a) == want;
}

bool gzip(const uint8_t *src, size_t srcLen, std::vector<uint8_t> &out) {
	if (!isGzip(src, srcLen)) return false;
	uint8_t flg = src[3];
	size_t p = 10;
	if (flg & 4) {                                   // FEXTRA
		if (p + 2 > srcLen) return false;
		p += 2 + (src[p] | (src[p + 1] << 8));
	}
	if (flg & 8) { while (p < srcLen && src[p]) p++; p++; }   // FNAME
	if (flg & 16) { while (p < srcLen && src[p]) p++; p++; }  // FCOMMENT
	if (flg & 2) p += 2;                             // FHCRC
	if (p >= srcLen) return false;
	size_t used = 0;
	size_t before = out.size();
	if (!raw(src + p, srcLen - p, out, &used)) return false;
	p += used;
	if (p + 8 > srcLen) return false;
	uint32_t crc = 0xffffffffu;
	for (size_t i = before; i < out.size(); i++) {
		crc ^= out[i];
		for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
	}
	crc = ~crc;
	uint32_t want = src[p] | (src[p + 1] << 8) | (src[p + 2] << 16) | ((uint32_t)src[p + 3] << 24);
	uint32_t isize = src[p + 4] | (src[p + 5] << 8) | (src[p + 6] << 16) | ((uint32_t)src[p + 7] << 24);
	return crc == want && isize == (uint32_t)(out.size() - before);
}

} // namespace Inflate
