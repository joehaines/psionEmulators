// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// PLP wire-format constants — all values from the official PLP spec
// (reference/Psion Link Protocol.pdf in this repo, also at
//  https://thoukydides.github.io/riscos-psifs/plp.html).
//
// Verified against captured traffic from the emulated Revo via the
// native harness (--serial-attach + --serial-auto-rule); see
// harness/run.cpp for the test surface and commit history for the
// captures that pinned each constant down.

// ── Data-link special characters ────────────────────────────────────
export const STX = 0x02;
export const ETX = 0x03;
export const EOT = 0x04;     // stuffed-ETX in the EPOC variant
export const DLE = 0x10;
export const SYN = 0x16;     // start-of-frame marker

// ── PDU types (high nibble of the Cont/Seq byte) ────────────────────
//
//   Cont  Seq         Name           Description
//   ----  ----------  -------------  -----------------------------------
//   0     last-rx     Ack_Pdu        Acknowledge or complete handshake
//   1     0           Disc_Pdu       Disconnect
//   1     1           Disc_Req_Pdu   First step of handshake disconnect
//   2     0           Req_Pdu        SIBO connection request
//   2     1-3         Req_Req_Pdu    EPOC initial connection request
//   4     4-6         Req_Con_Pdu    EPOC connection-request confirm
//                                    (4-byte magic in Data)
//   3     next-tx     Data_Pdu       Data frame (NCP packet inside)
//
// Note: empirically the Revo R5 ROM uses Cont=2 Seq=4 (= 0x24) for its
// Req_Con_Pdu, not the documented Cont=4 Seq=4 (= 0x44). The behaviour
// is identical (4-byte magic, advance on Ack_Pdu Seq=0) but the
// nibble layout differs from the spec. Both encodings are accepted on
// receive; we transmit using the device's preferred Cont=2 form so
// the round-trip works against the real device.
export const PDU_CONT_ACK     = 0x0;
export const PDU_CONT_DISC    = 0x1;
export const PDU_CONT_REQ     = 0x2;  // also Req_Req_Pdu and Req_Con_Pdu on R5
export const PDU_CONT_DATA    = 0x3;
export const PDU_CONT_REQ_CON = 0x4;  // spec value; Revo R5 uses CONT_REQ instead

// ── NCP frame types ─────────────────────────────────────────────────
// All NCP packets start with `[destChan, srcChan, type, ...]`.
// Control-frame variants (when destChan = 0x00) are listed in
// `NcpControl` below.
export const NCP = {
  COMPLETE: 0x01,     // Complete frame (final / single-fragment payload)
  PARTIAL:  0x02,     // Partial frame (will be followed by more)
  CONNECT:  0x03,     // Connect frame: ask to open a server channel
  CONNECT_RESPONSE: 0x04,
  CONN_TERM: 0x05,    // Server disconnected all clients
  INFO:     0x06,     // First frame after link-up; carries version + ID
  DISCONNECT: 0x07,   // Client disconnected from a server
  NCP_TERM: 0x08,     // NCP is shutting down
} as const;

// Special control sub-types used when destChan == 0 and the frame
// targets the NCP itself rather than a higher-level server.
export const NCP_CONTROL = {
  XOFF: 0x01,
  XON:  0x02,
} as const;

// NCP Information `Version` byte values.
export const NCP_VERSION = {
  SIBO_OLD:  0x02,
  SIBO_NEW:  0x03,
  EPOC_ER3:  0x06,    // what the Revo's ROM actually advertises
  EPOC_ER5:  0x10,
} as const;

// Well-known server channel and service names.
export const NCP_CONTROL_CHAN = 0x00;
export const NCP_LINK_CHAN    = 0x01;
export const NCP_SERVICE_LINK = 'LINK.*';
export const NCP_SERVICE_RFSV = 'SYS$RFSV.*';
export const NCP_SERVICE_RPCS = 'SYS$RPCS.*';
// Print spooler behind the device's "Printer via PC" driver. Unlike
// RFSV it isn't running until a LINK Register command loads it
// (register as 'SYS$WPRT', then Connect to 'SYS$WPRT.*').
export const NCP_SERVICE_WPRT  = 'SYS$WPRT.*';
export const NCP_REGISTER_WPRT = 'SYS$WPRT';

// ── WPRT (print spooler) ────────────────────────────────────────────
// Command/reply service: the host polls WPRT_DATA and the device
// replies with packets of printer data grouped by page. See the PLP
// spec "WPRT Server" section and lib/plp/wprt-spec.ts.
export const WPRT = {
  LEVEL:  0x00,   // exchange version numbers (2.0 everywhere)
  DATA:   0xF0,   // read printer data
  CANCEL: 0xF1,   // cancel the current job (reply = pending data)
  STOP:   0xFF,   // terminate the server (no reply)
} as const;

// Last-packet / last-page marker bytes in WPRT_DATA replies.
export const WPRT_MORE = 0x2A;   // not the last packet/page
export const WPRT_LAST = 0xFF;   // end of print job / last page

