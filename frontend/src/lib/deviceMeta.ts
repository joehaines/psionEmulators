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
  organiser2:  'organiser2.png',
  mc400:       'mc600.png',
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
  workaboutmx: 'workabout_workaboutmx.png',
  revo:        'revo.png',
  netbook:     '7_netbook.png',
  series7:     '7_netbook.png',
};

export const DEVICE_RELEASE_YEARS: Record<string, number> = {
  organiser2:  1986,
  mc400:       1989,
  series3:     1991,
  pocketbk:    1992,
  series3a:    1993,
  workabout:   1995,
  series3c:    1996,
  pocketbk2:   1996,
  siena:       1996,
  series5:     1997,
  series3mx:   1998,
  osaris:      1998,
  workaboutmx: 1998,
  '5mx':       1999,
  mc218:       1999,
  revo:        1999,
  netbook:     1999,
  '5mxpro':    2000,
  series7:     2000,
};
