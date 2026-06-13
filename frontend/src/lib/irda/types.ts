// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// IrDA protocol constants shared across the infrared stack layers
// (IrLAP / IrLMP / IAS / Tiny TP / IrOBEX). Kept dependency-free so the
// Node strip-types test runner can load every layer without dragging in
// runtime code — mirror of lib/plp/types.ts.

// ── IrLAP address field ───────────────────────────────────────────────
// Address byte = (connectionAddress << 1) | C/R, where C/R is 1 for a
// command frame and 0 for a response. Connection address 0x7F is the
// broadcast address used for discovery/connect; a real connection uses a
// 7-bit value in 0x01..0x7E negotiated in SNRM.
export const IRLAP_CR_COMMAND = 0x01;
export const IRLAP_CR_RESPONSE = 0x00;
export const IRLAP_ADDR_BROADCAST = 0x7f; // 7-bit connection address
// Connection address we ask the device to use for our session. Any value
// in 0x01..0x7e is legal; the device echoes it in its UA response.
export const IRLAP_CONN_ADDR = 0x01;

// ── IrLAP control field — unnumbered (U) frames ───────────────────────
// Base values (P/F bit 0x10 cleared). OR in 0x10 to set the poll/final.
export const U_SNRM = 0x83; // Set Normal Response Mode (connect)
export const U_UA = 0x63; // Unnumbered Acknowledgement
export const U_DISC = 0x43; // Disconnect
export const U_DM = 0x0f; // Disconnected Mode
export const U_UI = 0x03; // Unnumbered Information
// XID — eXchange station IDentification (used for discovery).
//
// IrLAP/HDLC puts XID in two distinct U-frame control encodings. The
// *command* (what the discovering primary sends, P-bit settable) uses the
// canonical HDLC XID control 0x2F. The 5mx/EPOC secondary answers with an
// XID *response* whose control sets the high modifier bit: 0xAF (F-bit
// settable). The earlier code used 0xAF for the command too — the real
// device received the frame (valid FCS/address) but didn't recognise the
// control as an XID command and replied with DM (0x0F), which surfaced as
// "No infrared device responding". Confirmed against a captured 5mx XID
// response (control 0xBF = 0xAF|F). We therefore SEND 0x2F and ACCEPT both
// 0x2F and 0xAF on receive.
export const U_XID = 0x2f; // XID command control (we transmit this)
export const U_XID_RESPONSE = 0xaf; // XID response control the secondary sends
export const U_FRMR = 0x87; // Frame Reject
export const PF_BIT = 0x10; // poll (command) / final (response) bit

// Supervisory (S) frame control: (Nr<<5) | (PF<<4) | code.
export const S_RR = 0x01; // Receive Ready
export const S_RNR = 0x05; // Receive Not Ready
export const S_REJ = 0x09; // Reject
// Information (I) frame control: (Nr<<5) | (PF<<4) | (Ns<<1). Bit0 = 0.

// ── IrLAP discovery (XID) I-field ─────────────────────────────────────
export const XID_FORMAT_DISCOVERY = 0x01;
// Slot-count encoding in the discovery flags low 2 bits.
export const XID_SLOTS_1 = 0x00;
export const XID_SLOTS_6 = 0x01;
export const XID_SLOTS_8 = 0x02;
export const XID_SLOTS_16 = 0x03;
export const XID_SLOT_FINAL = 0xff; // slot number in the final XID frame
// Charset byte preceding a discovery nickname. 0x00 = ASCII.
export const XID_CHARSET_ASCII = 0x00;

// ── IrLMP (LM-PDU) ────────────────────────────────────────────────────
// An LM-PDU rides in an IrLAP I-frame and starts with two LSAP-SEL bytes:
//   dstLSAP (bit7 = control flag), srcLSAP (bit7 = command/response).
// Control PDUs (dstLSAP bit7 set) carry an opcode byte next.
export const LMP_CONTROL_BIT = 0x80; // set in dstLSAP for a control PDU
export const LMP_CR_BIT = 0x80; // set in srcLSAP for a command PDU
export const LMP_CONNECT = 0x01; // Connect (with opcode reserved bits)
export const LMP_CONNECT_CNF = 0x81; // Connect confirm (reply form)
export const LMP_DISCONNECT = 0x02;
// Well-known LSAP-SELs.
export const LSAP_IAS = 0x00; // Information Access Service server
export const LSAP_CONNLESS = 0x70;
// Our client-side LSAP-SELs (any value in 0x01..0x6f works).
export const LSAP_CLIENT_IAS = 0x05;
export const LSAP_CLIENT_OBEX = 0x06;

