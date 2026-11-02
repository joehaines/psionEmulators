// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

#include "xscale.h"
#include <algorithm>
#include <array>
#include <utility>

namespace XScale {

namespace {

inline uint32_t ror32(uint32_t v, uint32_t r) {
	r &= 31;
	return r ? (v >> r) | (v << (32 - r)) : v;
}
inline int32_t sext16(uint32_t v) { return (int32_t)(int16_t)(v & 0xffff); }
inline int64_t sext40(int64_t v) { return (int64_t)((uint64_t)v << 24) >> 24; }

inline int32_t satAdd(int64_t r, uint32_t &q) {
	if (r > INT32_MAX) { q = 1; return INT32_MAX; }
	if (r < INT32_MIN) { q = 1; return INT32_MIN; }
	return (int32_t)r;
}

inline uint32_t ld32(const uint8_t *p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
inline uint32_t ld16(const uint8_t *p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
inline void st32(uint8_t *p, uint32_t v) { std::memcpy(p, &v, 4); }
inline void st16(uint8_t *p, uint32_t v) { uint16_t h = (uint16_t)v; std::memcpy(p, &h, 2); }

// PXA255 B-step core: XScale, ARMv5TE, product 0x2D (PXA255) revision 6.
constexpr uint32_t kCpuId = 0x69052D06;
// 32 KB I and D caches, 32-byte lines, 32-way (XScale cache type register).
constexpr uint32_t kCacheType = 0x0B1AA1AA;

} // namespace

Cpu::Cpu(Bus &bus) : bus_(bus) {
	std::memset(tlbPriv_, 0, sizeof(tlbPriv_));
	std::memset(tlbUser_, 0, sizeof(tlbUser_));
	reset();
}

void Cpu::reset() {
	std::memset(r_, 0, sizeof(r_));
	std::memset(usrR8_14_, 0, sizeof(usrR8_14_));
	std::memset(fiqR8_14_, 0, sizeof(fiqR8_14_));
	std::memset(irqR13_14_, 0, sizeof(irqR13_14_));
	std::memset(svcR13_14_, 0, sizeof(svcR13_14_));
	std::memset(abtR13_14_, 0, sizeof(abtR13_14_));
	std::memset(undR13_14_, 0, sizeof(undR13_14_));
	spsrFiq_ = spsrIrq_ = spsrSvc_ = spsrAbt_ = spsrUnd_ = 0;
	fN_ = fZ_ = fC_ = fV_ = fQ_ = 0;
	mode_ = SVC;
	iMask_ = fMask_ = true;
	tbit_ = false;
	cp15Control_ = 0x00000078;
	cp15Aux_ = 0;
	ttb_ = dacr_ = fsr_ = far_ = pid_ = cpar_ = 0;
	std::memset(cp15Dbg_, 0, sizeof(cp15Dbg_));
	cclkcfg_ = pwrmode_ = 0;
	pmnc_ = ccnt_ = pmn0_ = pmn1_ = evtsel_ = inten_ = flag_ = 0;
	ccntBase_ = insnCount_;
	acc0_ = 0;
	idle_ = false;
	aborted_ = false;
	pc_ = 0;
	invalidateDecodeAll();
	flushTlb();
}

// ── PSR and banking ─────────────────────────────────────────────────────

uint32_t Cpu::cpsr() const {
	return (fN_ << 31) | (fZ_ << 30) | (fC_ << 29) | (fV_ << 28) | (fQ_ << 27) |
	       ((uint32_t)iMask_ << 7) | ((uint32_t)fMask_ << 6) | ((uint32_t)tbit_ << 5) | mode_;
}

void Cpu::setCpsr(uint32_t v) {
	fN_ = (v >> 31) & 1; fZ_ = (v >> 30) & 1; fC_ = (v >> 29) & 1; fV_ = (v >> 28) & 1;
	fQ_ = (v >> 27) & 1;
	iMask_ = (v >> 7) & 1;
	fMask_ = (v >> 6) & 1;
	tbit_ = (v >> 5) & 1;
	switchMode(v & 0x1f);
}

uint32_t *Cpu::spsrPtr() {
	switch (mode_) {
	case FIQ: return &spsrFiq_;
	case IRQ: return &spsrIrq_;
	case SVC: return &spsrSvc_;
	case ABT: return &spsrAbt_;
	case UND: return &spsrUnd_;
	default:  return nullptr;
	}
}

static inline uint32_t *bank13_14(uint32_t mode, uint32_t *usr, uint32_t *irq, uint32_t *svc,
                                  uint32_t *abt, uint32_t *und) {
	switch (mode) {
	case IRQ: return irq;
	case SVC: return svc;
	case ABT: return abt;
	case UND: return und;
	default:  return usr;          // USR, SYS and anything unrecognised
	}
}

void Cpu::switchMode(uint32_t nm) {
	nm |= 0x10;
	uint32_t om = mode_;
	if (nm == om) return;
	// Save the outgoing mode's banked registers.
	if (om == FIQ) {
		for (int i = 0; i < 7; i++) fiqR8_14_[i] = r_[8 + i];
	} else {
		for (int i = 0; i < 5; i++) usrR8_14_[i] = r_[8 + i];
		uint32_t *b = bank13_14(om, &usrR8_14_[5], irqR13_14_, svcR13_14_, abtR13_14_, undR13_14_);
		b[0] = r_[13]; b[1] = r_[14];
	}
	// Load the incoming mode's.
	if (nm == FIQ) {
		for (int i = 0; i < 7; i++) r_[8 + i] = fiqR8_14_[i];
	} else {
		for (int i = 0; i < 5; i++) r_[8 + i] = usrR8_14_[i];
		uint32_t *b = bank13_14(nm, &usrR8_14_[5], irqR13_14_, svcR13_14_, abtR13_14_, undR13_14_);
		r_[13] = b[0]; r_[14] = b[1];
	}
	mode_ = nm;
	fetchTag_ = 0;   // privilege may have changed: the fetch page's permission too
	dVpage_ = 1;
}

void Cpu::restoreCpsrFromSpsr() {
	uint32_t *s = spsrPtr();
	if (s) setCpsr(*s);
}

// ── Exceptions ──────────────────────────────────────────────────────────

void Cpu::exception(uint32_t off, uint32_t newMode, uint32_t lr, bool disableFiq) {
	uint32_t old = cpsr();
	switchMode(newMode);
	*spsrPtr() = old;
	r_[14] = lr;
	iMask_ = true;
	if (disableFiq) fMask_ = true;
	tbit_ = false;
	pc_ = ((cp15Control_ & 0x2000) ? 0xffff0000u : 0u) + off;
	branched_ = true;
	lastExcPc_ = lr;
	excCount[off >> 2]++;
}

void Cpu::undefined() {
	exception(0x04, UND, pc_ + (tbit_ ? 2 : 4), false);
}

void Cpu::dataAbort(uint32_t va, uint32_t status, uint32_t domain) {
	fsr_ = (status & 0xf) | ((domain & 0xf) << 4);
	far_ = va;
	aborted_ = true;
}

void Cpu::prefetchAbort(uint32_t va, uint32_t status, uint32_t domain) {
	(void)va; (void)status; (void)domain;
	exception(0x0C, ABT, pc_ + 4, false);
}

// ── MMU ─────────────────────────────────────────────────────────────────

void Cpu::flushTlb() {
	dVpage_ = 1;
	tlbGen_++;
	if (tlbGen_ >= 0x1000) {
		std::memset(tlbPriv_, 0, sizeof(tlbPriv_));
		std::memset(tlbUser_, 0, sizeof(tlbUser_));
		tlbGen_ = 1;
	}
	fetchTag_ = 0;
}

uint32_t Cpu::translate(uint32_t va, bool write, bool user, uint32_t &pa, uint32_t &domOut,
                        TlbEnt *fill) {
	uint32_t perm;
	bool uniform4k = true;    // whole 4 KB page shares one translation + AP
	if (!(cp15Control_ & 1)) {
		pa = va;
		domOut = 0;
		perm = 3;
	} else {
		auto readPt = [&](uint32_t addr, uint32_t &out) -> bool {
			if (addr - ramBase_ < ramSize_ && addr - ramBase_ <= ramSize_ - 4) {
				out = ld32(ram_ + (addr - ramBase_));
				return true;
			}
			return bus_.busRead(addr, 4, out);
		};
		uint32_t l1;
		if (!readPt((ttb_ & 0xffffc000u) | ((va >> 18) & 0x3ffc), l1)) { domOut = 0; return 12; }
		uint32_t dom = (l1 >> 5) & 15;
		domOut = dom;
		uint32_t ap;
		bool section = false;
		switch (l1 & 3) {
		case 0:
			return 5;          // section translation fault
		case 2:
			section = true;
			pa = (l1 & 0xfff00000u) | (va & 0x000fffffu);
			ap = (l1 >> 10) & 3;
			break;
		default: {
			bool fine = (l1 & 3) == 3;
			uint32_t l2addr = fine ? ((l1 & 0xfffff000u) | ((va >> 8) & 0xffc))
			                       : ((l1 & 0xfffffc00u) | ((va >> 10) & 0x3fc));
			uint32_t l2;
			if (!readPt(l2addr, l2)) return 14;
			switch (l2 & 3) {
			case 0:
				return 7;      // page translation fault
			case 1: {          // large page, 64 KB, four AP subpages of 16 KB
				pa = (l2 & 0xffff0000u) | (va & 0xffff);
				ap = (l2 >> (4 + ((va >> 13) & 6))) & 3;
				break;
			}
			case 2: {          // small page, 4 KB, four AP subpages of 1 KB
				pa = (l2 & 0xfffff000u) | (va & 0xfff);
				ap = (l2 >> (4 + ((va >> 9) & 6))) & 3;
				uint32_t aps = (l2 >> 4) & 0xff;
				uniform4k = aps == ((aps & 3) * 0x55);
				break;
			}
			default:
				if (fine) {    // tiny page, 1 KB
					pa = (l2 & 0xfffffc00u) | (va & 0x3ff);
					uniform4k = false;
				} else {       // XScale extended small page, 4 KB, one AP
					pa = (l2 & 0xfffff000u) | (va & 0xfff);
				}
				ap = (l2 >> 4) & 3;
				break;
			}
			break;
		}
		}
		uint32_t d = (dacr_ >> (dom * 2)) & 3;
		if (d == 0 || d == 2) return section ? 9 : 11;   // domain fault
		if (d == 3) {
			perm = 3;
		} else {
			bool S = cp15Control_ & 0x100, R = cp15Control_ & 0x200;
			switch (ap) {
			case 0:
				if (S && !R) perm = user ? 0 : 1;
				else if (!S && R) perm = 1;
				else perm = 0;
				break;
			case 1: perm = user ? 0 : 3; break;
			case 2: perm = user ? 1 : 3; break;
			default: perm = 3; break;
			}
		}
		if (!(perm & (write ? 2 : 1))) return section ? 13 : 15;
	}
	if (fill && uniform4k) {
		uint32_t page = pa & ~0xfffu;
		fill->tag = (va & ~0xfffu) | tlbGen_;
		fill->pa = page;
		fill->perm = (uint8_t)perm;
		uint8_t *host = hostPtr(page);
		fill->hostR = (perm & 1) ? host : nullptr;
		fill->hostW = ((perm & 2) && !isCodePage(page)) ? host : nullptr;
	}
	return 0;
}

bool Cpu::readSlow(uint32_t va, int size, uint32_t &out, bool user) {
	TlbEnt *e = (user ? tlbUser_ : tlbPriv_) + ((va >> 12) & (TlbSize - 1));
	uint32_t pa, dom = 0;
	if (e->tag == ((va & ~0xfffu) | tlbGen_) && (e->perm & 1)) {
		pa = e->pa | (va & 0xfff);
	} else {
		uint32_t st = translate(va, false, user, pa, dom, e);
		if (st) { dataAbort(va, st, dom); return false; }
	}
	if (const uint8_t *p = hostPtr(pa)) {
		out = size == 4 ? ld32(p) : size == 2 ? ld16(p) : *p;
		return true;
	}
	uint32_t v = 0;
	if (!bus_.busRead(pa, size, v)) { dataAbort(va, 8, dom); return false; }
	out = size == 4 ? v : size == 2 ? (v & 0xffff) : (v & 0xff);
	return true;
}

bool Cpu::writeSlow(uint32_t va, int size, uint32_t val, bool user) {
	TlbEnt *e = (user ? tlbUser_ : tlbPriv_) + ((va >> 12) & (TlbSize - 1));
	uint32_t pa, dom = 0;
	if (e->tag == ((va & ~0xfffu) | tlbGen_) && (e->perm & 2)) {
		pa = e->pa | (va & 0xfff);
	} else {
		uint32_t st = translate(va, true, user, pa, dom, e);
		if (st) { dataAbort(va, st, dom); return false; }
	}
	if (uint8_t *p = hostPtr(pa)) {
		uint32_t off = pa - ramBase_;
		if (off < ramSize_) {
			uint32_t page = off >> 12;
			if (uint16_t m = codeMask_[page]) {
				uint32_t first = (off & 0xfff) >> 8, last = ((off & 0xfff) + size - 1) >> 8;
				for (uint32_t ch = first; ch <= last && ch < 16; ch++)
					if (m & (1u << ch)) invalidateCodeChunk(page, ch);
			}
			if (!codeMask_[page] && e->tag == ((va & ~0xfffu) | tlbGen_) && (e->perm & 2) && !e->hostW)
				e->hostW = p - (pa & 0xfff);
		}
		if (size == 4) st32(p, val); else if (size == 2) st16(p, val); else *p = (uint8_t)val;
		return true;
	}
	if (!bus_.busWrite(pa, size, size == 4 ? val : size == 2 ? (val & 0xffff) : (val & 0xff))) {
		dataAbort(va, 8, dom);
		return false;
	}
	return true;
}

bool Cpu::fetchSlow(uint32_t va, uint32_t &out, int size) {
	bool user = mode_ == USR;
	TlbEnt *e = (user ? tlbUser_ : tlbPriv_) + ((va >> 12) & (TlbSize - 1));
	uint32_t pa, dom = 0;
	if (e->tag == ((va & ~0xfffu) | tlbGen_) && (e->perm & 1)) {
		pa = e->pa | (va & 0xfff);
	} else {
		uint32_t st = translate(va, false, user, pa, dom, e);
		if (st) return false;
	}
	if (const uint8_t *p = hostPtr(pa)) {
		if (e->tag == ((va & ~0xfffu) | tlbGen_) && e->hostR) {
			fetchTag_ = (va & ~0xfffu) | tlbGen_;
			fetchHost_ = e->hostR;
		}
		out = size == 4 ? ld32(p) : ld16(p);
		return true;
	}
	uint32_t v = 0;
	if (!bus_.busRead(pa, size, v)) return false;
	out = size == 4 ? v : (v & 0xffff);
	return true;
}

#define XS_MVA(va) do { if (pid_ && (va) < 0x02000000u) (va) |= pid_; } while (0)
#define XS_TLB(user, va) ((user) ? tlbUser_ : tlbPriv_)[((va) >> 12) & (TlbSize - 1)]
#define XS_TAG(va) (((va) & ~0xfffu) | tlbGen_)

inline bool Cpu::read32(uint32_t va, uint32_t &out, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostR) { out = ld32(e.hostR + (va & 0xfff)); return true; }
	return readSlow(va, 4, out, user);
}
inline bool Cpu::read16(uint32_t va, uint32_t &out, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostR) { out = ld16(e.hostR + (va & 0xfff)); return true; }
	return readSlow(va, 2, out, user);
}
inline bool Cpu::read8(uint32_t va, uint32_t &out, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostR) { out = e.hostR[va & 0xfff]; return true; }
	return readSlow(va, 1, out, user);
}
inline bool Cpu::write32(uint32_t va, uint32_t v, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostW) { st32(e.hostW + (va & 0xfff), v); return true; }
	return writeSlow(va, 4, v, user);
}
inline bool Cpu::write16(uint32_t va, uint32_t v, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostW) { st16(e.hostW + (va & 0xfff), v); return true; }
	return writeSlow(va, 2, v, user);
}
inline bool Cpu::write8(uint32_t va, uint32_t v, bool user) {
	XS_MVA(va);
	const TlbEnt &e = XS_TLB(user, va);
	if (e.tag == XS_TAG(va) && e.hostW) { e.hostW[va & 0xfff] = (uint8_t)v; return true; }
	return writeSlow(va, 1, v, user);
}
inline bool Cpu::fetch32(uint32_t va, uint32_t &out) {
	XS_MVA(va);
	if (XS_TAG(va) == fetchTag_) { out = ld32(fetchHost_ + (va & 0xfff)); return true; }
	return fetchSlow(va, out, 4);
}
inline bool Cpu::fetch16(uint32_t va, uint32_t &out) {
	XS_MVA(va);
	if (XS_TAG(va) == fetchTag_) { out = ld16(fetchHost_ + (va & 0xfff)); return true; }
	return fetchSlow(va, out, 2);
}

