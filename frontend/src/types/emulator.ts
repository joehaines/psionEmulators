// SPDX-License-Identifier: LicenseRef-PsionWebEmulator
// Copyright (c) 2024-2026 Joe Haines <joehaines@gmail.com>. See LICENSE.

export interface DeviceInfo {
  digitiserWidth: number;
  digitiserHeight: number;
  lcdOffsetX: number;
  lcdOffsetY: number;
  lcdWidth: number;
  lcdHeight: number;
  deviceName: string;
  hasAudio: boolean;
  // Whether the device hardware has a microphone. hasAudio covers the
  // speaker side; speaker-only machines (buzzer-era SIBO, Siena,
  // Workabout) report hasMic=false so the Mic toggle is hidden.
  // Optional so an older psion.wasm without the field still loads.
  hasMic?: boolean;
  audioSampleRate: number;
}

export interface DeviceProfile {
  id: string;
  displayName: string;
  romFilename: string;
  skinFilename: string;
  status: 'supported' | 'coming-soon';
  // Whether the device has a CompactFlash slot. False for SIBO
  // machines (Series 3 family, Siena, Workabout) that used Psion's
  // proprietary SSD packs, and for the Revo (no external storage
  // slot at all). The CF Card control in EmulatorView is hidden on
  // devices where this is false.
  hasCFSlot: boolean;
  // Whether the device has an MMC (MultiMediaCard) slot. True only on
  // the netpad, whose card is an MMC in SPI mode on the board FPGA
  // rather than a PC-Card socket. It feeds the same card dialog as
  // hasCFSlot — either way the user hands the device a raw FAT16 image
  // — but the labels say MMC. No device has both.
  hasMmcSlot: boolean;
  // Number of Psion SSD pack slots. 0 for ARM-based machines that
  // never had SSD hardware; 2 for Series 3/3a/3c/3mx; 1 for Siena
  // (single Honda-connector slot). The frontend renders the SSD
  // dialog when this is > 0.
  ssdSlotCount: number;
  // Number of Psion Organiser II Datapak / Rampak slots. 2 on every
  // shipping Organiser II model; 0 elsewhere. Distinct from the SIBO
  // SSD slots above because the pack model has a per-slot kind
  // discriminant (Datapak = read-only EPROM vs Rampak = writable
  // SRAM), which the frontend dialog surfaces. Drives visibility of
  // the Datapak control in EmulatorView.
  datapakSlotCount: number;
  // Remote Link capability: the uartIndex the cable serial port is
  // wired to (passed to serialAttachHost), or -1 when the device has
  // no host-bridgeable serial port. Drives the Remote Link button.
  // Optional so an older psion.wasm still parses.
  remoteLinkUart?: number;
  // Infrared capability: the uartIndex of the IR transceiver, or -1
  // when the device has no IR port. Drives the Infrared button.
  infraredUart?: number;
  // Link-layer protocol the ROM speaks over the cable: 0 = none,
  // 1 = EPOC32 PLP + RFSV32, 2 = EPOC16 (SIBO) PLP + RFSV16.
  linkProtocol?: number;
  // Protocol over IR: 0 = none, 1 = IrDA + Eikon-IR beam (EPOC32),
  // 2 = PLP-over-IR remote link (SIBO).
  irProtocol?: number;
  // Hide this profile from the device picker. Set on alternate-ROM
  // variants of a device that already appears under its main id (e.g.
  // the older MC400 v1.26F ROM): still loadable by id and shown in the
  // header, just kept out of the picker list. Optional so an older
  // psion.wasm without the field still parses (treated as false).
  hiddenFromPicker?: boolean;
}

