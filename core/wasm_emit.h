// Minimal WebAssembly module writer for the ARM block compiler.
//
// Enough of the binary format to emit one module containing one function that
// imports the emulator's linear memory and operates on the register file in
// place. Deliberately not a general-purpose assembler: no tables, no globals,
// no multi-value, one type per signature. See docs/jit-engine-scope.md.
//
// Everything here is host-side byte assembly, so it builds and is testable in
// the native harness even though the module it produces only runs in the WASM
// build — which is the point: codegen bugs are found natively, where the
// interpreter is available as an oracle, not in a browser.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace wasmemit {

// ── Opcodes (only the ones the block compiler emits) ────────────────────────
enum Op : uint8_t {
	OP_BLOCK      = 0x02,
	OP_LOOP       = 0x03,
	OP_IF         = 0x04,
	OP_ELSE       = 0x05,
	OP_END        = 0x0B,
	OP_BR         = 0x0C,
	OP_BR_IF      = 0x0D,
	OP_RETURN     = 0x0F,
	OP_CALL       = 0x10,
	OP_CALL_IND   = 0x11,
	OP_DROP       = 0x1A,
	OP_SELECT     = 0x1B,
	OP_LOCAL_GET  = 0x20,
	OP_LOCAL_SET  = 0x21,
	OP_LOCAL_TEE  = 0x22,
	OP_I32_LOAD   = 0x28,
	OP_I64_LOAD   = 0x29,
	OP_I32_LOAD8U = 0x2D,
	OP_I32_STORE  = 0x36,
	OP_I32_STORE8 = 0x3A,
	OP_I64_STORE  = 0x37,
	OP_I32_CONST  = 0x41,
	OP_I32_EQZ    = 0x45,
	OP_I32_EQ     = 0x46,
	OP_I32_NE     = 0x47,
	OP_I32_LTU    = 0x49,
	OP_I32_GTU    = 0x4B,
	OP_I32_LEU    = 0x4D,
	OP_I32_GEU    = 0x4F,
	OP_I32_ADD    = 0x6A,
	OP_I32_SUB    = 0x6B,
	OP_I32_MUL    = 0x6C,
	OP_I32_AND    = 0x71,
	OP_I32_OR     = 0x72,
	OP_I32_XOR    = 0x73,
	OP_I32_SHL    = 0x74,
	OP_I32_SHRS   = 0x75,
	OP_I32_SHRU   = 0x76,
	OP_I32_ROTL   = 0x77,
	OP_I32_ROTR   = 0x78,
	OP_I64_ADD    = 0x7C,
	OP_I64_EXT_U  = 0xAD,   // i64.extend_i32_u
};

enum ValType : uint8_t { VT_I64 = 0x7E, VT_I32 = 0x7F, VT_VOID = 0x40 };

// ── LEB128 ─────────────────────────────────────────────────────────────────
inline void u32leb(std::vector<uint8_t> &out, uint32_t v) {
	do {
		uint8_t b = v & 0x7F;
		v >>= 7;
		if (v) b |= 0x80;
		out.push_back(b);
	} while (v);
}

// Signed LEB128. i32.const takes a SIGNED immediate, so a constant with the top
// bit set (0x80000000, and every negative mask the flag code uses) has to be
// written as the negative number it denotes — writing it unsigned produces a
// value the validator rejects as out of range for i32.
inline void i32leb(std::vector<uint8_t> &out, int32_t v) {
	bool more = true;
	while (more) {
		uint8_t b = v & 0x7F;
		v >>= 7;                                   // arithmetic shift
		if ((v == 0 && !(b & 0x40)) || (v == -1 && (b & 0x40))) more = false;
		else b |= 0x80;
		out.push_back(b);
	}
}

// ── Function body builder ──────────────────────────────────────────────────
// Emits into a flat code vector. Locals are declared up front by count; local
// index 0..nParams-1 are the parameters, the rest are the declared i32 locals.
struct Func {
	std::vector<uint8_t> code;
	uint32_t nParams = 0;
	// Local declarations in order. WASM groups locals by type as (count, type)
	// runs; keeping the types in a vector lets a function mix i32 and i64
	// without the caller tracking group boundaries.
	std::vector<ValType> localTypes;

	uint32_t addLocal(ValType t = VT_I32) {
		localTypes.push_back(t);
		return nParams + (uint32_t)localTypes.size() - 1;
	}

