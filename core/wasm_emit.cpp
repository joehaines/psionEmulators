#include "wasm_emit.h"

namespace wasmemit {

namespace {

void section(std::vector<uint8_t> &out, uint8_t id, const std::vector<uint8_t> &body) {
	if (body.empty()) return;
	out.push_back(id);
	u32leb(out, (uint32_t)body.size());
	out.insert(out.end(), body.begin(), body.end());
}

void name(std::vector<uint8_t> &out, const std::string &s) {
	u32leb(out, (uint32_t)s.size());
	out.insert(out.end(), s.begin(), s.end());
}

}  // namespace

uint32_t Module::typeIndex(const std::vector<ValType> &params, bool ret) const {
	for (size_t i = 0; i < types_.size(); i++)
		if (types_[i].ret == ret && types_[i].params == params) return (uint32_t)i;
	types_.push_back(Sig{params, ret});
	return (uint32_t)(types_.size() - 1);
}

uint32_t Module::addImportFunc(const std::string &module, const std::string &nm,
                               const std::vector<ValType> &params, bool returnsI32) {
	imports_.push_back(Import{module, nm, typeIndex(params, returnsI32)});
	return (uint32_t)(imports_.size() - 1);
}

uint32_t Module::addFunc(const std::string &exportName, const std::vector<ValType> &params,
                         bool returnsI32, Func f) {
	funcs_.push_back(Defined{exportName, typeIndex(params, returnsI32), std::move(f)});
	return (uint32_t)(imports_.size() + funcs_.size() - 1);
}

std::vector<uint8_t> Module::finish() const {
	std::vector<uint8_t> out = {0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00};

	// The memory import must be emitted before the type section is serialised,
	// because addImportFunc/setFunc may still be interning signatures. They are
	// all done by the time finish() runs, so types_ is final here.

	// Type section (1)
	std::vector<uint8_t> t;
	u32leb(t, (uint32_t)types_.size());
	for (const auto &sig : types_) {
		t.push_back(0x60);
		u32leb(t, (uint32_t)sig.params.size());
		for (auto p : sig.params) t.push_back((uint8_t)p);
		if (sig.ret) { u32leb(t, 1); t.push_back((uint8_t)VT_I32); }
		else         { u32leb(t, 0); }
	}
	section(out, 1, t);

	// Import section (2): the emulator's linear memory, then any host functions.
	// The memory is imported rather than defined so the generated code addresses
	// the real GPRs[] in place — the whole point of the exercise.
	std::vector<uint8_t> im;
	u32leb(im, (uint32_t)imports_.size() + 1 + (importTable_ ? 1u : 0u));
	name(im, "env"); name(im, "memory");
	im.push_back(0x02);            // memory
	im.push_back(0x00);            // flags: no maximum
	u32leb(im, 0);                 // minimum pages (the host's memory is larger)
	if (importTable_) {
		name(im, "env"); name(im, "__indirect_function_table");
		im.push_back(0x01);        // table
		im.push_back(0x70);        // funcref
		im.push_back(0x00);        // flags: no maximum
		u32leb(im, 0);             // minimum entries
	}
	for (const auto &i : imports_) {
		name(im, i.mod); name(im, i.name);
		im.push_back(0x00);        // func
		u32leb(im, i.type);
	}
	section(out, 2, im);

	if (funcs_.empty()) return out;

	// Function section (3)
	std::vector<uint8_t> fn;
	u32leb(fn, (uint32_t)funcs_.size());
	for (const auto &d : funcs_) u32leb(fn, d.type);
	section(out, 3, fn);

	// Export section (7). A function with no name is internal — a helper the
	// module's own functions call, which nothing outside needs to reach.
	std::vector<uint8_t> ex;
	uint32_t nExports = 0;
	for (const auto &d : funcs_) if (!d.exportName.empty()) nExports++;
	u32leb(ex, nExports);
	for (size_t i = 0; i < funcs_.size(); i++) {
		if (funcs_[i].exportName.empty()) continue;
		name(ex, funcs_[i].exportName);
		ex.push_back(0x00);            // func
		u32leb(ex, (uint32_t)(imports_.size() + i));
	}
	section(out, 7, ex);

	// Code section (10)
	std::vector<uint8_t> cs;
	u32leb(cs, (uint32_t)funcs_.size());
	for (const auto &d : funcs_) {
		// Locals are declared as runs of (count, type); walk the declaration
		// order and coalesce adjacent same-type locals into one run.
		std::vector<uint8_t> body;
		std::vector<std::pair<uint32_t, ValType>> runs;
		for (ValType t : d.body.localTypes) {
			if (!runs.empty() && runs.back().second == t) runs.back().first++;
			else runs.push_back({1u, t});
		}
		u32leb(body, (uint32_t)runs.size());
		for (const auto &r : runs) { u32leb(body, r.first); body.push_back((uint8_t)r.second); }
		body.insert(body.end(), d.body.code.begin(), d.body.code.end());
		body.push_back(OP_END);

		u32leb(cs, (uint32_t)body.size());
		cs.insert(cs.end(), body.begin(), body.end());
	}
	section(out, 10, cs);

	return out;
}

}  // namespace wasmemit