bool Cpu::debugTranslate(uint32_t va, uint32_t &pa) {
	XS_MVA(va);
	uint32_t dom;
	return translate(va, false, false, pa, dom, nullptr) == 0;
}

bool Cpu::debugRead32(uint32_t va, uint32_t &out) {
	uint32_t pa;
	if (!debugTranslate(va & ~3u, pa)) return false;
	if (pa - ramBase_ < ramSize_ - 3) { out = ld32(ram_ + (pa - ramBase_)); return true; }
	return false;
}

bool Cpu::debugRead8(uint32_t va, uint8_t &out) {
	uint32_t pa;
	if (!debugTranslate(va, pa)) return false;
	if (pa - ramBase_ < ramSize_) { out = ram_[pa - ramBase_]; return true; }
	return false;
}

// ── Decoded instruction cache ───────────────────────────────────────────
//
// ARM-state code in main memory is decoded once into DInsn records, kept per
// physical 4 KB page and indexed by (pc >> 2) & 1023, and run through a table
// of specialised handlers: data processing for every opcode / S / operand
// form (immediate, register, register shifted by a constant), word, byte and
// halfword loads and stores with immediate or LSL-register offsets, literal
// loads, branches, MUL/MLA. Anything else — and the awkward cases of the
// common forms (the PC as an operand, rotate-with-extend, unaligned
// accesses, the T variants) — keeps its raw word and goes to the plain
// decoder, so the semantics are those of execArm by construction.
//
// Coherence: a page with decoded entries ("code page") gets no direct write
// pointer in the TLB, so every store to it reaches writeSlow, which drops the
// decoded entries of the 256-byte chunk it touches. Turning a page into a
// code page flushes the TLB so no entry from before keeps a write pointer.
// Writes that bypass the core (DMA, the host) report through notifyRamWrite.

namespace {
constexpr unsigned OP_GENERIC = 1, OP_B = 2, OP_BL = 3, OP_BLOCK = 4, OP_MUL = 5, OP_LDRLIT = 6, OP_BLOCKFAST = 7;
constexpr unsigned OP_DP = 16, OP_SLS = OP_DP + 192, OP_EXTRA = OP_SLS + 32, OP_COUNT = OP_EXTRA + 32;
constexpr size_t kMaxDecodedPages = 4096;    // 48 MB of records at most
} // namespace

void Cpu::invalidateDecodeAll() {
	for (DPage *&p : dPages_) {
		if (p) { dFree_.push_back(p); p = nullptr; }
	}
	std::fill(codeMask_.begin(), codeMask_.end(), 0);
	dLive_ = 0;
	dVpage_ = 1;
	dPage_ = nullptr;
}

void Cpu::invalidateCodeChunk(uint32_t page, uint32_t chunk) {
	DPage *p = dPages_[page];
	if (p) std::memset(&p->ins[chunk * 64], 0, 64 * sizeof(DInsn));
	codeMask_[page] &= (uint16_t)~(1u << chunk);
}

void Cpu::notifyRamWrite(uint32_t pa, uint32_t len) {
	if (!len) return;
	uint32_t off = pa - ramBase_;
	if (off >= ramSize_) return;
	uint32_t end = std::min<uint64_t>((uint64_t)off + len, ramSize_);
	for (uint32_t a = off & ~0xffu; a < end; a += 0x100) {
		uint32_t page = a >> 12, chunk = (a >> 8) & 15;
		if (codeMask_[page] & (1u << chunk)) invalidateCodeChunk(page, chunk);
	}
}