// ── RFSV-32 (file services) ─────────────────────────────────────────
// Wire format (EPOC variant):
//   request: [reasonCode:u16-LE] [opId:u16-LE] [requestData...]
//   reply:   [0x0011:u16-LE]      [opId:u16-LE] [statusCode:i32-LE] [replyData...]
// Strings in payloads are 2-byte LE length + bytes (NOT NUL-terminated;
// most significant bit of the length field is a Unicode flag). The
// payload for an NCP Connect frame is the exception — the server name
// is NUL-terminated.
export const RFSV32 = {
  CLOSE_HANDLE:     0x01,
  OPEN_DIR:         0x10,
  READ_DIR:         0x12,
  GET_DRIVE_LIST:   0x13,
  VOLUME:           0x14,
  OPEN_FILE:        0x16,
  TEMP_FILE:        0x17,
  READ_FILE:        0x18,
  WRITE_FILE:       0x19,
  SEEK_FILE:        0x1A,
  DELETE:           0x1B,
  RENAME:           0x1F,
  MK_DIR_ALL:       0x20,
  PATH_TEST:        0x2B,
  CREATE_FILE:      0x29,   // verified against real Revo via harness
  REPLACE_FILE:     0x2A,   // verified against real Revo via harness
  SET_VOLUME_LABEL: 0x10,   // collides with OPEN_DIR; needs verification
} as const;

// RFSV-32 reply marker (replaces the request's reasonCode field).
export const RFSV32_REPLY_MARKER = 0x0011;

// Attribute bitflags returned by RFSV32_READ_DIR and accepted by
// RFSV32_OPEN_DIR's attribute filter.
export const ATTR = {
  READ_ONLY:   0x0001,
  HIDDEN:      0x0002,
  SYSTEM:      0x0004,
  DIRECTORY:   0x0010,
  ARCHIVE:     0x0020,
  VOLUME:      0x0040,
  NORMAL:      0x0080,
  TEMPORARY:   0x0100,
  COMPRESSED:  0x0800,
} as const;

// RFSV32_OPEN_FILE mode flags.
export const FILE_MODE = {
  SHARE_EXCLUSIVE: 0x0000,
  SHARE_READ:      0x0001,
  SHARE_ANY:       0x0002,
  BINARY:          0x0000,
  TEXT:            0x0020,
  READ_WRITE:      0x0200,
} as const;

// Status codes we surface in UI. All other negative values pass through.
export const EpocErr = {
  None:         0,
  NotFound:    -1,
  AlreadyExists: -11,
  PathNotFound: -12,
  AccessDenied: -21,
  DirFull:     -33,
  Eof:         -25,
} as const;
export type EpocErr = (typeof EpocErr)[keyof typeof EpocErr];

// ── Legacy constants ────────────────────────────────────────────────
// The first-cut NcpType and Rfsv32Op values pre-date the PDF spec
// landing in this repo. They're WRONG (the wire format the device
// actually expects is documented above as `NCP` and `RFSV32`) but the
// existing rfsv32.ts / ncp.ts / e2e mock still drive them, and the
// browser dialog still calls into that code. Rather than break
// everything at once, the legacy table stays compiling against the
// old (wrong) byte values; the rewrite happens incrementally in the
// follow-up commits that re-wire each layer to the new constants.
//
// What's verified to work against the real Revo (via the harness's
// --serial-* surface in harness/run.cpp) is the protocol described by
// the `NCP`, `RFSV32`, and PDU_CONT_* constants above — not these
// aliases. If you're writing new code, use the new constants.
export const NcpType = {
  NCON_REQ:  0x01,
  NCON_ACK:  0x02,
  NDIS_REQ:  0x04,
  NDIS_ACK:  0x05,
  NDATA:     0x09,
  NACK:      0x12,
  NREGISTER: 0x14,
  NREG_ACK:  0x15,
} as const;
export type NcpType = (typeof NcpType)[keyof typeof NcpType];

export const Rfsv32Op = {
  FOPEN:          0x01,
  FCLOSE:         0x02,
  FREAD:          0x03,
  FWRITE:         0x04,
  FSEEK:          0x05,
  FFLUSH:         0x06,
  FSETSIZE:       0x07,
  RENAME:         0x08,
  DELETE:         0x09,
  FINFO:          0x0A,
  SFSTAT:         0x0B,
  PARSE:          0x0C,
  DRIVE_LIST:     0x0D,
  DRIVE_INFO:     0x0E,
  SETVOLUMELABEL: 0x0F,
  FOPENUNIQUE:    0x10,
  TEMPFILE:       0x11,
  READDIR:        0x12,
  GETTIME:        0x13,
  CHANGEDIR:      0x14,
  SETMODIFY:      0x16,
  SETATT:         0x17,
  FGETATT:        0x18,
  FGETATTANDMOD:  0x19,
  MKDIR:          0x1A,
  OPENDIR:        0x1B,
  CLOSEDIR:       0x1C,
  READDIR_LFN:    0x1D,
} as const;
export type Rfsv32Op = (typeof Rfsv32Op)[keyof typeof Rfsv32Op];

// Old per-flag exports used by the existing client code.
export const RFSV32_OMODE_OPEN_EXISTING = 0x0000;
export const RFSV32_OMODE_CREATE        = 0x0100;
export const RFSV32_OMODE_REPLACE       = 0x0200;
export const RFSV32_OMODE_BINARY        = FILE_MODE.BINARY;
export const RFSV32_OMODE_TEXT          = FILE_MODE.TEXT;
export const RFSV32_OMODE_SHARE_EXCL    = FILE_MODE.SHARE_EXCLUSIVE;
export const RFSV32_OMODE_SHARE_READ    = FILE_MODE.SHARE_READ;
export const RFSV32_OMODE_SHARE_RW      = FILE_MODE.SHARE_ANY;
