# SQLite native dependency

SQLite 3.54.1 amalgamation, obtained from the official distribution:
https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip

Archive SHA3-256:
92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30

Upstream source files are unmodified. Their headers contain the public-domain
notice. See https://www.sqlite.org/copyright.html .

The MI wrapper at src/pc/mod_integrations/mi_sqlite.c disables loadable extensions
and compiles this dependency directly for both Makefile and Android.mk. Native
contract tests compile that same wrapper. No Android private SQLite API or
preinstalled shared library is needed by the game.

Review source provenance, release notes, binary size, both build paths and catalog
regression tests before updating the pinned version. Do not fetch source at runtime.

The scoped .gitattributes whitespace exception preserves upstream trailing spaces
in sqlite3.c; whitespace checks remain enabled for project-owned code.
