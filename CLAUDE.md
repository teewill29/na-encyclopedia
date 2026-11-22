# CLAUDE.md

Working notes for Claude Code in this repo.

## What this is
NA Encyclopedia by Noveltee Audio: beginner teaching plugins, one per mixing tool. Built: **Vol. IV, Compression** (Milestones 1–7). In development: **Vol. I, Gain Staging** (`docs/GAIN-STAGING.md`). Both run on the shared book in `shared/book`. JUCE 8 + CMake, AU first, VST3 too. Read `README.md` first.

## Read before working on…
- Anything visual → `docs/UI-REFERENCE.md`
- Metrics, feedback or chapter logic → `docs/FEEDBACK-ENGINE.md`
- Copy, targets, instruments, goals → `content/compression.json` (Vol. IV), `content/gain-staging.json` (Vol. I)
- Vol. I → `docs/GAIN-STAGING.md`

## Hard rules
- **No user-facing text in C++.** Every string comes from the content JSON. Add new strings there.
- **Audio thread:** no allocation, no locks, no logging, no file I/O. Use lock-free FIFOs to hand data to analysis.
- **Shared vs volume code:** anything another volume could reuse (book frame, knob, meter, analysis, chapter engine, content loader) goes in `shared/`. Only compressor-specific code goes in `volumes/04-compression/`.
- **Don't change target numbers in the content file** unless asked. They're under review.
- Keep the build green on GitHub Actions. The owner tests by downloading the `.pkg` artifact onto a Mac running Logic.
- **Branches:** work on one working branch; every push builds there. At each checkpoint give the owner a direct artifact link (`https://github.com/teewill29/na-encyclopedia/actions/runs/<run-id>/artifacts/<artifact-id>`) plus the run page, and say exactly what to test. Merge into `main` only after the owner has tested it in Logic and said so. `main` only holds tested builds.
- **Version:** bump the patch version (`NA_COMPRESSION_VERSION` / `NA_GAINSTAGING_VERSION` in each volume's CMakeLists.txt) for every build handed over for testing, for each volume whose code changed. Logic caches name and validation per version.
- Work milestone by milestone (README → Milestones). Don't start the book UI before the compressor and metrics work.
- **Cloth = plate:** each volume's cloth boards and its spine label plate are the same colour (one `book.cloth` per content file; UI-REFERENCE §8). The plate is a flat fill, never textured.
- **Licensing:** every Noveltee plugin will need its own licence/unlock before it is sold (approach not chosen yet). Owner's requirements so far: **one licence per computer**, and **multi-seat sales** (e.g. a school buying 20 licences in one purchase). When it's built, it belongs in `shared/` so every volume uses the same system. Also check the JUCE licence terms then.

## Owner context
- Tests in Logic Pro on macOS. Works browser-only through Claude Code on the web and GitHub.
- Not a C++ developer. Explain build failures and testing steps in plain English, with exact commands to run on the Mac.