bool Cpu::lookupDecoded(uint32_t pc) {
	uint32_t va = pc;
	XS_MVA(va);
	bool user = mode_ == USR;
	TlbEnt *e = &XS_TLB(user, va);
	if (!(e->tag == XS_TAG(va) && (e->perm & 1))) {
		uint32_t pa, dom;
		if (translate(va, false, user, pa, dom, e)) return false;     // plain path raises the abort
		if (e->tag != XS_TAG(va)) return false;                       // not cacheable (tiny pages)
	}
	uint32_t off = e->pa - ramBase_;
	if (off >= ramSize_) return false;                                // not main memory
	uint32_t idx = off >> 12;
	DPage *p = dPages_[idx];
	if (!p) {
		if (dLive_ >= kMaxDecodedPages) {
			invalidateDecodeAll();
			flushTlb();
		}
		if (!dFree_.empty()) { p = dFree_.back(); dFree_.pop_back(); }
		else p = new DPage;
		std::memset(p, 0, sizeof(DPage));
		dPages_[idx] = p;
		dLive_++;
	}
	dPage_ = p;
	dIndex_ = idx;
	dHost_ = ram_ + off;
	dVpage_ = pc & ~0xfffu;
	return true;
}

void Cpu::decodeArm(DInsn &d, uint32_t insn, uint32_t pc) {
	(void)pc;   // decoded records must not depend on the virtual address
	d = DInsn{};
	d.op = OP_GENERIC;
	d.cond = 0xE;
	d.imm = insn;
	uint32_t cond = insn >> 28;
	if (cond == 0xF) return;
	uint8_t rd = (insn >> 12) & 15, rn = (insn >> 16) & 15, rm = insn & 15;
	switch ((insn >> 25) & 7) {
	case 0: case 1: {
		bool imm = insn & (1u << 25);
		if (!imm && (insn & 0x90) == 0x90) {
			if ((insn & 0x0fe000f0) == 0x00000090 || (insn & 0x0fe000f0) == 0x00200090) {
				// MUL / MLA without S
				uint8_t mrd = (insn >> 16) & 15, mrn = (insn >> 12) & 15, mrs = (insn >> 8) & 15;
				if ((insn & (1u << 20)) || mrd == 15 || mrn == 15 || mrs == 15 || rm == 15) return;
				d.op = OP_MUL; d.rd = mrd; d.rn = mrn; d.rs = mrs; d.rm = rm;
				d.aux = (insn >> 21) & 1;
				break;
			}
			uint32_t sh = (insn >> 5) & 3;
			if (sh == 0) return;                                // SWP and friends
			bool P = insn & (1u << 24), W = insn & (1u << 21), L = insn & (1u << 20);
			bool I = insn & (1u << 22);
			if (!L && sh != 1) return;                          // LDRD / STRD
			if (rd == 15 || rn == 15 || (!P && W)) return;
			if ((W || !P) && rn == rd) return;
			if (!I && rm == 15) return;
			unsigned T = L ? (sh == 1 ? 1 : sh == 2 ? 2 : 3) : 0;
			d.op = (uint16_t)(OP_EXTRA + ((T << 3) | (P << 2) | (W << 1) | (I ? 0 : 1)));
			d.rd = rd; d.rn = rn; d.rm = rm;
			break;
		}
		uint32_t opc = (insn >> 21) & 15;
		bool S = insn & (1u << 20);
		bool test = opc >= 8 && opc <= 11;
		if (test && !S) return;                                 // MRS / MSR / BX / CLZ / ...
		if (!test && rd == 15) return;
		if (rn == 15 && opc != 13 && opc != 15) return;
		unsigned K;
		if (imm) {
			uint32_t rot = (insn >> 7) & 0x1e;
			d.imm = ror32(insn & 0xff, rot);
			d.aux = rot != 0;
			K = 0;
		} else {
			if (insn & 0x10) return;                            // shift by register
			if (rm == 15) return;
			uint32_t amt = (insn >> 7) & 31;
			switch ((insn >> 5) & 3) {
			case 0: K = amt ? 2 : 1; d.rs = (uint8_t)amt; break;
			case 1: K = 3; d.rs = (uint8_t)(amt ? amt : 32); break;
			case 2: K = 4; d.rs = (uint8_t)(amt ? amt : 32); break;
			default:
				if (!amt) return;                               // RRX
				K = 5; d.rs = (uint8_t)amt; break;
			}
		}
		d.op = (uint16_t)(OP_DP + opc * 12 + (S ? 6 : 0) + K);
		d.rd = rd; d.rn = rn; d.rm = rm;
		break;
	}
	case 2: case 3: {
		bool R = insn & (1u << 25);
		if (R && (insn & 0x10)) return;
		bool P = insn & (1u << 24), B = insn & (1u << 22), W = insn & (1u << 21), L = insn & (1u << 20);
		if (!P && W) return;                                    // LDRT / STRT
		if (rd == 15) return;
		if (rn == 15) {
			if (L && !B && P && !W && !R) {
				// Kept PC-relative: one physical page can be code at several
				// virtual addresses (a shared library in two processes).
				uint32_t off = insn & 0xfff;
				if (off & 3) return;
				d.op = OP_LDRLIT; d.rd = rd;
				d.imm = (insn & (1u << 23)) ? 8 + off : 8 - off;
				break;
			}
			return;
		}
		if ((W || !P) && rn == rd) return;
		if (R) {
			if (rm == 15 || ((insn >> 5) & 3) != 0) return;     // LSL only
			d.rs = (insn >> 7) & 31;
		}
		d.op = (uint16_t)(OP_SLS + ((L << 4) | (B << 3) | (P << 2) | (W << 1) | (R ? 1 : 0)));
		d.rd = rd; d.rn = rn; d.rm = rm;
		break;
	}
	case 4: {
		uint32_t list = insn & 0xffff;
		bool S = insn & (1u << 22), W = insn & (1u << 21), L = insn & (1u << 20);
		d.op = OP_BLOCK;
		if (!S && rn != 15 && list && !(W && (list & (1u << rn)))) {
			d.op = OP_BLOCKFAST;
			d.rn = rn;
			d.rs = (uint8_t)__builtin_popcount(list);
		}
		(void)L;
		break;
	}
	case 5:
		d.op = (insn & (1u << 24)) ? OP_BL : OP_B;
		d.imm = 8 + (uint32_t)((int32_t)(insn << 8) >> 6);
		break;
	default:
		return;
	}
	d.cond = (uint8_t)cond;
}

void Cpu::hGeneric(Cpu &c, const DInsn &d) { c.execArm(d.imm); }
void Cpu::hB(Cpu &c, const DInsn &d) { c.branchTo(c.pc_ + d.imm); }
void Cpu::hBL(Cpu &c, const DInsn &d) { c.r_[14] = c.pc_ + 4; c.branchTo(c.pc_ + d.imm); }
void Cpu::hBlock(Cpu &c, const DInsn &d) { c.armBlock(d.imm); }

// LDM / STM without the S bit, moving whole words within one page that the
// TLB already maps to host memory; anything else goes to armBlock.
void Cpu::hBlockFast(Cpu &c, const DInsn &d) {
	uint32_t raw = d.imm;
	uint32_t list = raw & 0xffff;
	uint32_t n = d.rs;
	uint32_t base = c.r_[d.rn];
	bool U = raw & (1u << 23), P = raw & (1u << 24), W = raw & (1u << 21), L = raw & (1u << 20);
	uint32_t start = U ? (P ? base + 4 : base) : (P ? base - 4 * n : base - 4 * n + 4);
	uint32_t addr = start & ~3u;
	uint32_t last = addr + 4 * n - 4;
	if ((addr ^ last) & ~0xfffu) { c.armBlock(raw); return; }
	uint32_t va = addr;
	if (c.pid_ && va < 0x02000000u) va |= c.pid_;
	bool user = c.mode_ == USR;
	const TlbEnt &e = (user ? c.tlbUser_ : c.tlbPriv_)[(va >> 12) & (TlbSize - 1)];
	uint8_t *host = L ? e.hostR : e.hostW;
	if (e.tag != ((va & ~0xfffu) | c.tlbGen_) || !host) { c.armBlock(raw); return; }
	uint8_t *p = host + (va & 0xfff);
	uint32_t newBase = U ? base + 4 * n : base - 4 * n;
	if (L) {
		uint32_t pcv = 0;
		for (uint32_t i = 0; i < 16; i++) {
			if (!(list & (1u << i))) continue;
			uint32_t v = ld32(p);
			p += 4;
			if (i == 15) pcv = v; else c.r_[i] = v;
		}
		if (W) c.r_[d.rn] = newBase;
		if (list & 0x8000) c.branchInterwork(pcv);
	} else {
		for (uint32_t i = 0; i < 16; i++) {
			if (!(list & (1u << i))) continue;
			st32(p, i == 15 ? c.pc_ + 8 : c.r_[i]);
			p += 4;
		}
		if (W) c.r_[d.rn] = newBase;
	}
}
void Cpu::hMul(Cpu &c, const DInsn &d) {
	uint32_t r = c.r_[d.rm] * c.r_[d.rs];
	if (d.aux) r += c.r_[d.rn];
	c.r_[d.rd] = r;
}
void Cpu::hLdrLit(Cpu &c, const DInsn &d) {
	uint32_t v;
	if (c.read32(c.pc_ + d.imm, v, c.mode_ == USR)) c.r_[d.rd] = v;
}

template <unsigned OP, bool S, unsigned K>
void Cpu::hDp(Cpu &c, const DInsn &d) {
	uint32_t b, shC = c.fC_;
	if constexpr (K == 0) {
		b = d.imm;
		if (S && d.aux) shC = b >> 31;
	} else {
		uint32_t m = c.r_[d.rm], a = d.rs;
		if constexpr (K == 1) b = m;
		else if constexpr (K == 2) { b = m << a; if (S) shC = (m >> (32 - a)) & 1; }
		else if constexpr (K == 3) { b = a == 32 ? 0 : m >> a; if (S) shC = (m >> (a - 1)) & 1; }
		else if constexpr (K == 4) { b = (uint32_t)((int32_t)m >> (a == 32 ? 31 : a)); if (S) shC = (m >> (a - 1)) & 1; }
		else { b = ror32(m, a); if (S) shC = b >> 31; }
	}
	uint32_t x = c.r_[d.rn], res;
	constexpr bool logical = OP == 0 || OP == 1 || OP == 8 || OP == 9 || OP >= 12;
	constexpr bool writes = OP < 8 || OP >= 12;
	uint32_t cf = 0, v = 0;
	if constexpr (OP == 0x0 || OP == 0x8) res = x & b;
	else if constexpr (OP == 0x1 || OP == 0x9) res = x ^ b;
	else if constexpr (OP == 0x2 || OP == 0xA) { res = x - b; if (S) { cf = x >= b; v = ((x ^ b) & (x ^ res)) >> 31; } }
	else if constexpr (OP == 0x3) { res = b - x; if (S) { cf = b >= x; v = ((b ^ x) & (b ^ res)) >> 31; } }
	else if constexpr (OP == 0x4 || OP == 0xB) { res = x + b; if (S) { cf = res < x; v = (~(x ^ b) & (x ^ res)) >> 31; } }
	else if constexpr (OP == 0x5) {
		uint64_t s = (uint64_t)x + b + c.fC_;
		res = (uint32_t)s;
		if (S) { cf = (uint32_t)(s >> 32); v = (~(x ^ b) & (x ^ res)) >> 31; }
	} else if constexpr (OP == 0x6) {
		uint32_t borrow = c.fC_ ^ 1;
		res = x - b - borrow;
		if (S) { cf = (uint64_t)x >= (uint64_t)b + borrow; v = ((x ^ b) & (x ^ res)) >> 31; }
	} else if constexpr (OP == 0x7) {
		uint32_t borrow = c.fC_ ^ 1;
		res = b - x - borrow;
		if (S) { cf = (uint64_t)b >= (uint64_t)x + borrow; v = ((b ^ x) & (b ^ res)) >> 31; }
	} else if constexpr (OP == 0xC) res = x | b;
	else if constexpr (OP == 0xD) res = b;
	else if constexpr (OP == 0xE) res = x & ~b;
	else res = ~b;
	if constexpr (writes) c.r_[d.rd] = res;
	if constexpr (S) {
		c.fN_ = res >> 31;
		c.fZ_ = res == 0;
		if constexpr (logical) c.fC_ = shC;
		else { c.fC_ = cf; c.fV_ = v; }
	}
	(void)shC; (void)cf; (void)v; (void)x;
}

