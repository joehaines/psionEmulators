# App-library source (3-Lib)

The categorised software collection from Steve Litchfield's **3-Lib**
shareware library CD-ROM, preserved here as the source material for the
site's app library. Each `<category>/` directory holds one folder per
app (the original files, untouched); each `<category>.htm` is the CD's
own catalogue page, whose `<PRE>` index supplies the app names, dates
and descriptions. `3LIB-README.txt` is the CD's original introduction.

`scripts/build-app-library.mts` turns all of this into the deployable
library (`dist/apps`: `manifest.json` + one zip per app + icons
extracted from EPOC `.AIF` resources) — see that script's header for
usage, and the "App library" section of the top-level README for how
the site consumes it. CI regenerates the output on every deploy;
nothing under `dist/` is committed.

The software here is shareware or freeware by its original authors,
preserved for historical interest. If you are an author and would like
something amended or removed, please open an issue.
