# ROMs

This directory follows the layout of
[explit28/Psion-ROM](https://github.com/explit28/Psion-ROM) —
`<Model>/<build>/<image>` — and carries a copy of every ROM image from
there (the upstream README is kept as
[Psion-ROM-README.md](Psion-ROM-README.md), with its per-build
`readme.txt` files alongside the images). Copied from upstream commit
`58891d6f` (2026-09-02). The upstream `Tools/` folder (PsiROM, patch
utilities, SIS packages) is not copied: it holds no ROM images.

## Which image a device boots

- **Default:** each device's `romFilename` in `core/device_registry.cpp`.
  These are the images the site has always shipped.
- **Alternatives:** `frontend/src/lib/romCatalog.ts` lists the other builds
  a device can boot, chosen per device under **Settings → ROM versions**.
  Every image listed there reached a live screen under the native harness's
  boot gate. For the 5mx Pro and the netBook the choice is the OS image the
  site writes onto the boot card.
- `tests/unit/rom-catalog-sync.mts` checks the registry, the catalogue,
  the desktop app's bundle list and the files here against each other.
- **What goes on the website:** only the images the site serves (the
  defaults, the Settings choices and the fixed pack/OS images, ~650 MB),
  staged by `scripts/stage-deploy-roms.mts` and mirrored to the server's
  `roms/` by the deploy workflow, which also removes anything there the
  site no longer serves. The rest of this directory stays in git.

## Local additions (not in Psion-ROM)

| Folder | What it is |
|---|---|
| `Organiser1/Organiser1_eng/` | Organiser I mask ROM (MAME `psion1`) |
| `OrganiserII/OrganiserII_LZ64_eng/` | Organiser II LZ64 ROM |
| `MC400/MC400_v2.60F_eng/` | MC400 v2.60F boot ROM, the ROM:: System Disk pack and its unpacked files (`ROM_disk/`) |
| `MC400/MC400_v1.26F_eng/` | MC400 v1.26F boot ROM (the `mc400v126` profile) |
| `MC200/MC200_v2.12F_eng/` | MC200 ROM (built from the two chip dumps in `ROM_disk/` by `scripts/build-mc200-rom.mts`) and its System Disk |
| `HC120/hc120_v1.72F_eng/` | HC120 ROM and the two chip dumps it is joined from (`scripts/build-hc120-rom.mts`) |
| `Conan/Conan_v0.10(17)_eng/` | The Conan ROM dumped with `tools/romdump` |
| `netPad/netPad_v1.75(247)_eng/` | English netpad image |
| `Series5mxPRO/5mxPRO_v1.05(319)_patch_site_eng/` | The 5mx Pro OS this site has always shipped; differs in a few KB from upstream's `5mxPRO_v1.05(319)_patch_eng` |
| `Series5mxPRO/ESHELL_v1.06_eng/` | 5mx Pro ESHELL test OS (easter-egg boot) |
| `netBook/Patched/netBook_v1.05(450)_patch_site_eng/` | The netBook OS this site has always shipped; differs from upstream's `patch2` build |
| `netBook/ESHELL_v0.01_eng/` | netBook ESHELL test OS (easter-egg boot) |
| `netBook/Quartz_v6.0_eng/` | netBook build of Quartz (`docs/netbook-quartz.md`) |
| `netBook/Quartz_v6.0_b_eng/` | A second, larger Quartz image; not currently used |

Name note: the Series 5 image this site used to call `series5_v1.01(144)_eng.bin`
is byte-identical to upstream's `S5_v1.01(145)_eng`, whose header says
1.01(145); it now lives there.

## Images that do not boot here

In this directory but not offered in Settings — they fail the boot gate on
the device's current emulation:

- `Series3a/s3a_v3.22f_eng` and `Workabout/w1_v1.00f_eng` — 1 MB images for
  machines emulated with a 2 MB ROM (the 3a v3.22f is still used by
  `scripts/build-mame-3a.sh`).
- `Series3/s3_v1.77f_eng` — two chip dumps (256 KB + 128 KB) that would need
  joining.
- `Series5/S5_v1.00(113)_eng` — the Series 5 prototype.
- `netPad/netPad_v1.40(174)_ger` — a different image layout from the
  English build.
- `Series5mxPRO/BootLoader/5mxPRO_BL_v1.08_ger` and the `Series7/Updates`
  images are not selectable either.