template <bool L, bool B, bool P, bool W, bool R>
void Cpu::hSls(Cpu &c, const DInsn &d) {
	uint32_t raw = d.imm;
	uint32_t off = R ? (c.r_[d.rm] << d.rs) : (raw & 0xfff);
	uint32_t base = c.r_[d.rn];
	uint32_t moved = (raw & (1u << 23)) ? base + off : base - off;
	uint32_t addr = P ? moved : base;
	bool user = c.mode_ == USR;
	if constexpr (L) {
		uint32_t v;
		if constexpr (B) {
			if (!c.read8(addr, v, user)) return;
		} else {
			if (addr & 3) { c.armSingleLoadStore(raw); return; }
			if (!c.read32(addr, v, user)) return;
		}
		if (!P || W) c.r_[d.rn] = moved;
		c.r_[d.rd] = v;
	} else {
		uint32_t v = c.r_[d.rd];
		if constexpr (B) {
			if (!c.write8(addr, v, user)) return;
		} else {
			if (addr & 3) { c.armSingleLoadStore(raw); return; }
			if (!c.write32(addr, v, user)) return;
		}
		if (!P || W) c.r_[d.rn] = moved;
	}
}

template <unsigned T, bool P, bool W, bool R>
void Cpu::hExtra(Cpu &c, const DInsn &d) {
	uint32_t raw = d.imm;
	uint32_t off = R ? c.r_[d.rm] : (((raw >> 4) & 0xf0) | (raw & 0xf));
	uint32_t base = c.r_[d.rn];
	uint32_t moved = (raw & (1u << 23)) ? base + off : base - off;
	uint32_t addr = P ? moved : base;
	bool user = c.mode_ == USR;
	uint32_t v;
	if constexpr (T == 0) {
		if (addr & 1) { c.armExtraLoadStore(raw); return; }
		if (!c.write16(addr, c.r_[d.rd], user)) return;
		if (!P || W) c.r_[d.rn] = moved;
		return;
	} else if constexpr (T == 1) {
		if (addr & 1) { c.armExtraLoadStore(raw); return; }
		if (!c.read16(addr, v, user)) return;
	} else if constexpr (T == 2) {
		if (!c.read8(addr, v, user)) return;
		v = (uint32_t)(int32_t)(int8_t)v;
	} else {
		if (addr & 1) { c.armExtraLoadStore(raw); return; }
		if (!c.read16(addr, v, user)) return;
		v = (uint32_t)(int32_t)(int16_t)v;
	}
	if (!P || W) c.r_[d.rn] = moved;
	c.r_[d.rd] = v;
}

namespace {
template <unsigned N>
constexpr Cpu::Handler handlerFor() {
	if constexpr (N >= OP_EXTRA) {
		constexpr unsigned i = N - OP_EXTRA;
		return &Cpu::hExtra<(i >> 3) & 3, bool((i >> 2) & 1), bool((i >> 1) & 1), bool(i & 1)>;
	} else if constexpr (N >= OP_SLS) {
		constexpr unsigned i = N - OP_SLS;
		return &Cpu::hSls<bool((i >> 4) & 1), bool((i >> 3) & 1), bool((i >> 2) & 1), bool((i >> 1) & 1), bool(i & 1)>;
	} else if constexpr (N >= OP_DP) {
		constexpr unsigned i = N - OP_DP;
		return &Cpu::hDp<i / 12, bool((i / 6) & 1), i % 6>;
	} else if constexpr (N == OP_B) {
		return &Cpu::hB;
	} else if constexpr (N == OP_BL) {
		return &Cpu::hBL;
	} else if constexpr (N == OP_BLOCK) {
		return &Cpu::hBlock;
	} else if constexpr (N == OP_BLOCKFAST) {
		return &Cpu::hBlockFast;
	} else if constexpr (N == OP_MUL) {
		return &Cpu::hMul;
	} else if constexpr (N == OP_LDRLIT) {
		return &Cpu::hLdrLit;
	} else {
		return &Cpu::hGeneric;
	}
}
template <size_t... I>
constexpr std::array<Cpu::Handler, sizeof...(I)> makeHandlerTable(std::index_sequence<I...>) {
	return { { handlerFor<(unsigned)I>()... } };
}
constexpr auto kHandlerTable = makeHandlerTable(std::make_index_sequence<OP_COUNT>{});
} // namespace

// ── Main loop ───────────────────────────────────────────────────────────

uint32_t Cpu::run(uint32_t n) {
	runDone_ = 0;
	runLimit_ = n;
	while (runDone_ < runLimit_) {
		if (__builtin_expect(irqLine_ | fiqLine_ | idle_, 0)) {
			if (fiqLine_ && !fMask_) {
				idle_ = false;
				exception(0x1C, FIQ, pc_ + 4, true);
			} else if (irqLine_ && !iMask_) {
				idle_ = false;
				exception(0x18, IRQ, pc_ + 4, false);
			}
			if (idle_) break;
		}
		branched_ = false;
		uint32_t insn;
		if (!tbit_ && predecode_ && !traceHook) {
			if (((pc_ ^ dVpage_) & ~0xfffu) != 0 && !lookupDecoded(pc_)) goto plain;
			// Straight-line run through this page. Leaves on a taken branch,
			// an abort, the end of the page, the slice budget, or anything
			// that forgets the current page (a TLB flush, a mode change),
			// raises an interrupt line or idles the core.
			DInsn *ins = dPage_->ins;
			for (;;) {
				uint32_t pc = pc_;
				uint32_t idx = (pc >> 2) & 1023;
				DInsn d = ins[idx];
				if (!d.op) {
					uint32_t raw;
					std::memcpy(&raw, dHost_ + (pc & 0xfff), 4);
					decodeArm(ins[idx], raw, pc);
					uint16_t &mask = codeMask_[dIndex_];
					if (!mask) {
						// First decoded code in this page: no TLB entry may keep a
						// direct write pointer to it any more. (The flush forgets
						// the current page; the loop looks it up again next time.)
						flushTlb();
					}
					mask |= (uint16_t)(1u << ((pc & 0xfff) >> 8));
					d = ins[idx];
				}
				runDone_++;
				if (d.cond == 0xE || condPassed(d.cond)) {
					branched_ = false;
					kHandlerTable[d.op](*this, d);
					if (branched_ | aborted_) {
						if (aborted_) {
							aborted_ = false;
							exception(0x10, ABT, pc_ + 8, false);
						}
						break;
					}
				}
				pc_ = pc + 4;
				if (idx == 1023 || runDone_ >= runLimit_ || dVpage_ == 1 ||
				    (irqLine_ | fiqLine_ | idle_))
					break;
			}
			continue;
		}
	plain:
		if (!tbit_) {
			if (!fetch32(pc_, insn)) { prefetchAbort(pc_, 0, 0); runDone_++; continue; }
			if (traceHook) traceHook(traceCtx, pc_, insn, false);
			execArm(insn);
			if (aborted_) {
				aborted_ = false;
				exception(0x10, ABT, pc_ + 8, false);
			} else if (!branched_) {
				pc_ += 4;
			}
		} else {
			if (!fetch16(pc_, insn)) { prefetchAbort(pc_, 0, 0); runDone_++; continue; }
			if (traceHook) traceHook(traceCtx, pc_, insn, true);
			execThumb(insn);
			if (aborted_) {
				aborted_ = false;
				exception(0x10, ABT, pc_ + 8, false);
			} else if (!branched_) {
				pc_ += 2;
			}
		}
		runDone_++;
	}
	uint32_t done = runDone_;
	insnCount_ += done;
	runDone_ = 0;
	return done;
}

void Cpu::writeRegAlu(int d, uint32_t v) {
	if (d == 15) branchTo(v & (tbit_ ? ~1u : ~3u));
	else r_[d] = v;
}

void Cpu::writeRegLoad(int d, uint32_t v) {
	if (d == 15) branchInterwork(v);
	else r_[d] = v;
}

// ── ARM state ───────────────────────────────────────────────────────────

void Cpu::execArm(uint32_t insn) {
	uint32_t cond = insn >> 28;
	if (cond != 0xE) {
		if (cond == 0xF) { armUncond(insn); return; }
		if (!condPassed(cond)) return;
	}
	execArmBody(insn);
}

