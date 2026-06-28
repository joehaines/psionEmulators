// Native-ROM upload smoke test: connect to the real 5mx ROM via the harness
// socket bridge and upload a ~190KB file (Jumpy-sized), confirming the new
// host-side retransmission path doesn't break or stall a real upload over a
// (lossless) socket. Reports elapsed time and whether bytes round-trip.
import * as net from 'node:net';
import { spawn } from 'node:child_process';
import * as fs from 'node:fs';
import { PlpClient } from '../plp/client-spec.ts';

const REPO = new URL('../../../../', import.meta.url).pathname;
const HARNESS = `${REPO}harness/run`;
const ROM = `${REPO}roms/5mx_v1.05(260)_eng.bin`;
const SIZE = Number(process.argv[2] ?? 190_000);
const SOCK = `/tmp/psion-plp-upload.sock`;
try { fs.unlinkSync(SOCK); } catch { /* */ }

let rxBuf: number[] = [];
let sock: net.Socket | null = null;
const read = () => { if (!rxBuf.length) return new Uint8Array(0); const o = new Uint8Array(rxBuf); rxBuf = []; return o; };
const write = (d: Uint8Array) => { sock?.write(Buffer.from(d)); return d.length; };

const server = net.createServer(s => { sock = s; s.on('data', d => { for (const b of d) rxBuf.push(b); }); s.on('error', () => {}); void run(); });
let child: ReturnType<typeof spawn>;
server.listen(SOCK, () => {
  child = spawn(HARNESS, [ROM, '--device', '5mx', '--quiet-logs',
    '--serial-bridge-socket', SOCK, '--serial-attach', '2', '8',
    '--serial-poll-until', '600', '2'],
    { env: { ...process.env, PSION_REALTIME: '1' }, stdio: ['ignore', 'ignore', 'ignore'] });
});

async function run(): Promise<void> {
  const client = new PlpClient(read, write, { pollHz: 50, protocol: 'rfsv32', conSeq: 4 });
  client.start();
  let code = 1;
  try {
    await client.connect(30_000);
    const payload = new Uint8Array(SIZE);
    for (let i = 0; i < SIZE; i++) payload[i] = (i * 31 + 7) & 0xFF;
    const t0 = Date.now();
    let lastPct = -1;
    await client.uploadFile('C:\\Documents\\big.bin', payload, (done, total) => {
      const pct = Math.floor((done / total) * 100);
      if (pct >= lastPct + 10) { lastPct = pct; console.log(`  upload ${pct}% (${done}/${total})`); }
    });
    const secs = ((Date.now() - t0) / 1000).toFixed(1);
    const back = await client.downloadFile('C:\\Documents\\big.bin');
    let ok = back.length === payload.length;
    if (ok) for (let i = 0; i < payload.length; i++) if (back[i] !== payload[i]) { ok = false; console.log(`  MISMATCH at ${i}`); break; }
    console.log(`\nRESULT: upload ${SIZE}B in ${secs}s, round-trip ${ok ? 'OK' : 'CORRUPT'} (got ${back.length}B)`);
    code = ok ? 0 : 1;
  } catch (e) {
    console.log(`\nRESULT: FAIL ${(e as Error).message}`);
  } finally {
    try { client.disconnect(); } catch {}
    try { child.kill('SIGKILL'); } catch {}
    process.exit(code);
  }
}
