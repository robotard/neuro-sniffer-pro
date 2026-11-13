# ADR-016 — Git Is the Source of Truth, Not the Build Script

- Status: Accepted
- Date: 2026-08-21
- Domains affected: SW-0.2.0, DOC
- Tags: [build, git, architecture, source-of-truth]

## Context

The original `BUILD_REPO.sh` model embedded every file's content as
heredocs inside the shell script itself. This was inherited from the
project's earliest (C6/RP2040-era) uploads and carried forward
unquestioned. It has two structural problems: (1) it hits a hard byte
limit on any tool that assembles the script in one shot, and (2) it
creates two sources of truth — the script's embedded content and the
actual output files — that can silently drift apart. The reconciliation
pass in `docs/BUILD_SCRIPT_RECONCILIATION.md` found exactly this drift:
old scripts referencing C6 and RP2040 long after both were superseded.

Now that `github.com/robotard/neuro-sniffer-pro` is a real, live repo,
that duplication is no longer necessary.

## Decision

Git is the single source of truth. `BUILD_REPO.sh` and `BUILD_REPO_MAC.sh`
do exactly two things: clone (or pull) the real repo, then install the
local toolchain (ESP-IDF v5.5.2+, Pico SDK 2.0+). No file content is
embedded in either script.

## Alternatives Considered

### Keep embedding content, split into sub-scripts (SW-004 original plan)
Was the plan before GitHub push existed. Solves the byte-limit problem
but not the drift problem — two sources of truth still exist, just
spread across more files. Superseded by this ADR.

### No build script at all, just `git clone` in the README
Simpler still. Rejected only because the toolchain install step
(ESP-IDF + Pico SDK, ~10 minutes, several gotchas) is worth automating.

## Consequences

### Positive
- One source of truth. Editing a file means editing it once.
- `BUILD_REPO.sh` shrank from 1019+ lines to 41.
- No byte-limit risk, ever, regardless of project size.

### Negative
- Requires network access to GitHub at setup time (was already implicitly
  true — the old scripts also needed network access to clone ESP-IDF/
  Pico SDK, this just adds one more clone).

## References
- docs/BUILD_SCRIPT_RECONCILIATION.md
- SW-004 (Trello) — superseded by this decision
