// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

// Frontend-side metadata about each device that isn't in the C++ device
// registry. Keyed by `DeviceProfile.id`. Currently the only field is the
// approximate launch year, used by the Settings "Sort by age" option.
//
// Years reflect the original consumer launch of each model. The Organiser
// II family is dated to its 1986 introduction (the emulated ROM is the
// later LZ revision, but the device line as a whole is 1986). Where a
// year isn't known, the device is sorted to the end of the age list.
export const DEVICE_LOGO_MAP: Record<string, string> = {
  // The Organiser I predates the Organiser II's wordmark logo, and the
  // line drawing in device-logos is of the II; the I falls through to
  // DevicePanel's initials rather than being labelled as its successor.
  organiser2:  'organiser2.png',
  mc200:       'mc600.png',
  mc400:       'mc600.png',
  mc400v126:   'mc600.png',
  mc600:       'mc600.png',
  series3:     '3_3a_pb1.png',
  series3a:    '3_3a_pb1.png',
  pocketbk:    '3_3a_pb1.png',
  workabout:   'workabout_workaboutmx.png',
  series3c:    '3c_3mx_pb2.png',
  series3mx:   '3c_3mx_pb2.png',
  pocketbk2:   '3c_3mx_pb2.png',
  siena:       'siena.png',
  series5:     '5_5mx_5mxpro_mc218.png',
  '5mx':       '5_5mx_5mxpro_mc218.png',
  '5mxpro':   '5_5mx_5mxpro_mc218.png',
  mc218:       '5_5mx_5mxpro_mc218.png',
  osaris:      'osaris.png',
  // The line drawing this row waited for: until it existed the only
  // Geofox artwork in the tree was a photograph, which would have read as
  // a mistake in the picker's 56x40 thumbnail beside fifteen drawings, so
  // the Geofox fell through to DevicePanel's initials instead.
  geofox:      'geofox.png',
  workaboutmx: 'workabout_workaboutmx.png',
  revo:        'revo.png',
  conan:       'conan_icon.png',
  conanv001:   'conan_icon.png',
  netbook:     '7_netbook.png',
  series7:     '7_netbook.png',
  netpad:      'netpad_logo.png',
};

export const DEVICE_RELEASE_YEARS: Record<string, number> = {
  organiser1:  1984,   // the first Psion computer of any kind
  organiser2:  1986,
  mc200:       1989,   // launched alongside the MC400
  mc400:       1989,
  mc400v126:   1989,
  series3:     1991,
  pocketbk:    1992,
  series3a:    1993,
  workabout:   1995,
  series3c:    1996,
  pocketbk2:   1996,
  siena:       1996,
  series5:     1997,
  geofox:      1997,   // Geofox Ltd's one and only machine, out the same
                       // year as the Series 5 and on the same EPOC R1
  series3mx:   1998,
  osaris:      1998,
  workaboutmx: 1998,
  '5mx':       1999,
  mc218:       1999,
  revo:        1999,
  netbook:     1999,
  '5mxpro':    2000,
  series7:     2000,
  conan:       2001,   // no consumer launch to date it by — the year the
                       // machine's own ROM was built (TRomHeader: 2001-06-20,
                       // and its splash reads "Psion Digital 2001")
  conanv001:   2001,   // the earlier engineering image (TRomHeader: 2001-05-12)
  netpad:      2001,
};

// Devices whose ROM speaks the ER3/ER4 CL-PS711x dialect of the PLP link
// layer rather than the EPOC R5 one. Two things follow from membership,
// and both matter wherever a PlpClient is built:
//
//   * the Req_Con_Pdu these ROMs accept carries seq 0x22, not the R5
//     machines' 0x24 (LinkConfig.conSeq);
//   * the host must never speak first. A host-sent Req_Req_Pdu doesn't
//     just go unanswered on these ROMs, it wedges their link server —
//     so the client waits for the device's own burst (PlpClient's
//     passive mode) and the cable is re-plugged on every connect to
//     provoke a fresh one.
//
// Kept here, in one list, because it was previously spelled out
// separately at each call site and the Geofox — added long after the
// Series 5 and Osaris — reached only one of them.
export const CLPS711X_LINK_DEVICES: ReadonlySet<string> =
  new Set(['series5', 'osaris', 'geofox']);

// Req_Con_Pdu seq flavour for a device id — feeds LinkConfig.conSeq, and
// conSeq === 2 is also what marks the passive handshake above. Takes a
// nullable id so callers holding "no device loaded yet" can ask without
// a guard; that answers 4, the R5 default, and nothing is connected to
// talk to anyway.
export function linkConSeq(deviceId: string | null | undefined): number {
  return deviceId != null && CLPS711X_LINK_DEVICES.has(deviceId) ? 2 : 4;
}
