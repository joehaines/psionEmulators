# App-library source (3-Lib)

The categorised software collection from Steve Litchfield's **3-Lib**
shareware library CD-ROM, preserved here as the source material for the
site's app library. Each `<category>/` directory holds one folder per
app (the original files, untouched).

`catalogue.json` is the CD's own index — one array per category, in the
CD's order, giving each app's name, date (in the CD's `DD/MM/YY`
dialect), description and the section heading it appeared under. It was
extracted once from the `<PRE>` block of the CD's HTML catalogue pages,
which the build used to parse directly; those pages are no longer kept,
so this file is the source of truth for everything that shipped on the
CD.

`extra-apps.json` catalogues anything added to the library since the
CD. A new app gets its folder under the relevant `<category>/` plus an
entry in that file (name, date, description, and optional per-app
device / `tryDevice` overrides). An entry can also name SIS installers
elsewhere in the library whose payloads belong in its bundle — how
**Wall (installed)**, which is zExe-compressed, picks up the loader DLLs
from `epocutil/zexe` that its stub needs at runtime.

`netpad/` is the one category absent from `catalogue.json`: it holds the
applications Psion Teklogix shipped on the netpad's support CD (the
machine's EPOC R5 ROM carries none of them), so every entry lives in
`extra-apps.json` instead. The emulator links to it from the netpad's
**Install standard apps** button, which opens the library at
`#/apps?device=netpad&category=Standard+apps`.

That category is the netpad's *own* set, not the limit of what it runs:
the machine is an EPOC R5 ARM device with the 5mx's own 640×240 panel,
so every `epoc*` category is catalogued for it as well and installs on
it by the same MMC route (see `EPOC_DEVICES` in
`scripts/build-app-library.mts`). The button names the category so those
twenty apps aren't buried among the thousand the machine can also run;
clearing the category filter in the library shows the rest.

`scripts/build-app-library.mts` turns all of this into the deployable
library (`dist/apps`: `manifest.json` + one zip per app + icons
extracted from EPOC `.AIF` resources) — see that script's header for
usage, and the "App library" section of the top-level README for how
the site consumes it. CI regenerates the output on every deploy;
nothing under `dist/` is committed.

The software here is shareware or freeware by its original authors,
preserved for historical interest. If you are an author and would like
something amended or removed, please open an issue.