	void op(uint8_t o)                    { code.push_back(o); }
	void op(uint8_t o, uint32_t imm)      { code.push_back(o); u32leb(code, imm); }
	void i32Const(int32_t v)              { code.push_back(OP_I32_CONST); i32leb(code, v); }
	void localGet(uint32_t i)             { op(OP_LOCAL_GET, i); }
	void localSet(uint32_t i)             { op(OP_LOCAL_SET, i); }
	void localTee(uint32_t i)             { op(OP_LOCAL_TEE, i); }
	// Memory operands carry alignment (log2) then offset. The register file is
	// 4-byte aligned, so align=2 for i32 accesses and align=0 for the byte
	// loads the interrupt check uses.
	void i32Load(uint32_t offset)         { op(OP_I32_LOAD, 2); u32leb(code, offset); }
	void i64Load(uint32_t offset)         { op(OP_I64_LOAD, 3); u32leb(code, offset); }
	void i64Store(uint32_t offset)        { op(OP_I64_STORE, 3); u32leb(code, offset); }
	void i32Load8u(uint32_t offset)       { op(OP_I32_LOAD8U, 0); u32leb(code, offset); }
	void i32Store(uint32_t offset)        { op(OP_I32_STORE, 2); u32leb(code, offset); }
	void i32Store8(uint32_t offset)       { op(OP_I32_STORE8, 0); u32leb(code, offset); }
	void ifVoid()                         { op(OP_IF); code.push_back(VT_VOID); }
	void ifI32()                          { op(OP_IF); code.push_back(VT_I32); }
	void blockVoid()                      { op(OP_BLOCK); code.push_back(VT_VOID); }
	void loopVoid()                       { op(OP_LOOP); code.push_back(VT_VOID); }
	void br(uint32_t depth)               { op(OP_BR, depth); }
	void brIf(uint32_t depth)             { op(OP_BR_IF, depth); }
	void ret()                            { op(OP_RETURN); }
	void end()                            { op(OP_END); }
	// call_indirect against the imported table. In Emscripten a table index IS
	// a C function pointer, so this reaches a C++ function directly with no JS
	// shim in between — which is the whole reason the table is imported.
	void callIndirect(uint32_t typeIdx)   { op(OP_CALL_IND, typeIdx); u32leb(code, 0); }
	// A direct call to another function in the same module.
	void call(uint32_t funcIdx)           { op(OP_CALL, funcIdx); }

	// Absolute linear-memory access: the address is a compile-time constant, so
	// push 0 as the dynamic base and put the whole address in the static offset
	// field. That keeps the emitted sequence to two instructions.
	void loadAbs(uint32_t addr)  { i32Const(0); i32Load(addr); }
	void load64Abs(uint32_t addr){ i32Const(0); i64Load(addr); }
	void storeAbsPrep()          { i32Const(0); }   // caller then pushes value
	void storeAbs(uint32_t addr) { i32Store(addr); }
	void load8Abs(uint32_t addr) { i32Const(0); i32Load8u(addr); }
};

// ── Module builder ─────────────────────────────────────────────────────────
// One imported memory, zero or more imported functions, one exported function.
class Module {
public:
	// Declares an imported function with signature (i32...) -> i32 or -> void.
	// Returns its function index (imports come before defined functions).
	uint32_t addImportFunc(const std::string &module, const std::string &name,
	                       const std::vector<ValType> &params, bool returnsI32);
	// Imports env.__indirect_function_table, so generated code can reach the
	// main module's functions by table index.
	void importTable() { importTable_ = true; }
	// Interns a signature and returns its type index, for call_indirect.
	uint32_t signatureIndex(const std::vector<ValType> &params, bool returnsI32) {
		return typeIndex(params, returnsI32);
	}
	// Appends a defined+exported function. Returns its index (imports first,
	// then definitions in the order they are added).
	//
	// A module can hold many: instantiating one is expensive — measured at
	// ~4.4 ms, and it was 63% of the runtime when the code generator built one
	// module per compiled region — so regions are batched into a single module
	// and instantiated together.
	// An empty exportName makes the function INTERNAL: reachable by `call` from
	// the module's other functions and by nothing else. That is what lets a
	// batch of regions share one copy of the memory-access helpers instead of
	// each inlining sixty-odd instructions of it — measured, module compilation
	// and instantiation were 164 ms of a 6.4 s run, and the emitted bytes are
	// what V8 charges for.
	uint32_t addFunc(const std::string &exportName, const std::vector<ValType> &params,
	                 bool returnsI32, Func f);

	std::vector<uint8_t> finish() const;

private:
	struct Sig { std::vector<ValType> params; bool ret; };
	// Interns a signature and returns its type index.
	uint32_t typeIndex(const std::vector<ValType> &params, bool ret) const;

	mutable std::vector<Sig> types_;
	struct Import { std::string mod, name; uint32_t type; };
	std::vector<Import> imports_;
	bool importTable_ = false;
	struct Defined { std::string exportName; uint32_t type; Func body; };
	std::vector<Defined> funcs_;
};

}  // namespace wasmemit
