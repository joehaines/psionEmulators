// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

import { useCallback, useEffect, useState } from 'react';
import type { EmulatorControls } from '../hooks/useEmulator';
import {
  hex8, hex16, group4, formatUniqueId, sanitiseUniqueId, parseUniqueId,
} from '../lib/uniqueId';

interface Props {
  controls: EmulatorControls;
  onClose: () => void;
}

const btn        = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-mid border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';
const btnPrimary = 'px-3 py-1.5 rounded text-xs font-mono whitespace-nowrap cursor-pointer bg-psion-highlight border border-psion-accent/50 text-psion-charcoal hover:bg-psion-accent hover:text-white transition-colors disabled:opacity-40 disabled:cursor-not-allowed';

export default function MachineIdDialog({ controls, onClose }: Props) {
  const { machineId, machineIdPrefix, machineIdPrefixSettable,
          setMachineId, resetDevice } = controls;
  // 16 digits when this machine's ROM-side half is known, 8 when it isn't.
  // (Whether the high half will actually take a new value is a separate
  // question — machineIdPrefixSettable — and the read-back after Set says so.)
  const width = machineIdPrefix != null ? 16 : 8;
  const shown = useCallback((id: number) => formatUniqueId(machineIdPrefix, id), [machineIdPrefix]);

  const [text, setText] = useState(() => (machineId != null ? shown(machineId) : ''));
  const [status, setStatus] = useState('');
  const [busy, setBusy] = useState(false);
  // Set once an id has been programmed in this session: the running EPOC read
  // the identity chip at boot and cached it, so the change isn't visible to
  // the OS until the machine restarts.
  const [needsReset, setNeedsReset] = useState(false);

  // Fill the field in once the id is known — the panel can be opened while a
  // load is still settling. Never overwrites what the user has typed; the live
  // value is always shown in the "Current:" line below regardless.
  useEffect(() => {
    if (machineId != null && !busy) setText(prev => (prev === '' ? shown(machineId) : prev));
  }, [machineId, busy, shown]);

  const parsed = parseUniqueId(text, machineIdPrefix);

  const program = useCallback(async (id: bigint) => {
    setBusy(true);
    try {
      const effective = await setMachineId(id);
      if (effective == null) {
        setStatus('This device has no writable Unique id.');
        return;
      }
      const asShown = group4(hex16(effective).slice(machineIdPrefix != null ? 0 : 8));
      setText(asShown);
      setNeedsReset(true);
      // A machine can still refuse the high half — that one is a ROM
      // constant, and it only changes where the emulator could locate it.
      setStatus(effective !== id
        ? `Programmed ${asShown} — this machine wouldn't take all of it.`
        : `Unique id set to ${asShown}.`);
    } finally {
      setBusy(false);
    }
  }, [setMachineId, machineIdPrefix]);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60"
         onClick={onClose}>
      <div className="bg-psion-dark border border-psion-accent/40 rounded-lg p-4 w-[520px] max-w-[95vw] max-h-[85vh] overflow-y-auto shadow-xl"
           onClick={e => e.stopPropagation()}>
        <div className="flex justify-between items-center mb-3">
          <h2 className="text-sm font-mono font-semibold text-psion-charcoal">Unique id</h2>
          <button className={btn} onClick={onClose}>Close</button>
        </div>

        <p className="text-xs font-mono text-gray-500 mb-4 leading-relaxed">
          The machine's Unique id, as shown under System → Information →
          Machine. EPOC reads it at boot, and software that licenses itself to
          one machine keys off it. Setting it here reprograms the emulated
          identity chip (the ETNA PROM on the Series 5mx family, the serial
          EEPROM on the Series 7 / netBook / netpad) and remembers the value
          for this device, so it survives reloads and resets.
        </p>
        <p className="text-xs font-mono text-gray-500 mb-4 leading-relaxed">
          {machineIdPrefixSettable ? (
            <>
              All four groups are editable. The last two are the identity
              chip; the first two are the machine's model UID, a constant in
              the OS image — setting them patches every copy of it, so
              anything else that identifies the machine sees the new value
              too. On the 5mx Pro and netBook that image is the one on the CF
              card, so the change lands when the OS card is next inserted.
            </>
          ) : machineIdPrefix != null ? (
            <>
              The first two groups (<span className="text-psion-charcoal">
                {group4(hex8(machineIdPrefix))}</span>) are the model UID from
              the ROM; this machine's copy can't be located for patching, so
              only the last two — the identity chip — will change here.
            </>
          ) : (
            <>
              This is the identity chip's half of the id. The model UID EPOC
              prints in front of it isn't known for this machine, so it isn't
              shown.
            </>
          )}
          {' '}Reset the machine after setting it — EPOC caches the id when it
          boots.
        </p>

        <div className="flex items-end gap-2 flex-wrap mb-3">
          <label className="flex flex-col gap-1">
            <span className="text-[10px] font-mono uppercase tracking-wide text-gray-500">
              Unique id (hex)
            </span>
            <input
              type="text"
              value={text}
              spellCheck={false}
              autoComplete="off"
              placeholder={machineIdPrefix != null ? '1000-118A-1234-5678' : '1234-5678'}
              onChange={e => setText(sanitiseUniqueId(e.target.value, width))}
              onKeyDown={e => { if (e.key === 'Enter' && parsed != null) void program(parsed); }}
              className={`${machineIdPrefix != null ? 'w-56' : 'w-44'} px-2 py-1.5 rounded bg-psion-mid border border-psion-accent/50 text-xs font-mono text-psion-charcoal tracking-widest focus:outline-none focus:border-psion-accent`}
            />
          </label>
          <button
            className={btnPrimary}
            disabled={busy || parsed === null}
            onClick={() => { if (parsed != null) void program(parsed); }}
          >
            Set
          </button>
          <button
            className={btn}
            disabled={busy}
            title="Fill in a random id"
            onClick={() => {
              const r = new Uint32Array(2);
              crypto.getRandomValues(r);
              // Only randomise what this machine will accept.
              setText(machineIdPrefixSettable
                ? group4(hex8(r[0]) + hex8(r[1]))
                : shown(r[1]));
              setStatus('');
            }}
          >
            Randomise
          </button>
        </div>

        <div className="text-[10px] font-mono text-gray-500 mb-3">
          {machineIdPrefixSettable
            ? 'Up to 16 hexadecimal digits (grouped as EPOC displays them).'
            : machineIdPrefix != null
              ? 'Edit the last two groups — the first two snap back to the ROM value.'
              : 'Up to 8 hexadecimal digits (grouped as EPOC displays them).'}
        </div>

        <div className="text-xs font-mono text-gray-500 leading-relaxed">
          <div>
            Current: <span className="text-psion-charcoal">
              {machineId != null ? shown(machineId) : '—'}
            </span>
          </div>
          {status && <div className="mt-2">{status}</div>}
          {needsReset && (
            <div className="mt-2 flex items-center gap-2 flex-wrap">
              <span>
                EPOC cached the old id when it booted — reset the device for
                the OS to report the new one.
              </span>
              <button
                className={btnPrimary}
                disabled={busy}
                onClick={() => { setNeedsReset(false); resetDevice(); onClose(); }}
              >
                Reset device
              </button>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
