# CLAUDE.md

Working notes for Claude Code in this repo.

## What this is
NA Encyclopedia by Noveltee Audio: beginner teaching plugins, one per mixing tool. Current volume: **Vol. IV, Compression**. JUCE 8 + CMake, AU first, VST3 too. Read `README.md` first.

## Read before working on…
- Anything visual → `docs/UI-REFERENCE.md`
- Metrics, feedback or chapter logic → `docs/FEEDBACK-ENGINE.md`
- Copy, targets, instruments, goals → `content/compression.json`

## Hard rules
- **No user-facing text in C++.** Every string comes from the content JSON. Add new strings there.
- **Audio thread:** no allocation, no locks, no logging, no file I/O. Use lock-free FIFOs to hand data to analysis.
- **Shared vs volume code:** anything another volume could reuse (book frame, knob, meter, analysis, chapter engine, content loader) goes in `shared/`. Only compressor-specific code goes in `volumes/04-compression/`.
- **Don't change target numbers in the content file** unless asked. They're under review.
- Keep the build green on GitHub Actions. The owner tests by downloading artifacts onto a Mac running Logic, so every push to `main` should produce a working AU.
- Work milestone by milestone (README → Milestones). Don't start the book UI before the compressor and metrics work.

## Owner context
- Tests in Logic Pro on macOS. Works browser-only through Claude Code on the web and GitHub.
- Not a C++ developer. Explain build failures and testing steps in plain English, with exact commands to run on the Mac.