void Cpu::execArmBody(uint32_t insn) {
	switch ((insn >> 25) & 7) {
	case 0:
		if ((insn & 0x90) == 0x90) {
			if ((insn & 0x60) == 0) {
				if (insn & (1u << 24)) {
					// SWP / SWPB
					if ((insn & 0x0FB00FF0) != 0x01000090) { undefined(); return; }
					int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15, rm = insn & 15;
					uint32_t addr = r_[rn];
					bool user = mode_ == USR;
					uint32_t old;
					if (insn & (1u << 22)) {
						if (!read8(addr, old, user)) return;
						if (!write8(addr, r_[rm], user)) return;
					} else {
						if (alignCheck() && (addr & 3)) { dataAbort(addr, 1, 0); return; }
						if (!read32(addr & ~3u, old, user)) return;
						old = ror32(old, (addr & 3) * 8);
						if (!write32(addr & ~3u, r_[rm], user)) return;
					}
					r_[rd] = old;
				} else {
					armMultiply(insn);
				}
			} else {
				armExtraLoadStore(insn);
			}
			return;
		}
		if ((insn & 0x01900000) == 0x01000000) { armMisc(insn); return; }
		armDataProc(insn);
		return;
	case 1:
		if ((insn & 0x01900000) == 0x01000000) {
			if (!(insn & (1u << 21))) { undefined(); return; }
			armMisc(insn);   // MSR immediate
			return;
		}
		armDataProc(insn);
		return;
	case 2:
		armSingleLoadStore(insn);
		return;
	case 3:
		if (insn & 0x10) { undefined(); return; }
		armSingleLoadStore(insn);
		return;
	case 4:
		armBlock(insn);
		return;
	case 5: {
		int32_t off = (int32_t)(insn << 8) >> 6;
		if (insn & (1u << 24)) r_[14] = pc_ + 4;
		branchTo(pc_ + 8 + (uint32_t)off);
		return;
	}
	case 6:
		armCoproc(insn);
		return;
	default:
		if (insn & (1u << 24)) {
			exception(0x08, SVC, pc_ + 4, false);
			return;
		}
		armCoproc(insn);
		return;
	}
}

void Cpu::armUncond(uint32_t insn) {
	if ((insn & 0x0E000000) == 0x0A000000) {
		// BLX <imm>
		int32_t off = (int32_t)(insn << 8) >> 6;
		off |= (insn >> 23) & 2;
		r_[14] = pc_ + 4;
		tbit_ = true;
		branchTo(pc_ + 8 + (uint32_t)off);
		return;
	}
	if ((insn & 0x0D70F000) == 0x0550F000) return;   // PLD
	undefined();
}

void Cpu::armDataProc(uint32_t insn) {
	uint32_t op = (insn >> 21) & 15;
	bool S = insn & (1u << 20);
	int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
	uint32_t b, shC = fC_;
	uint32_t a;
	if (insn & (1u << 25)) {
		uint32_t rot = (insn >> 7) & 0x1e;
		b = ror32(insn & 0xff, rot);
		if (rot) shC = b >> 31;
		a = (rn == 15) ? pc_ + 8 : r_[rn];
	} else {
		int rm = insn & 15;
		uint32_t type = (insn >> 5) & 3;
		if (insn & 0x10) {
			uint32_t amt = r_[(insn >> 8) & 15] & 0xff;
			uint32_t v = (rm == 15) ? pc_ + 12 : r_[rm];
			a = (rn == 15) ? pc_ + 12 : r_[rn];
			if (amt == 0) {
				b = v;
			} else {
				switch (type) {
				case 0:
					if (amt < 32) { b = v << amt; shC = (v >> (32 - amt)) & 1; }
					else if (amt == 32) { b = 0; shC = v & 1; }
					else { b = 0; shC = 0; }
					break;
				case 1:
					if (amt < 32) { b = v >> amt; shC = (v >> (amt - 1)) & 1; }
					else if (amt == 32) { b = 0; shC = v >> 31; }
					else { b = 0; shC = 0; }
					break;
				case 2:
					if (amt < 32) { b = (uint32_t)((int32_t)v >> amt); shC = (v >> (amt - 1)) & 1; }
					else { b = (uint32_t)((int32_t)v >> 31); shC = v >> 31; }
					break;
				default:
					if ((amt & 31) == 0) { b = v; shC = v >> 31; }
					else { b = ror32(v, amt & 31); shC = b >> 31; }
					break;
				}
			}
		} else {
			uint32_t amt = (insn >> 7) & 31;
			uint32_t v = (rm == 15) ? pc_ + 8 : r_[rm];
			a = (rn == 15) ? pc_ + 8 : r_[rn];
			switch (type) {
			case 0:
				if (amt == 0) b = v;
				else { b = v << amt; shC = (v >> (32 - amt)) & 1; }
				break;
			case 1:
				if (amt == 0) { b = 0; shC = v >> 31; }
				else { b = v >> amt; shC = (v >> (amt - 1)) & 1; }
				break;
			case 2:
				if (amt == 0) { b = (uint32_t)((int32_t)v >> 31); shC = v >> 31; }
				else { b = (uint32_t)((int32_t)v >> amt); shC = (v >> (amt - 1)) & 1; }
				break;
			default:
				if (amt == 0) { b = (fC_ << 31) | (v >> 1); shC = v & 1; }
				else { b = ror32(v, amt); shC = b >> 31; }
				break;
			}
		}
	}

	uint32_t res;
	bool logical = false, writes = true;
	uint32_t c = fC_, v = fV_;
	switch (op) {
	case 0x0: res = a & b; logical = true; break;                         // AND
	case 0x1: res = a ^ b; logical = true; break;                         // EOR
	case 0x2: res = a - b; c = a >= b; v = ((a ^ b) & (a ^ res)) >> 31; break;   // SUB
	case 0x3: res = b - a; c = b >= a; v = ((b ^ a) & (b ^ res)) >> 31; break;   // RSB
	case 0x4: res = a + b; c = res < a; v = (~(a ^ b) & (a ^ res)) >> 31; break; // ADD
	case 0x5: {                                                            // ADC
		uint64_t s = (uint64_t)a + b + fC_;
		res = (uint32_t)s; c = (uint32_t)(s >> 32); v = (~(a ^ b) & (a ^ res)) >> 31;
		break;
	}
	case 0x6: {                                                            // SBC
		uint32_t borrow = fC_ ^ 1;
		res = a - b - borrow;
		c = (uint64_t)a >= (uint64_t)b + borrow;
		v = ((a ^ b) & (a ^ res)) >> 31;
		break;
	}
	case 0x7: {                                                            // RSC
		uint32_t borrow = fC_ ^ 1;
		res = b - a - borrow;
		c = (uint64_t)b >= (uint64_t)a + borrow;
		v = ((b ^ a) & (b ^ res)) >> 31;
		break;
	}
	case 0x8: res = a & b; logical = true; writes = false; break;         // TST
	case 0x9: res = a ^ b; logical = true; writes = false; break;         // TEQ
	case 0xA: res = a - b; c = a >= b; v = ((a ^ b) & (a ^ res)) >> 31; writes = false; break; // CMP
	case 0xB: res = a + b; c = res < a; v = (~(a ^ b) & (a ^ res)) >> 31; writes = false; break; // CMN
	case 0xC: res = a | b; logical = true; break;                         // ORR
	case 0xD: res = b; logical = true; break;                             // MOV
	case 0xE: res = a & ~b; logical = true; break;                        // BIC
	default:  res = ~b; logical = true; break;                            // MVN
	}

	if (writes && rd == 15) {
		if (S) {
			restoreCpsrFromSpsr();
			branchTo(res & (tbit_ ? ~1u : ~3u));
		} else {
			branchTo(res & ~3u);
		}
		return;
	}
	if (S) {
		fN_ = res >> 31;
		fZ_ = res == 0;
		if (logical) fC_ = shC;
		else { fC_ = c; fV_ = v; }
	}
	if (writes) r_[rd] = res;
}

void Cpu::armMisc(uint32_t insn) {
	if (insn & (1u << 25)) {
		// MSR immediate
		uint32_t rot = (insn >> 7) & 0x1e;
		uint32_t val = ror32(insn & 0xff, rot);
		uint32_t mask = 0;
		if (insn & (1u << 16)) mask |= 0x000000ff;
		if (insn & (1u << 17)) mask |= 0x0000ff00;
		if (insn & (1u << 18)) mask |= 0x00ff0000;
		if (insn & (1u << 19)) mask |= 0xff000000;
		if (insn & (1u << 22)) {
			uint32_t *s = spsrPtr();
			if (s) *s = (*s & ~mask) | (val & mask);
		} else {
			if (!privileged()) mask &= 0xff000000;
			uint32_t nv = (cpsr() & ~mask) | (val & mask);
			nv = (nv & ~0x20u) | (tbit_ ? 0x20u : 0);
			setCpsr(nv);
		}
		return;
	}
	uint32_t op2 = (insn >> 4) & 15;
	uint32_t op = (insn >> 21) & 3;
	switch (op2) {
	case 0x0:
		if (op & 1) {
			// MSR register
			uint32_t val = r_[insn & 15];
			uint32_t mask = 0;
			if (insn & (1u << 16)) mask |= 0x000000ff;
			if (insn & (1u << 17)) mask |= 0x0000ff00;
			if (insn & (1u << 18)) mask |= 0x00ff0000;
			if (insn & (1u << 19)) mask |= 0xff000000;
			if (insn & (1u << 22)) {
				uint32_t *s = spsrPtr();
				if (s) *s = (*s & ~mask) | (val & mask);
			} else {
				if (!privileged()) mask &= 0xff000000;
				uint32_t nv = (cpsr() & ~mask) | (val & mask);
				nv = (nv & ~0x20u) | (tbit_ ? 0x20u : 0);
				setCpsr(nv);
			}
		} else {
			// MRS
			int rd = (insn >> 12) & 15;
			uint32_t v;
			if (insn & (1u << 22)) { uint32_t *s = spsrPtr(); v = s ? *s : cpsr(); }
			else v = cpsr();
			if (rd != 15) r_[rd] = v;
		}
		return;
	case 0x1:
		if (op == 1) { branchInterwork(r_[insn & 15]); return; }          // BX
		if (op == 3) {                                                     // CLZ
			uint32_t m = r_[insn & 15];
			r_[(insn >> 12) & 15] = m ? (uint32_t)__builtin_clz(m) : 32;
			return;
		}
		break;
	case 0x3:
		if (op == 1) {                                                     // BLX register
			uint32_t t = r_[insn & 15];
			r_[14] = pc_ + 4;
			branchInterwork(t);
			return;
		}
		break;
	case 0x5: {                                                            // Q-ops
		int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15, rm = insn & 15;
		int32_t m = (int32_t)r_[rm], nv = (int32_t)r_[rn];
		int32_t res;
		switch (op) {
		case 0: res = satAdd((int64_t)m + nv, fQ_); break;
		case 1: res = satAdd((int64_t)m - nv, fQ_); break;
		case 2: res = satAdd((int64_t)m + satAdd((int64_t)nv * 2, fQ_), fQ_); break;
		default: res = satAdd((int64_t)m - satAdd((int64_t)nv * 2, fQ_), fQ_); break;
		}
		r_[rd] = (uint32_t)res;
		return;
	}
	case 0x7:
		if (op == 1) {                                                     // BKPT
			exception(0x0C, ABT, pc_ + 4, false);
			return;
		}
		break;
	case 0x8: case 0xA: case 0xC: case 0xE: {                              // signed multiplies
		int rd = (insn >> 16) & 15, rn = (insn >> 12) & 15;
		int rs = (insn >> 8) & 15, rm = insn & 15;
		bool x = insn & 0x20, y = insn & 0x40;
		int32_t mHalf = x ? sext16(r_[rm] >> 16) : sext16(r_[rm]);
		int32_t sHalf = y ? sext16(r_[rs] >> 16) : sext16(r_[rs]);
		switch (op) {
		case 0: {                                                          // SMLAxy
			// Wraps rather than saturating; overflow of the add only sets Q.
			int32_t p = mHalf * sHalf;
			uint32_t ur = (uint32_t)p + r_[rn];
			int64_t full = (int64_t)p + (int64_t)(int32_t)r_[rn];
			if (full != (int32_t)ur) fQ_ = 1;
			r_[rd] = ur;
			return;
		}
		case 1: {
			int64_t prod = ((int64_t)(int32_t)r_[rm] * sHalf) >> 16;
			uint32_t p32 = (uint32_t)prod;
			if (x) {                                                       // SMULWy
				r_[rd] = p32;
			} else {                                                       // SMLAWy
				uint32_t ur = p32 + r_[rn];
				int64_t full = (int64_t)(int32_t)p32 + (int64_t)(int32_t)r_[rn];
				if (full != (int32_t)ur) fQ_ = 1;
				r_[rd] = ur;
			}
			return;
		}
		case 2: {                                                          // SMLALxy
			int64_t acc = (int64_t)(((uint64_t)r_[rd] << 32) | r_[rn]);
			acc += (int64_t)mHalf * sHalf;
			r_[rn] = (uint32_t)acc;
			r_[rd] = (uint32_t)((uint64_t)acc >> 32);
			return;
		}
		default:                                                           // SMULxy
			r_[rd] = (uint32_t)(mHalf * sHalf);
			return;
		}
	}
	default:
		break;
	}
	undefined();
}