export interface PsionModule {
  prepareROMUpload(size: number): number;
  loadBufferedROM(size: number, deviceId?: string): string;
  stepFrame(): boolean;
  // Unbounded variant of stepFrame: advances a full frame of simulated time
  // regardless of wall-clock cost (stepFrame() is wall-clock-budgeted to keep
  // the main thread responsive under a CPU-bound guest). Used by the cold-boot
  // preroll, which must fast-forward a fixed amount of sim time.
  stepFrameFull(): boolean;
  readLCD(ptr: number): void;
  getDeviceInfo(): DeviceInfo;
  sendKey(epocKey: number, down: boolean): void;
  sendTouch(x: number, y: number, down: boolean): void;
  // Live state of the LCD electroluminescent backlight pin. Polled by
  // EmulatorView to drive the on-screen backlight overlay. Optional so
  // a freshly-pulled checkout whose psion.wasm pre-dates this binding
  // still loads — JS treats missing as "always off".
  getBacklight?(): boolean;
  // Quarter-turns anticlockwise the panel image has to be shown at for
  // the UI to read upright. Non-zero only on the netpad, whose Tools
  // menu → "Switch orientation" makes EPOC draw the desktop rotated
  // inside the same 640×240 framebuffer. Optional so a psion.wasm that
  // pre-dates the binding still loads (missing = never rotated).
  getScreenOrientation?(): number;
  prepareCFImageUpload(size: number): number;
  attachCFImage(size: number): boolean;
  // In-place CF content update (Series 7): replaces the card bytes without an
  // eject/insert and refreshes the OS's cached directory listing. Optional so
  // an older psion.wasm without the export still type-checks; callers guard on
  // `typeof mod.updateCFImageInPlace === 'function'`.
  updateCFImageInPlace?(size: number): boolean;
  detachCFImage(): void;
  isCFImageAttached(): boolean;
  getCFImageSize(): number;
  readCFImage(ptr: number): void;
  // ── RAM snapshot ──────────────────────────────────────────────────
  // Live snapshot of the emulator's host-visible RAM (SIBO/SIBO2 only;
  // returns 0 size on devices without a meaningful host RAM image).
  // The "Download RAM Snapshot" button uses this to capture state at
  // the moment of a user-reported bug for offline replay.
  getRamSnapshotSize(): number;
  readRamSnapshot(ptr: number): void;
  // ── Host serial bridge ────────────────────────────────────────────
  // Bridges JS to the Windermere UART (Revo / MC218 / Series 5 / 5mx).
  // uartIndex is 1 or 2 matching the SoC's UART1/UART2. Returns false
  // on non-Windermere devices. Used by the Remote Link dialog to talk
  // PLP to the emulated Revo's built-in PsiWin service.
  serialAttachHost(uartIndex: number): boolean;
  serialDetachHost(uartIndex: number): boolean;
  serialIsAttached(uartIndex: number): boolean;
  serialWriteFromHost(uartIndex: number, ptr: number, len: number): number;
  serialReadToHost(uartIndex: number, ptr: number, cap: number): number;
  serialHostTxAvailable(uartIndex: number): number;
  serialPumpCycles?(): void;
  // ── Psion SSD pack uploads (slot 0 / 1) ───────────────────────────
  prepareSSDImageUpload(size: number): number;
  // ssdType mirrors PsionSSD::Type — 0 auto (sniff 0xF1A5), 1 RAM,
  // 2 Type 1 Flash, 3 hardware write-protected. RAM and Flash are both
  // read/write drives on the Psion; 3 is the factory system-disk strap
  // and mounts read-only.
  attachSSDImage(slot: number, size: number, ssdType: number): boolean;
  detachSSDImage(slot: number): void;
  isSSDImageAttached(slot: number): boolean;
  getSSDImageSize(slot: number): number;
  readSSDImage(slot: number, ptr: number): void;
  // ── Psion Organiser II Datapak / Rampak (slot 0 / 1) ──────────────
  // Distinct from the SSD path above: kind discriminant lets the UI
  // label the slot as Datapak (read-only) vs Rampak (writable). 0 =
  // empty, 1 = Datapak, 2 = Rampak.
  prepareDatapakUpload(size: number): number;
  attachDatapakImage(slot: number, size: number): boolean;
  detachDatapakImage(slot: number): void;
  isDatapakAttached(slot: number): boolean;
  getDatapakImageSize(slot: number): number;
  readDatapakImage(slot: number, ptr: number): void;
  getDatapakKind(slot: number): number;
  isCFPollGapActive(): boolean;
  // ── EPOC machine ID ───────────────────────────────────────────────
  // The factory-programmed 32-bit ID in the device's identity chip
  // (ETNA PROM on the Series 5mx family / Revo, Eiger serial EEPROM on
  // the Series 7 / netBook / netpad). hasMachineId() is false on devices
  // whose identity chip the guest can't read, and setMachineId may store
  // fewer than 32 bits (the SA-1100 EEPROM word carries reserved panel
  // fields) — so always read the effective value back with
  // getMachineId(). All three are optional so a psion.wasm that
  // pre-dates the bindings still loads; the UI hides the control then.
  hasMachineId?(): boolean;
  getMachineId?(): number;
  // The ID the device powers up with — asked of the emulator rather than
  // remembered host-side so it stays right across a saved-state restore,
  // where the snapshot itself carries a reprogrammed ID.
  // High half of the 64-bit Unique id EPOC prints — the model UID the
  // kernel takes from the ROM. 0 when it hasn't been read out of that
  // machine's dialog. Settable only where the constant could be located
  // unambiguously in the ROM image (canSetMachineIdPrefix).
  getMachineIdPrefix?(): number;
  canSetMachineIdPrefix?(): boolean;
  setMachineIdPrefix?(prefix: number): boolean;
  setMachineId?(id: number): boolean;
  // ── ROM language variant ──────────────────────────────────────────
  // A multilingual ROM carries one set of resources per language and
  // picks between them at boot. getLanguageCount() is 0 or 1 where there
  // is nothing to choose — only the Geofox One has two, English (UK) and
  // English (USA). The guest reads the choice once during boot, so
  // setLanguage() lands on the next reset. Optional so a psion.wasm that
  // pre-dates the bindings still loads; the UI hides the control then.
  getLanguageCount?(): number;
  getLanguageName?(index: number): string;
  getLanguage?(): number;
  setLanguage?(index: number): boolean;
  // Debug/feature flag: set a PSION_* process env var before a device loads
  // (e.g. PSION_NB_NATIVE_CF for the netBook faithful CF boot).  Optional —
  // older WASM builds may not export it.
  setEnvVar?(name: string, value: string): void;
  setLoggingEnabled(enabled: boolean): void;
  getAllDeviceProfilesJSON(): string;
  // Drains up to `maxSamples` int16 speaker samples into the WASM heap at
  // `ptr` (byte offset). Returns the number written.
  readAudioOutput(ptr: number, maxSamples: number): number;
  // Pushes `count` int16 mic samples from the WASM heap at `ptr`.
  writeAudioInput(ptr: number, count: number): void;
  setHostAudioEnabled(speaker: boolean, mic: boolean): void;
  _malloc(size: number): number;
  _free(ptr: number): void;
  HEAPU8: Uint8Array;
}