// ── IAS (Information Access Service) ───────────────────────────────────
// IAS rides over an LM-MUX connection to LSAP 0x00. Operation byte: the
// low 6 bits are the opcode, bit7 is the last-frame flag, bit6 ack.
export const IAS_OP_GET_VALUE_BY_CLASS = 0x04;
export const IAS_LAST_FRAME = 0x80;
// IAS return codes.
export const IAS_RET_SUCCESS = 0x00;
export const IAS_RET_NO_SUCH_CLASS = 0x01;
export const IAS_RET_NO_SUCH_ATTRIB = 0x02;
// IAS value types.
export const IAS_TYPE_MISSING = 0x00;
export const IAS_TYPE_INTEGER = 0x01;
export const IAS_TYPE_OCTET_SEQ = 0x02;
export const IAS_TYPE_USER_STRING = 0x03;
// The class/attribute we query to find the device's file-receive server's
// LSAP-SEL. The Psion 5mx "Infrared receive" dialog is EPOC's Eikon IR
// transfer app (EIKIRDA.CPP), NOT a generic IrOBEX inbox. It registers IAS
// class "Epoc32:EikonIr:v2.0" (and v1.0) with attribute "IrDA:TinyTP:LsapSel"
// → integer 8 (KEikIrDALsapSel). Querying class "OBEX" returns NO_SUCH_CLASS
// because no OBEX server is registered by that dialog. We therefore query the
// EikonIr class. (The legacy IAS_OBEX_* names are retained as aliases so old
// callers/tests still resolve.)
export const IAS_EIKONIR_V2_CLASS = 'Epoc32:EikonIr:v2.0';
export const IAS_EIKONIR_V1_CLASS = 'Epoc32:EikonIr:v1.0';
export const IAS_TINYTP_LSAPSEL_ATTR = 'IrDA:TinyTP:LsapSel';
// Back-compat alias: the "OBEX class" the high-level client looks up is now
// the EikonIr v2 class.
export const IAS_OBEX_CLASS = IAS_EIKONIR_V2_CLASS;
// The well-known LSAP-SEL the EPOC Eikon IR receiver binds to (KEikIrDALsapSel).
// The IAS query returns this value; this constant is the documented default.
export const EIKONIR_LSAPSEL = 8;

// ── Tiny TP ───────────────────────────────────────────────────────────
// First octet of a TinyTP connect PDU: bit7 = P (parameters present),
// bits0-6 = initial credit. Data PDUs: bit7 = M (more/segmented),
// bits0-6 = delta credit.
export const TTP_P_BIT = 0x80;
export const TTP_M_BIT = 0x80;
export const TTP_CREDIT_MASK = 0x7f;
export const TTP_INITIAL_CREDIT = 7; // credit we advertise to the device

// ── IrOBEX ────────────────────────────────────────────────────────────
// Request opcodes (bit7 = final).
export const OBEX_CONNECT = 0x80;
export const OBEX_DISCONNECT = 0x81;
export const OBEX_PUT = 0x02;
export const OBEX_PUT_FINAL = 0x82;
// Response codes (bit7 = final).
export const OBEX_RESP_CONTINUE = 0x90;
export const OBEX_RESP_SUCCESS = 0xa0;
// Header identifiers (high 2 bits encode the value form).
export const OBEX_HDR_NAME = 0x01; // null-terminated UTF-16BE text
export const OBEX_HDR_TYPE = 0x42; // byte sequence
export const OBEX_HDR_LENGTH = 0xc3; // 4-byte big-endian unsigned
export const OBEX_HDR_BODY = 0x48; // byte sequence
export const OBEX_HDR_END_OF_BODY = 0x49; // byte sequence
export const OBEX_VERSION = 0x10; // OBEX 1.0
// Default maximum OBEX packet we offer / assume until the device's
// CONNECT response narrows it.
export const OBEX_DEFAULT_MTU = 0x2000;

// ── EPOC Eikon IR transfer protocol (the real 5mx "Infrared receive") ──
// Verified against reference/Epoc SDK .../Eikon/src/EIKIRDA.CPP. After the
// TinyTP channel to LSAP 8 is up, the sender drives a tiny text protocol:
//
//   sender → "FILE <size> <att> <timeHi> <timeLo> <name>"   (one TinyTP SDU)
//   recv   → "ACK Y"  (accept) or "ACK N" (reject)
//   sender → raw file bytes, chunked to the negotiated max data size
//   sender → TinyTP/MUX disconnect
//
// All command strings are 8-bit ASCII written verbatim as the SDU (no NUL).
// <size>     decimal file size in bytes
// <att>      decimal EPOC file-attribute mask (KEntryAttArchive = 0x20 = 32)
// <timeHi>/<timeLo>  the two 32-bit halves of the TInt64 EPOC modified time
//            (microseconds since 1AD nominal); 0 0 is accepted by the receiver.
// <name>     bare file name + extension (no path; may contain spaces — it is
//            parsed last, so everything after the 4th space is the name).
export const EIKONIR_FILE_ATTR_ARCHIVE = 0x20; // KEntryAttArchive
export const EIKONIR_ACK_YES = 'ACK Y';
export const EIKONIR_ACK_NO = 'ACK N';