void Cpu::armMultiply(uint32_t insn) {
	uint32_t op = (insn >> 21) & 7;
	bool S = insn & (1u << 20);
	int rd = (insn >> 16) & 15, rn = (insn >> 12) & 15, rs = (insn >> 8) & 15, rm = insn & 15;
	switch (op) {
	case 0:
	case 1: {
		uint32_t res = r_[rm] * r_[rs];
		if (op == 1) res += r_[rn];
		r_[rd] = res;
		if (S) setNZ(res);
		return;
	}
	case 4: case 5: case 6: case 7: {
		uint64_t res;
		if (op & 2) res = (uint64_t)((int64_t)(int32_t)r_[rm] * (int64_t)(int32_t)r_[rs]);
		else res = (uint64_t)r_[rm] * r_[rs];
		if (op & 1) res += ((uint64_t)r_[rd] << 32) | r_[rn];
		r_[rn] = (uint32_t)res;
		r_[rd] = (uint32_t)(res >> 32);
		if (S) { fN_ = (uint32_t)(res >> 63); fZ_ = res == 0; }
		return;
	}
	default:
		undefined();
		return;
	}
}

void Cpu::armExtraLoadStore(uint32_t insn) {
	bool P = insn & (1u << 24), U = insn & (1u << 23), I = insn & (1u << 22);
	bool W = insn & (1u << 21), L = insn & (1u << 20);
	int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
	uint32_t sh = (insn >> 5) & 3;
	uint32_t off = I ? (((insn >> 4) & 0xf0) | (insn & 0xf)) : r_[insn & 15];
	uint32_t base = (rn == 15) ? pc_ + 8 : r_[rn];
	uint32_t moved = U ? base + off : base - off;
	uint32_t addr = P ? moved : base;
	bool wb = !P || W;
	bool user = mode_ == USR;
	bool A = alignCheck();

	if (!L && (sh & 2)) {
		// LDRD (sh=2) / STRD (sh=3)
		if (rd & 1) { undefined(); return; }
		if ((addr & 7) && A) { dataAbort(addr, 1, 0); return; }
		addr &= ~3u;
		if (sh == 2) {
			uint32_t lo, hi;
			if (!read32(addr, lo, user)) return;
			if (!read32(addr + 4, hi, user)) return;
			if (wb) r_[rn] = moved;
			r_[rd] = lo;
			writeRegLoad(rd + 1, hi);
		} else {
			uint32_t lo = r_[rd];
			uint32_t hi = (rd + 1 == 15) ? pc_ + 8 : r_[rd + 1];
			if (!write32(addr, lo, user)) return;
			if (!write32(addr + 4, hi, user)) return;
			if (wb) r_[rn] = moved;
		}
		return;
	}
	if (L) {
		uint32_t val;
		if (sh == 1) {                       // LDRH
			if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
			if (!read16(addr & ~1u, val, user)) return;
		} else if (sh == 2) {                // LDRSB
			if (!read8(addr, val, user)) return;
			val = (uint32_t)(int32_t)(int8_t)val;
		} else {                             // LDRSH
			if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
			if (!read16(addr & ~1u, val, user)) return;
			val = (uint32_t)(int32_t)(int16_t)val;
		}
		if (wb) r_[rn] = moved;
		writeRegLoad(rd, val);
	} else {
		// STRH (sh == 1)
		if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
		uint32_t val = (rd == 15) ? pc_ + 8 : r_[rd];
		if (!write16(addr & ~1u, val, user)) return;
		if (wb) r_[rn] = moved;
	}
}

void Cpu::armSingleLoadStore(uint32_t insn) {
	bool I = insn & (1u << 25), P = insn & (1u << 24), U = insn & (1u << 23);
	bool B = insn & (1u << 22), W = insn & (1u << 21), L = insn & (1u << 20);
	int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
	uint32_t off;
	if (!I) {
		off = insn & 0xfff;
	} else {
		uint32_t v = (insn & 15) == 15 ? pc_ + 8 : r_[insn & 15];
		uint32_t amt = (insn >> 7) & 31;
		switch ((insn >> 5) & 3) {
		case 0: off = v << amt; break;
		case 1: off = amt ? v >> amt : 0; break;
		case 2: off = (uint32_t)((int32_t)v >> (amt ? amt : 31)); break;
		default: off = amt ? ror32(v, amt) : ((fC_ << 31) | (v >> 1)); break;
		}
	}
	uint32_t base = (rn == 15) ? pc_ + 8 : r_[rn];
	uint32_t moved = U ? base + off : base - off;
	uint32_t addr = P ? moved : base;
	bool wb = !P || W;
	bool user = (mode_ == USR) || (!P && W);    // LDRT/STRT
	if (L) {
		uint32_t val;
		if (B) {
			if (!read8(addr, val, user)) return;
		} else {
			if (addr & 3) {
				if (alignCheck()) { dataAbort(addr, 1, 0); return; }
				if (!read32(addr & ~3u, val, user)) return;
				val = ror32(val, (addr & 3) * 8);
			} else {
				if (!read32(addr, val, user)) return;
			}
		}
		if (wb) r_[rn] = moved;
		writeRegLoad(rd, val);
	} else {
		uint32_t val = (rd == 15) ? pc_ + 8 : r_[rd];
		if (B) {
			if (!write8(addr, val, user)) return;
		} else {
			if ((addr & 3) && alignCheck()) { dataAbort(addr, 1, 0); return; }
			if (!write32(addr & ~3u, val, user)) return;
		}
		if (wb) r_[rn] = moved;
	}
}

void Cpu::armBlock(uint32_t insn) {
	bool P = insn & (1u << 24), U = insn & (1u << 23), S = insn & (1u << 22);
	bool W = insn & (1u << 21), L = insn & (1u << 20);
	int rn = (insn >> 16) & 15;
	uint32_t list = insn & 0xffff;
	if (!list) return;
	uint32_t n = (uint32_t)__builtin_popcount(list);
	uint32_t base = r_[rn];
	uint32_t start;
	if (U) start = P ? base + 4 : base;
	else   start = P ? base - 4 * n : base - 4 * n + 4;
	uint32_t newBase = U ? base + 4 * n : base - 4 * n;
	uint32_t addr = start & ~3u;
	bool user = mode_ == USR;
	bool pcInList = list & 0x8000;
	// "User bank" transfer: S set, and for LDM the PC not in the list.
	bool userBank = S && !(L && pcInList) && mode_ != USR && mode_ != SYS;

	if (L) {
		uint32_t vals[16];
		uint32_t a = addr;
		for (int i = 0; i < 16; i++) {
			if (!(list & (1u << i))) continue;
			if (!read32(a, vals[i], user)) return;
			a += 4;
		}
		if (W) r_[rn] = newBase;
		for (int i = 0; i < 15; i++) {
			if (!(list & (1u << i))) continue;
			if (userBank && i >= 8) {
				if (mode_ == FIQ) usrR8_14_[i - 8] = vals[i];
				else if (i >= 13) usrR8_14_[i - 8] = vals[i];
				else r_[i] = vals[i];
			} else {
				r_[i] = vals[i];
			}
		}
		if (pcInList) {
			if (S) {
				restoreCpsrFromSpsr();
				branchTo(vals[15] & (tbit_ ? ~1u : ~3u));
			} else {
				branchInterwork(vals[15]);
			}
		}
	} else {
		uint32_t a = addr;
		for (int i = 0; i < 16; i++) {
			if (!(list & (1u << i))) continue;
			uint32_t v;
			if (i == 15) v = pc_ + 8;
			else if (userBank && i >= 8) {
				if (mode_ == FIQ) v = usrR8_14_[i - 8];
				else if (i >= 13) v = usrR8_14_[i - 8];
				else v = r_[i];
			} else v = r_[i];
			if (!write32(a, v, user)) return;
			a += 4;
		}
		if (W) r_[rn] = newBase;
	}
}

