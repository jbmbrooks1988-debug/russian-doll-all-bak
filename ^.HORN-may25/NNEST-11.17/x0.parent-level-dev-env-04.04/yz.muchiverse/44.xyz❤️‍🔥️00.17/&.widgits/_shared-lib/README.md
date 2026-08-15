# _shared-lib

Single canonical source for small files that were hand-duplicated,
byte-identical, across multiple widget `ops/` dirs (found + confirmed
via md5sum during the 2026-08-12 duplication-inventory pass):

- `khtpm_css_parser.c` / `.h` — the `.chtpm` stylesheet parser, was a
  hand copy in both `*.monads/*.livedesk-taskbar/ops/` (used by
  khtpm_strip_parser AND khtpm_hq_render/db-hq) and
  `&.widgits/events-hq/ops/`.
- `stb_image_write.h` — same two consumers, PNG dump support.

**Convention (matches this house's existing pattern, not new):** this
is a source-of-truth directory, not a shared runtime include path.
Each consumer's own `build.sh`/`build_*.sh` copies these files into
its own `ops/` dir as a build step BEFORE compiling — the exact same
shape `&.widgits/tile-picker/scripts/build.sh` already used for
copying `prisc+x`/`chtpm_parser_pal`/etc. from `014.wsr-pal/system/`.
This is deliberate, not an oversight: `xyz-installer-dev/dev-doc/
04.harnecient-fresh-install-design.md` copies each widget's `ops/` dir
as its own **self-contained** subtree (§5.1) — a real shared *runtime*
include path would mean two independent top-level install units
(`*.monads/*.livedesk-taskbar/` and `&.widgits/events-hq/`) reaching
across each other via a relative path, which is the exact
`!.HOUSE_STDS.md` #20 hardcoded/relative-path-fragility class this
house has already been burned by more than once. Sync-then-compile
keeps every built `ops/` dir self-contained post-build, so the
installer needs ZERO changes to know about this directory - the files
are just already sitting in each consumer's own tree by the time
anything gets copied or zipped.

**If you add a new consumer**: add a `cp "$SHARED"/... ops/` line to
its build script (see `build_khtpm_strip.sh`/`build_events_hq.sh` for
the pattern), don't hand-copy the file again.

**If `_shared-lib` itself needs to be installable** (e.g. a future
widget that ships without ever being dev-built first): flag it as a
Phase-1 installer follow-up in `04.harnecient-fresh-install-design.md`
§10 — not needed today since every current consumer builds from dev
source before packaging.
