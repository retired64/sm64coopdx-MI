# DJUI overflow and MI language fallback

Follow-up to the first catalog preview, 2026-10-29. Base remains
ManIsCat2/android at 99c8d21abe4d24649b508d0f216c4359ed5d2a7a; no android-dev
or newtouch merge was performed.

## Reports and causes

The user confirmed that resetting the existing game settings restored the touch
controls. The source inspection found saved touch sizes of 10 while this base's
button/stick editor allows 1–2. No touch-control patch or external settings repair
was applied by MI; the user performed the reset.

The user then reported a button labelled TITLE and lower controls disappearing
when increasing DJUI scale. These are separate issues:

- The native language loader reads the external game/lang/*.ini files. The
  installed files predate MI. The Android asset copier preserves existing files,
  so packaging new INIs does not update those files. Missing keys fall through
  to the literal key, including TITLE.
- Three-panel rendering clamps body height to the viewport but previously had
  no mechanism to reach the remainder of a tall standard menu.

## Changes

### MI labels on an existing game installation

All MI UI calls use mi_catalog_text. External translations are honored when the
native loader supplies them; otherwise the embedded EN/ES/PT table provides the
labels. Unknown languages use English. The table is generated from the existing
MI_CATALOG sections by tools/mi/embed_catalog.py, which also checks fixture drift.

No external language file, user configuration, mod or save is overwritten.
Language selection is read on lookup; there is no separate persistent locale
cache. This fallback only covers the new MI keys, not every historical game key.

### Reusable standard-menu overflow

Panels created by djui_panel_menu_create opt into vertical scrolling. Existing
custom three-panels opt out by default. The selected persistent DJUI scale is
preserved; content is not globally shrunk to defeat that selection.

The standard vertical body measures visible absolute-height entries each frame,
reserves room for ^/v navigation when necessary, and clips content to its body
viewport. The offset is owned by that panel, clamped after viewport/content
changes and destroyed with it. Touch/mouse can use the arrows; wheel input falls
back to the menu when the focused control has no wheel handler. Controller cursor
selection requests that its body element be exposed. This does not change the
existing page navigation of the game's local Mods panel or MI's result pages.

Geometry lives in djui_panel_layout.h so the layout and offset limits can be
regression tested independently of rendering. The main integration points are
three_panel, panel_menu, cursor and interactable; record these in upstream review.
No network, mod activation or touch-gameplay code was changed.

## Automated validation

```sh
python tools/mi/embed_catalog.py --check
python tools/mi/test_catalog.py
env LD_LIBRARY_PATH=/data/data/com.termux/files/usr/lib make -j$(nproc)
git diff --check
```

The native test command includes:

- Existing JSON/identity/search tests.
- Missing external MI translations in EN/ES/PT, English fallback and a custom
  external override, using the production lookup code with a stub native loader.
- Viewport geometry at heights 480/720/1080 and scales 0.5/0.75/1/1.5, bounds,
  lower-action reachability, short menus and custom-panel opt-out.

The explicit LD_LIBRARY_PATH is a per-command workaround for this Codex session's
vendor library conflict with Termux zipalign, not a system environment change.

## Device regression matrix — pending for this patch

- Keep the existing external language files: verify Explore mods no longer says
  TITLE; check list, details, error and disabled Install labels in EN/ES/PT.
- Increase scale to Large in both centered/non-centered themes. Open the game's
  Mods menu and MI list/details; use ^/v to reach lower actions and Back.
- Exercise mouse wheel where available and controller selection past the viewport.
- Check return from details, open/back repeatedly and close/reopen the menus.
- Change scale back to default: controls should disappear when content fits.
- Search/filter to fewer results: scrolling clamps and hidden entries do not
  remain active. Change catalog page and recheck selection/details.
- Check the existing game settings, language menu, local mod selection and custom
  touch editor; custom panels should keep their original layout behavior.

Compilation and pure geometry tests do not establish that every theme, device,
controller or nested layout renders correctly; physical validation is required.


## Build result — 2026-10-29

Native catalog, language fallback and viewport tests passed. Termux Makefile
compilation completed; APK signature, 4 KiB alignment and ZIP CRC verified.
The ARM64 library inside the APK matches build/us_pc/libmain.so.

- Artifact: build/us_pc/sm64coopdx.apk.
- Bytes: 119908665.
- SHA-256: 7fb291e742f3daaecac48151d3965e4bb9bfb014937bd9da28033ecc8939920b.
- Device validation of this patch remains pending.
- No commit, branch merge or release was created.

## Verificación de sincronización de estado

EN/ES/PT MI_CATALOG sections write the source labels; the checked generator
produces the embedded fallback and its --check detects drift. DJUI reads the
native language selection on lookup and honors available external labels.
No external language/config files were rewritten or locale cache added.

Each standard panel owns its offset and controls; viewport/content dimensions
reconcile bounds every frame. The controller requests visibility through that
same state. The panel tree destroys controls and state at teardown. No timers,
subscriptions, background jobs or unawaited Futures were introduced.
Reviewed siblings: existing Mods panel, MI list/details, standard settings and
language menus; custom three-panels opt out by default. Physical regression of
those screens is still required.


### Device follow-up — 2026-10-29

The user confirmed that the updated APK worked on their phone after the TITLE
and enlarged-scale clipping fixes. This records general physical confirmation;
individual language/theme/controller matrix results have not been supplied.
Earlier pending-device notes describe the state before this confirmation.
