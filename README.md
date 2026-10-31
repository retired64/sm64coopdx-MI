# SM64coopdx-MI

![sm64coopdx Logo](textures/segment2/custom_coopdx_logo.rgba32.png)

**Mods Integration for the Android port of sm64coopdx.** Browse a mod catalog,
install mods, and update them directly from the game, using a native interface.

This is an independent fork maintained by [retired64](https://github.com/retired64),
based on [ManIsCat2/sm64coopdx's stable `android` branch](https://github.com/ManIsCat2/sm64coopdx/tree/android).
MI development takes place on **`mi-main`**. It is not an official release of the
upstream Android port or the Coop Deluxe Team.

SM64coopdx-MI is developed and maintained by **retired64**, with AI assistance
for implementation, documentation, and testing. Project direction, review, and
release decisions remain with the maintainer.

[Releases](https://github.com/retired64/sm64coopdx-MI/releases) ·
[Build artifacts](https://github.com/retired64/sm64coopdx-MI/actions/workflows/mi-actions-build.yml) ·
[Report an MI issue](https://github.com/retired64/sm64coopdx-MI/issues) ·
[Project documentation](mi-docs/README.md)

## What MI adds

- An in-game catalog with search, pagination, mod details, and thumbnails.
- A readable, responsive layout that respects the game's menu scale.
- English, Spanish, and Portuguese MI labels, with compiled fallback translations.
- A remote JSON catalog with a validated local cache for offline browsing.
- Direct installation of standalone Lua files and ZIP packages, with real
  download/install progress and cancellation before replacement begins.
- Update checks for mods associated through MI installation receipts.
- Individual updates that preserve the installed destination, check local
  changes, and use a backup and transaction journal for rollback and recovery.

**The game still discovers, refreshes, activates, and runs mods.** MI delivers
their files; it does not add a second activation system or guarantee gameplay
compatibility. Downloaded archives are temporary installation inputs.

Individual replacement and recovery have automated local test coverage;
the latest changes still need the full physical-device validation described in
[the replacement guide](mi-docs/CATALOG_REPLACEMENT.md).

## Using the catalog

1. Launch the game and open **Mods**.
2. Open the catalog from the Mods menu header.
3. Search or browse, open a mod, choose its available file, and install it.
4. Return to the game's Mods screen, refresh its list, and use its usual controls
   to activate the mod.

Use the catalog's update check to look for newer versions of MI-managed mods.
Updates are explicit user actions. MI checks receipts and file hashes rather
than relying on a version label inside `main.lua`. Local edits or conflicting
files can block replacement to protect existing content.

After an interrupted installation or update, open the catalog to let MI reconcile
pending transactions before refreshing the local mod list. If recovery encounters
unexpected files or modified backups, it preserves evidence and reports the
problem instead of forcing an overwrite.

Currently supported installation inputs are **ZIP and standalone Lua**.
RAR/7z extraction, manual association of existing mods, Update all, a database
catalog format, and a standalone desktop application remain future work.

## APKs, builds, and future releases

The [MI Android APK workflow](.github/workflows/mi-actions-build.yml) currently
runs manually and uploads an **ARM64 test APK**, build information, and checksums
as Actions artifacts. Artifacts expire after seven days. It does not publish
GitHub Releases or run automatically on each push.

The [Releases page](https://github.com/retired64/sm64coopdx-MI/releases) is the
intended destination for future published MI builds; this README does not imply
that a release is already available. Automated release publishing is planned.

Current test APKs retain the inherited application identity and **public test
signing key**. They do not yet use an exclusive MI production identity. Do not
assume they install alongside another build or can update it: Android requires
compatible package identity and signing certificates. Back up your data before
switching builds. A release signing/versioning policy must be established before
automating public releases.

The CI build does not download or require a ROM. On a fresh installation, the game
asks you to provide your own supported ROM. No ROM download is provided here.
See [the build and signing guide](mi-docs/MI_ACTIONS_BUILD.md) for exact behavior.

## Build from source

Clone the MI development branch:

```sh
git clone --branch mi-main https://github.com/retired64/sm64coopdx-MI.git
cd sm64coopdx-MI
```

For the locally tested Termux build, install the required packages:

```sh
pkg install x11-repo
pkg install git wget make python getconf zip apksigner clang binutils libglvnd-dev aapt libandroid-execinfo which sdl2
make -j$(nproc)
```

The local APK output is `build/us_pc/sm64coopdx.apk`. Package conflicts and
environment-specific workarounds are documented in
[the maintenance guide](mi-docs/MANTENER-UPDATES.md); review them before changing
your installed packages. Local build prerequisites and CI's ROM-free build path
are documented separately and should not be assumed interchangeable.

To build in your own GitHub fork, enable Actions and authenticate GitHub CLI
with an account allowed to run workflows in that repository. From your local
clone, run:

```sh
gh workflow run mi-actions-build.yml --ref mi-main
gh run list --workflow mi-actions-build.yml --branch mi-main --limit 5
```

GitHub CLI uses the repository associated with the current clone. Check it with
`gh repo view`; if it points to upstream instead of your fork, set your fork with
`gh repo set-default OWNER/REPOSITORY` (replace the placeholder with your own).
If you renamed the development branch, replace `mi-main` in both commands.
The workflow must also exist on your repository's default branch for manual
execution to be available. These commands are for contributors building an APK;
players downloading a published build do not need GitHub CLI.

## Development and maintenance

Start with [AGENTS.md](AGENTS.md) and [the documentation index](mi-docs/README.md).
Most engineering notes are currently written in Spanish.

- [Rules and integration contracts](mi-docs/RULES.md).
- [Upstream setup](mi-docs/UPSTREAM_SETUP.md) and
  [stable Android synchronization](mi-docs/MANTENER-UPDATES.md).
- [Installation](mi-docs/CATALOG_INSTALLATION.md),
  [update detection](mi-docs/CATALOG_UPDATES.md), and
  [replacement, rollback, and recovery](mi-docs/CATALOG_REPLACEMENT.md).
- [Accessibility](mi-docs/CATALOG_ACCESSIBILITY.md) and
  [thumbnail handling](mi-docs/CATALOG_THUMBNAILS.md).
- [Maintenance tests](mi-docs/MI_MAINTENANCE_TESTS.md).
- [Future native Linux/Windows application plan](mi-docs/NATIVE_PORTABILITY_PLAN.md).

MI keeps its implementation concentrated in `src/pc/mod_integrations`, with tests
under `tests/mi` and tooling under `tools/mi`. Upstream integration follows the
stable Android branch; a conflict-free merge still requires semantic review,
build verification, and device testing.

When reporting an issue, include the commit or APK source, Android version,
reproduction steps, and relevant logs. Distinguish catalog/download/install
failures from errors produced when the game executes a mod. Remove personal data
and credentials before sharing logs.

## Upstream project and credits

sm64coopdx is an online multiplayer project for the Super Mario 64 PC port,
started by the **Coop Deluxe Team**, continuing **djoslin0's sm64ex-coop**. It
synchronizes entities and levels and provides customization and a Lua mod API.
MI builds on that work and the Android port; original authors retain credit for
their contributions. Existing source notices and dependency licenses still apply.

- [Android upstream and wiki](https://github.com/ManIsCat2/sm64coopdx/wiki).
- [Lua API documentation](docs/lua/lua.md).
- [C documentation](docs/c/c.md).
- [Upstream community Discord](https://discord.gg/TJVKHS4) — upstream community,
  not a dedicated MI support channel.

The inherited project contains code from:

- [AloXado320/sm64ex-alo](https://github.com/AloXado320/sm64ex-alo).
- [VDavid003/sm64-port-android](https://github.com/VDavid003/sm64-port-android).
- [VDavid003/sm64-port-android-base](https://github.com/VDavid003/sm64-port-android-base).
- [porcino/sm64-port-android](https://github.com/porcino/sm64-port-android).
- [robertkirkman/sm64ex-coop](https://github.com/robertkirkman/sm64ex-coop).
