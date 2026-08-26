#!/usr/bin/env python3
"""Checks kJitTraceStopPcs against the hooks it is supposed to describe.

The SoC's per-batch hooks act on exact program counters, found by sampling
cpu.getRealPC(). A generated region runs for hundreds of tick-equivalents
without returning, so an address inside one is never observable at a sampling
point and the hook keyed on it silently stops firing — the netBook stopped
mounting its card exactly this way. core/arm_jit.cpp therefore ends a trace at
any address in kJitTraceStopPcs.

That list has to stay in step with the hooks by hand, and nothing else would
notice if it did not: the failure is silent, and only under the code generator.
So this re-extracts the addresses from the hook bodies and fails if the table
has fallen behind. Extra entries in the table are fine (a superset only costs a
little coverage); missing ones are not.

Usage: python3 tests/unit/check-jit-trace-stops.py [core/sa1100.cpp]
"""
import re
import sys

# Hooks that run from the batch loop. cascadeTrace is included even though it is
# diagnostic-only: a superset is the safe direction.
HOOKS = ["s7CfMountHook", "nbCfMountHook", "pwrUpTrace", "cascadeTrace",
         "recFixHook", "dqGuardHook", "tmrHook", "tmrStartFix"]

# Names a hook body binds cpu.getRealPC() to before comparing.
PC_NAMES = r"getRealPC\(\)|\bpc\b|\bp\b|\bprePc\b|\bnowPc\b"


def lambda_body(lines, name):
    """The source lines of `auto <name> = [...] { ... };`, by brace depth."""
    start = next((i for i, l in enumerate(lines) if f"auto {name} = [" in l), None)
    if start is None:
        return None
    depth, seen = 0, False
    for i in range(start, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        seen = seen or "{" in lines[i]
        if seen and depth <= 0:
            return lines[start:i + 1]
    return lines[start:]


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else "core/sa1100.cpp"
    lines = open(path).read().split("\n")

    table = set()
    m = re.search(r"kJitTraceStopPcs\[\]\s*=\s*\{(.*?)\};", "\n".join(lines), re.S)
    if not m:
        print("FAIL: kJitTraceStopPcs not found in", path)
        return 1
    for h in re.finditer(r"0x([0-9a-fA-F]+)u?", m.group(1)):
        table.add(int(h.group(1), 16))

    found, missing = set(), {}
    for name in HOOKS:
        body = lambda_body(lines, name)
        if body is None:
            print(f"FAIL: hook {name} not found — has it been renamed?")
            return 1
        text = "\n".join(body)
        pcs = set()
        for h in re.finditer(rf"(?:{PC_NAMES})\s*(?:==|!=)\s*0x([0-9a-fA-F]{{6,8}})u?", text):
            pcs.add(int(h.group(1), 16))
        for h in re.finditer(rf"0x([0-9a-fA-F]{{6,8}})u?\s*==\s*(?:{PC_NAMES})", text):
            pcs.add(int(h.group(1), 16))
        found |= pcs
        gap = pcs - table
        if gap:
            missing[name] = sorted(gap)

    if missing:
        print("FAIL: hooks watch addresses kJitTraceStopPcs does not list.")
        print("A generated region would swallow these and the hook would stop firing.")
        for name, pcs in missing.items():
            print(f"  {name}: " + " ".join(f"0x{p:08x}u," for p in pcs))
        print("\nAdd them to kJitTraceStopPcs in core/sa1100.cpp.")
        return 1

    stale = table - found
    print(f"jit-trace-stops: {len(found)} addresses across {len(HOOKS)} hooks, "
          f"all present in a table of {len(table)}"
          + (f" ({len(stale)} extra, which is harmless)" if stale else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