void Cpu::armCoproc(uint32_t insn) {
	int cp = (insn >> 8) & 15;
	if ((insn & 0x0E000000) == 0x0C000000) {
		// LDC/STC, MCRR/MRRC
		if ((insn & 0x0FE00000) == 0x0C400000 && cp == 0 && (cpar_ & 1)) {
			int rdhi = (insn >> 16) & 15, rdlo = (insn >> 12) & 15;
			if ((insn & 0xff) != 0) { undefined(); return; }        // acc0 only
			if (insn & (1u << 20)) {                                  // MRA
				r_[rdlo] = (uint32_t)acc0_;
				r_[rdhi] = (uint32_t)(int32_t)(int8_t)((uint64_t)acc0_ >> 32);
			} else {                                                  // MAR
				acc0_ = sext40((int64_t)(((uint64_t)(r_[rdhi] & 0xff) << 32) | r_[rdlo]));
			}
			return;
		}
		undefined();
		return;
	}
	// CDP / MCR / MRC
	if (!(insn & 0x10)) { undefined(); return; }
	bool load = insn & (1u << 20);
	int opc1 = (insn >> 21) & 7, crn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
	int opc2 = (insn >> 5) & 7, crm = insn & 15;
	if (cp == 0) {
		if (!(cpar_ & 1) || load || opc1 != 1 || opc2 != 0) { undefined(); return; }
		// MIA family: opcode_3 in CRn, Rs in Rd field, Rm in CRm.
		uint32_t rs = r_[rd], rm = r_[crm];
		int64_t add;
		switch (crn) {
		case 0x0: add = (int64_t)(int32_t)rm * (int32_t)rs; break;                       // MIA
		case 0x8: add = (int64_t)(sext16(rm) * sext16(rs)) +
		                (int64_t)(sext16(rm >> 16) * sext16(rs >> 16)); break;          // MIAPH
		case 0xC: add = (int64_t)sext16(rm) * sext16(rs); break;                         // MIABB
		case 0xD: add = (int64_t)sext16(rm) * sext16(rs >> 16); break;                   // MIABT
		case 0xE: add = (int64_t)sext16(rm >> 16) * sext16(rs); break;                   // MIATB
		case 0xF: add = (int64_t)sext16(rm >> 16) * sext16(rs >> 16); break;             // MIATT
		default: undefined(); return;
		}
		acc0_ = sext40(acc0_ + add);
		return;
	}
	if ((cp == 14 || cp == 15) && !privileged()) { undefined(); return; }
	if (load) {
		uint32_t v = 0;
		if (!mrc(cp, opc1, crn, crm, opc2, v)) { undefined(); return; }
		if (rd == 15) { fN_ = v >> 31; fZ_ = (v >> 30) & 1; fC_ = (v >> 29) & 1; fV_ = (v >> 28) & 1; }
		else r_[rd] = v;
	} else {
		uint32_t v = (rd == 15) ? pc_ + 8 : r_[rd];
		if (!mcr(cp, opc1, crn, crm, opc2, v)) { undefined(); return; }
	}
}

// ── Coprocessors ────────────────────────────────────────────────────────

bool Cpu::mrc(int cp, int opc1, int crn, int crm, int opc2, uint32_t &v) {
	if (opc1 != 0) return false;
	if (cp == 15) {
		switch (crn) {
		case 0: v = (opc2 == 1) ? kCacheType : kCpuId; return true;
		case 1: v = (opc2 == 1) ? cp15Aux_ : cp15Control_; return true;
		case 2: v = ttb_; return true;
		case 3: v = dacr_; return true;
		case 5: v = fsr_; return true;
		case 6: v = far_; return true;
		case 7: case 8: case 9: case 10: v = 0; return true;
		case 13: v = pid_; return true;
		case 14: v = cp15Dbg_[crm & 3]; return true;
		case 15: v = (crm == 1) ? cpar_ : 0; return true;
		default: v = 0; return true;
		}
	}
	if (cp == 14) {
		switch (crn) {
		case 0: v = pmnc_ | (flag_ << 8); return true;
		case 1: v = (uint32_t)((insnCount_ - ccntBase_) & 0xffffffffu); return true;
		case 2: v = pmn0_; return true;
		case 3: v = pmn1_; return true;
		case 6: v = cclkcfg_; return true;
		case 7: v = pwrmode_; return true;
		default: v = 0; return true;
		}
	}
	return false;
}

bool Cpu::mcr(int cp, int opc1, int crn, int crm, int opc2, uint32_t v) {
	if (opc1 != 0) return false;
	if (cp == 15) {
		switch (crn) {
		case 0: return true;
		case 1:
			if (opc2 == 1) { cp15Aux_ = v; return true; }
			if ((v ^ cp15Control_) & 0x2301) flushTlb();   // M, S, R, V
			cp15Control_ = v | 0x70;
			fetchTag_ = 0;
			return true;
		case 2: ttb_ = v; flushTlb(); return true;
		case 3: dacr_ = v; flushTlb(); return true;
		case 5: fsr_ = v; return true;
		case 6: far_ = v; return true;
		case 7: return true;                               // cache maintenance
		case 8:
			if (opc2 == 1) {                               // invalidate single entry
				uint32_t va = v;
				XS_MVA(va);
				uint32_t idx = (va >> 12) & (TlbSize - 1);
				if ((tlbPriv_[idx].tag & ~0xfffu) == (va & ~0xfffu)) tlbPriv_[idx].tag = 0;
				if ((tlbUser_[idx].tag & ~0xfffu) == (va & ~0xfffu)) tlbUser_[idx].tag = 0;
				fetchTag_ = 0;
				dVpage_ = 1;
			} else {
				flushTlb();
			}
			return true;
		case 9: case 10: return true;                      // lockdown
		case 13: pid_ = v & 0xfe000000u; flushTlb(); return true;
		case 14: cp15Dbg_[crm & 3] = v; return true;
		case 15: if (crm == 1) cpar_ = v & 0x3fff; return true;
		default: return true;
		}
	}
	if (cp == 14) {
		switch (crn) {
		case 0:
			pmnc_ = v & 0x0ff0007f & ~0x6u;
			flag_ &= ~((v >> 8) & 7);
			if (v & 4) ccntBase_ = insnCount_;
			return true;
		case 1: ccntBase_ = insnCount_ - v; return true;
		case 2: pmn0_ = v; return true;
		case 3: pmn1_ = v; return true;
		case 6: cclkcfg_ = v; bus_.onClockConfig(v); return true;
		case 7:
			pwrmode_ = v & 3;
			if ((v & 3) == 1) {
				if (!irqLine_ && !fiqLine_) idle_ = true;
			} else if ((v & 3) != 0) {
				bus_.onPowerMode(v);
			}
			return true;
		default: return true;
		}
	}
	return false;
}

// ── Thumb state ─────────────────────────────────────────────────────────

