# MI catalog preview — roadmap stage 1

Base: ManIsCat2/android, commit 99c8d21abe4d24649b508d0f216c4359ed5d2a7a.

## What is implemented

The existing Browse Mods entry opens a DJUI catalog panel instead of the website.
It remains available only in the original offline context (`NT_NONE`). The local
mod list, discovery, refresh and activation rules are unchanged.

The panel reads an embedded schema-2 fixture containing eight synthetic mods.
It supports five results per page, search across title/author/category/description,
and details. Search folds ASCII case and preserves UTF-8; accent folding and
Unicode case folding are not implemented. No network call, game configuration
write, mod installation or receipt is performed. Install is disabled and the
panel identifies the sample/preview explicitly.

Catalog source: tests/mi/catalog_sample.json. It follows the published JSON
shape (catalog identity/status/count, mod IDs, version IDs and file IDs). The
sample uses example.invalid URLs and cannot be downloaded. UI strings are
localized in English, Spanish and Portuguese, including embedded fallback labels
for existing installations with old external INIs; other languages fall back to English. Sample content is English and explicitly synthetic.

## Boundaries and ownership

- `src/pc/mod_integrations/mi_catalog.cpp`: bounded, immutable display projection
  using the existing vendored nlohmann/json. Schema 2 only, complete catalogs,
  stable unique mod IDs; malformed inputs never expose partial results.
- `mi_catalog.h`: C interface, indexed immutable entries. The DTO is display-only;
  version/file ownership and download selection are intentionally not implemented.
- `mi_catalog_panel.c`: DJUI list/details, per-panel snapshot/search/page ownership.
  Its destructor releases the snapshot. There are no threads, timers, subscriptions
  or asynchronous callbacks. Opening details does not mutate catalog/game state.
- `djui_panel_host_mods.c`: small entry/return adapter. The existing separate tooltip
  panel is hidden while browsing and restored on return; no scanner/selection fork.
- Makefile and Android.mk: include the new source directory for both build paths.

The preview uses its own page controls because the upstream pagination helper
stores control pointers globally while the parent Mods screen stays alive.
The initial preview did not change generic DJUI behavior. The subsequent
viewport fix adds opt-in scrolling to standard menus; see
[DJUI_VIEWPORT_AND_TRANSLATIONS.md](DJUI_VIEWPORT_AND_TRANSLATIONS.md). Parent search/page and selection remain owned
by the existing screen; the MI snapshot and search are released when MI closes.

Display text is bounded, UTF-8-safe at truncation boundaries and cannot inject
DJUI color/alpha commands. Catalog input is limited to 8 MiB, 5000 entries and
32 nesting levels. The parser checks catalog ID syntax, not its content hash:
content-hash verification is still required when implementing remote/cache trust.
Version/file parsing and full production validation are future work, not implied
by successful display parsing. Nothing in this stage certifies gameplay compatibility.

## Build and checks

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
make -j$(nproc)
git diff --check
```

After editing the JSON fixture, regenerate its checked-in raw-string include:

```sh
python tools/mi/embed_catalog.py
```

The standalone C++ tests need no ROM, SDL or Android UI. They cover malformed,
partial/future schema, counts, duplicate/oversized identities, missing optional
metadata, empty results, search across pages, DTO lifetime and UTF-8 display bounds.
The embedded sample is checked against the source fixture to prevent drift.

## Device acceptance — pending until explicitly tested

1. From the existing Mods screen, open Explore mods; no browser should open.
2. Confirm preview notice and first page; next/previous should reach all 8 entries.
3. Search `UTILITY` (two results), `fixture team` (all), `standalone` (one), and
   a nonexistent term (empty results). Filter from page 2 and verify page resets.
4. Open details from a filtered result and verify correct title/metadata; Install
   stays disabled. Back returns to the same query/page.
5. Return to the existing Mods screen: tooltip, filtering, pagination and selection
   should still work. Repeat open/back and close/reopen the entire menu.
6. Test touch and controller, English/Spanish/Portuguese, supported themes and
   narrow landscape layouts. UI layout/interaction cannot be certified by compilation.
7. No new mods, receipts or selection/config changes should result from browsing.

Next roadmap stage: remote catalog fetch and last-valid cache, off the UI thread.
Installation Lua/ZIP with receipts/cancellation follows; not part of this preview.


## Validation recorded — 2026-10-29

- `python tools/mi/test_catalog.py`: passed with Clang 21.1.8 and C++17.
- JSON contract validation of the fixture: passed (8 mods, 8 versions, 8 files).
- Native game C/C++ compilation and APK packaging: passed through the Termux Makefile.
- APK signature verification, 4 KiB alignment and ZIP CRC checks: passed.
- APK ARM64 libmain.so matches the built library; EN/ES/PT MI strings are packaged.
- `git diff --check`: passed. Device UI acceptance remains pending.

Artifact: `build/us_pc/sm64coopdx.apk`, 119888185 bytes.
SHA-256: `87fc1a866998c79f58ada237fbf1c829484b941a89080eabb4331750477e35ef`.
This is a local preview on the recorded upstream base plus uncommitted MI changes;
no release, tag or commit has been created. Android.mk source inclusion was
updated, but the separate Gradle build was not executed.

The Codex session supplied a vendor C++ library through LD_LIBRARY_PATH that
prevented Termux zipalign from starting. For this session only, the successful
build used the installed Termux libraries:

```sh
env LD_LIBRARY_PATH=/data/data/com.termux/files/usr/lib make -j$(nproc)
```

No packages or global environment settings were changed. The inherited Makefile
also reports duplicate build/us_pc/ recipe warnings; they did not prevent the
successful build and were not changed as part of the catalog preview.


### Device follow-up — 2026-10-29

The user confirmed that the updated APK worked on their phone after the TITLE
and enlarged-scale clipping fixes. This records general physical confirmation;
individual language/theme/controller matrix results have not been supplied.
Earlier pending-device notes describe the state before this confirmation.


## Superseded by stage 2

This document records the historical embedded preview. The current panel loads
the remote catalog with asynchronous requests and a persistent last-valid cache.
See [CATALOG_REMOTE.md](CATALOG_REMOTE.md) for current limits, trust boundaries,
ownership and checks. The fixture remains only for regression tests.