void Cpu::execThumb(uint32_t insn) {
	switch (insn >> 11) {
	case 0x00: case 0x01: case 0x02: {           // shift by immediate
		uint32_t op = (insn >> 11) & 3;
		uint32_t amt = (insn >> 6) & 31;
		int rs = (insn >> 3) & 7, rd = insn & 7;
		uint32_t v = r_[rs], res;
		switch (op) {
		case 0:
			if (amt == 0) res = v;
			else { res = v << amt; fC_ = (v >> (32 - amt)) & 1; }
			break;
		case 1:
			if (amt == 0) { res = 0; fC_ = v >> 31; }
			else { res = v >> amt; fC_ = (v >> (amt - 1)) & 1; }
			break;
		default:
			if (amt == 0) { res = (uint32_t)((int32_t)v >> 31); fC_ = v >> 31; }
			else { res = (uint32_t)((int32_t)v >> amt); fC_ = (v >> (amt - 1)) & 1; }
			break;
		}
		r_[rd] = res;
		setNZ(res);
		return;
	}
	case 0x03: {                                 // add/sub register or imm3
		int rd = insn & 7, rs = (insn >> 3) & 7;
		uint32_t a = r_[rs];
		uint32_t b = (insn & 0x400) ? ((insn >> 6) & 7) : r_[(insn >> 6) & 7];
		uint32_t res;
		if (insn & 0x200) { res = a - b; fC_ = a >= b; fV_ = ((a ^ b) & (a ^ res)) >> 31; }
		else { res = a + b; fC_ = res < a; fV_ = (~(a ^ b) & (a ^ res)) >> 31; }
		r_[rd] = res;
		setNZ(res);
		return;
	}
	case 0x04: case 0x05: case 0x06: case 0x07: { // mov/cmp/add/sub imm8
		int rd = (insn >> 8) & 7;
		uint32_t imm = insn & 0xff;
		uint32_t a = r_[rd], res;
		switch ((insn >> 11) & 3) {
		case 0: res = imm; r_[rd] = res; setNZ(res); return;
		case 1: res = a - imm; fC_ = a >= imm; fV_ = ((a ^ imm) & (a ^ res)) >> 31; setNZ(res); return;
		case 2: res = a + imm; fC_ = res < a; fV_ = (~(a ^ imm) & (a ^ res)) >> 31; r_[rd] = res; setNZ(res); return;
		default: res = a - imm; fC_ = a >= imm; fV_ = ((a ^ imm) & (a ^ res)) >> 31; r_[rd] = res; setNZ(res); return;
		}
	}
	case 0x08:
		if (!(insn & 0x400)) {                   // ALU operations
			int rd = insn & 7, rs = (insn >> 3) & 7;
			uint32_t a = r_[rd], b = r_[rs], res;
			switch ((insn >> 6) & 15) {
			case 0x0: res = a & b; r_[rd] = res; setNZ(res); return;
			case 0x1: res = a ^ b; r_[rd] = res; setNZ(res); return;
			case 0x2: {
				uint32_t amt = b & 0xff;
				if (amt == 0) res = a;
				else if (amt < 32) { res = a << amt; fC_ = (a >> (32 - amt)) & 1; }
				else if (amt == 32) { res = 0; fC_ = a & 1; }
				else { res = 0; fC_ = 0; }
				r_[rd] = res; setNZ(res); return;
			}
			case 0x3: {
				uint32_t amt = b & 0xff;
				if (amt == 0) res = a;
				else if (amt < 32) { res = a >> amt; fC_ = (a >> (amt - 1)) & 1; }
				else if (amt == 32) { res = 0; fC_ = a >> 31; }
				else { res = 0; fC_ = 0; }
				r_[rd] = res; setNZ(res); return;
			}
			case 0x4: {
				uint32_t amt = b & 0xff;
				if (amt == 0) res = a;
				else if (amt < 32) { res = (uint32_t)((int32_t)a >> amt); fC_ = (a >> (amt - 1)) & 1; }
				else { res = (uint32_t)((int32_t)a >> 31); fC_ = a >> 31; }
				r_[rd] = res; setNZ(res); return;
			}
			case 0x5: {
				uint64_t s = (uint64_t)a + b + fC_;
				res = (uint32_t)s; fC_ = (uint32_t)(s >> 32); fV_ = (~(a ^ b) & (a ^ res)) >> 31;
				r_[rd] = res; setNZ(res); return;
			}
			case 0x6: {
				uint32_t borrow = fC_ ^ 1;
				res = a - b - borrow;
				fC_ = (uint64_t)a >= (uint64_t)b + borrow;
				fV_ = ((a ^ b) & (a ^ res)) >> 31;
				r_[rd] = res; setNZ(res); return;
			}
			case 0x7: {
				uint32_t amt = b & 0xff;
				if (amt == 0) res = a;
				else if ((amt & 31) == 0) { res = a; fC_ = a >> 31; }
				else { res = ror32(a, amt & 31); fC_ = res >> 31; }
				r_[rd] = res; setNZ(res); return;
			}
			case 0x8: res = a & b; setNZ(res); return;
			case 0x9: res = 0 - b; fC_ = b == 0; fV_ = (b & res) >> 31; r_[rd] = res; setNZ(res); return;
			case 0xA: res = a - b; fC_ = a >= b; fV_ = ((a ^ b) & (a ^ res)) >> 31; setNZ(res); return;
			case 0xB: res = a + b; fC_ = res < a; fV_ = (~(a ^ b) & (a ^ res)) >> 31; setNZ(res); return;
			case 0xC: res = a | b; r_[rd] = res; setNZ(res); return;
			case 0xD: res = a * b; r_[rd] = res; setNZ(res); return;
			case 0xE: res = a & ~b; r_[rd] = res; setNZ(res); return;
			default:  res = ~b; r_[rd] = res; setNZ(res); return;
			}
		} else {                                 // hi register ops / BX / BLX
			int rd = (insn & 7) | ((insn >> 4) & 8);
			int rm = (insn >> 3) & 15;
			uint32_t mv = (rm == 15) ? pc_ + 4 : r_[rm];
			switch ((insn >> 8) & 3) {
			case 0: {
				uint32_t dv = (rd == 15) ? pc_ + 4 : r_[rd];
				uint32_t res = dv + mv;
				if (rd == 15) branchTo(res & ~1u); else r_[rd] = res;
				return;
			}
			case 1: {
				uint32_t dv = (rd == 15) ? pc_ + 4 : r_[rd];
				uint32_t res = dv - mv;
				fC_ = dv >= mv; fV_ = ((dv ^ mv) & (dv ^ res)) >> 31; setNZ(res);
				return;
			}
			case 2:
				if (rd == 15) branchTo(mv & ~1u); else r_[rd] = mv;
				return;
			default:
				if (insn & 0x80) r_[14] = (pc_ + 2) | 1;   // BLX
				branchInterwork(mv);
				return;
			}
		}
	case 0x09: {                                 // LDR pc-relative
		uint32_t addr = ((pc_ + 4) & ~3u) + ((insn & 0xff) << 2);
		uint32_t v;
		if (!read32(addr, v, mode_ == USR)) return;
		r_[(insn >> 8) & 7] = v;
		return;
	}
	case 0x0A: case 0x0B: {                      // load/store register offset
		int rd = insn & 7;
		uint32_t addr = r_[(insn >> 3) & 7] + r_[(insn >> 6) & 7];
		bool user = mode_ == USR, A = alignCheck();
		uint32_t v;
		switch ((insn >> 9) & 7) {
		case 0:                                  // STR
			if ((addr & 3) && A) { dataAbort(addr, 1, 0); return; }
			write32(addr & ~3u, r_[rd], user); return;
		case 1:                                  // STRH
			if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
			write16(addr & ~1u, r_[rd], user); return;
		case 2:                                  // STRB
			write8(addr, r_[rd], user); return;
		case 3:                                  // LDRSB
			if (!read8(addr, v, user)) return;
			r_[rd] = (uint32_t)(int32_t)(int8_t)v; return;
		case 4:                                  // LDR
			if (addr & 3) {
				if (A) { dataAbort(addr, 1, 0); return; }
				if (!read32(addr & ~3u, v, user)) return;
				v = ror32(v, (addr & 3) * 8);
			} else if (!read32(addr, v, user)) return;
			r_[rd] = v; return;
		case 5:                                  // LDRH
			if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
			if (!read16(addr & ~1u, v, user)) return;
			r_[rd] = v; return;
		case 6:                                  // LDRB
			if (!read8(addr, v, user)) return;
			r_[rd] = v; return;
		default:                                 // LDRSH
			if ((addr & 1) && A) { dataAbort(addr, 1, 0); return; }
			if (!read16(addr & ~1u, v, user)) return;
			r_[rd] = (uint32_t)(int32_t)(int16_t)v; return;
		}
	}
	case 0x0C: case 0x0D: case 0x0E: case 0x0F: { // load/store imm5 word/byte
		int rd = insn & 7;
		bool byte = insn & 0x1000, load = insn & 0x800;
		uint32_t imm = (insn >> 6) & 31;
		uint32_t addr = r_[(insn >> 3) & 7] + (byte ? imm : imm << 2);
		bool user = mode_ == USR;
		uint32_t v;
		if (byte) {
			if (load) { if (!read8(addr, v, user)) return; r_[rd] = v; }
			else write8(addr, r_[rd], user);
		} else {
			if ((addr & 3) && alignCheck()) { dataAbort(addr, 1, 0); return; }
			if (load) {
				if (!read32(addr & ~3u, v, user)) return;
				r_[rd] = ror32(v, (addr & 3) * 8);
			} else {
				write32(addr & ~3u, r_[rd], user);
			}
		}
		return;
	}
	case 0x10: case 0x11: {                      // LDRH/STRH imm5
		int rd = insn & 7;
		uint32_t addr = r_[(insn >> 3) & 7] + (((insn >> 6) & 31) << 1);
		bool user = mode_ == USR;
		if ((addr & 1) && alignCheck()) { dataAbort(addr, 1, 0); return; }
		if (insn & 0x800) {
			uint32_t v;
			if (!read16(addr & ~1u, v, user)) return;
			r_[rd] = v;
		} else {
			write16(addr & ~1u, r_[rd], user);
		}
		return;
	}
	case 0x12: case 0x13: {                      // SP-relative load/store
		int rd = (insn >> 8) & 7;
		uint32_t addr = r_[13] + ((insn & 0xff) << 2);
		bool user = mode_ == USR;
		if ((addr & 3) && alignCheck()) { dataAbort(addr, 1, 0); return; }
		if (insn & 0x800) {
			uint32_t v;
			if (!read32(addr & ~3u, v, user)) return;
			r_[rd] = ror32(v, (addr & 3) * 8);
		} else {
			write32(addr & ~3u, r_[rd], user);
		}
		return;
	}
	case 0x14: case 0x15: {                      // ADD rd, pc/sp, imm8
		int rd = (insn >> 8) & 7;
		uint32_t imm = (insn & 0xff) << 2;
		r_[rd] = (insn & 0x800) ? r_[13] + imm : ((pc_ + 4) & ~3u) + imm;
		return;
	}
	case 0x16: case 0x17: {                      // misc
		uint32_t sub = (insn >> 8) & 15;
		if (sub == 0) {                          // ADD/SUB sp, imm7
			uint32_t imm = (insn & 0x7f) << 2;
			if (insn & 0x80) r_[13] -= imm; else r_[13] += imm;
			return;
		}
		if ((sub & 6) == 4) {                    // PUSH / POP
			bool load = insn & 0x800, R = insn & 0x100;
			uint32_t list = insn & 0xff;
			uint32_t n = (uint32_t)__builtin_popcount(list) + (R ? 1 : 0);
			bool user = mode_ == USR;
			if (!load) {
				uint32_t addr = (r_[13] - 4 * n) & ~3u;
				uint32_t a = addr;
				for (int i = 0; i < 8; i++) {
					if (!(list & (1u << i))) continue;
					if (!write32(a, r_[i], user)) return;
					a += 4;
				}
				if (R && !write32(a, r_[14], user)) return;
				r_[13] -= 4 * n;
			} else {
				uint32_t a = r_[13] & ~3u;
				uint32_t vals[9];
				for (int i = 0; i < 8; i++) {
					if (!(list & (1u << i))) continue;
					if (!read32(a, vals[i], user)) return;
					a += 4;
				}
				if (R && !read32(a, vals[8], user)) return;
				r_[13] += 4 * n;
				for (int i = 0; i < 8; i++) if (list & (1u << i)) r_[i] = vals[i];
				if (R) branchInterwork(vals[8]);
			}
			return;
		}
		if (sub == 0xE) {                        // BKPT
			exception(0x0C, ABT, pc_ + 4, false);
			return;
		}
		undefined();
		return;
	}
	case 0x18: case 0x19: {                      // STMIA / LDMIA
		int rb = (insn >> 8) & 7;
		uint32_t list = insn & 0xff;
		if (!list) return;
		uint32_t n = (uint32_t)__builtin_popcount(list);
		uint32_t a = r_[rb] & ~3u;
		bool user = mode_ == USR;
		if (insn & 0x800) {
			uint32_t vals[8];
			for (int i = 0; i < 8; i++) {
				if (!(list & (1u << i))) continue;
				if (!read32(a, vals[i], user)) return;
				a += 4;
			}
			r_[rb] += 4 * n;
			for (int i = 0; i < 8; i++) if (list & (1u << i)) r_[i] = vals[i];
		} else {
			for (int i = 0; i < 8; i++) {
				if (!(list & (1u << i))) continue;
				if (!write32(a, r_[i], user)) return;
				a += 4;
			}
			r_[rb] += 4 * n;
		}
		return;
	}
	case 0x1A: case 0x1B: {                      // conditional branch / SWI
		uint32_t cond = (insn >> 8) & 15;
		if (cond == 0xF) { exception(0x08, SVC, pc_ + 2, false); return; }
		if (cond == 0xE) { undefined(); return; }
		if (!condPassed(cond)) return;
		int32_t off = (int32_t)(int8_t)(insn & 0xff) * 2;
		branchTo(pc_ + 4 + (uint32_t)off);
		return;
	}
	case 0x1C: {                                 // unconditional branch
		int32_t off = ((int32_t)(insn << 21) >> 20);
		branchTo(pc_ + 4 + (uint32_t)off);
		return;
	}
	case 0x1D: {                                 // BLX suffix
		if (insn & 1) { undefined(); return; }
		uint32_t target = (r_[14] + ((insn & 0x7ff) << 1)) & ~3u;
		r_[14] = (pc_ + 2) | 1;
		tbit_ = false;
		branchTo(target);
		return;
	}
	case 0x1E: {                                 // BL/BLX prefix
		int32_t off = (int32_t)(insn << 21) >> 9;
		r_[14] = pc_ + 4 + (uint32_t)off;
		return;
	}
	default: {                                   // BL suffix
		uint32_t target = r_[14] + ((insn & 0x7ff) << 1);
		r_[14] = (pc_ + 2) | 1;
		branchTo(target & ~1u);
		return;
	}
	}
}

} // namespace XScale
