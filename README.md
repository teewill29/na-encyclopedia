# NA Encyclopedia

**by Noveltee Audio**

A series of beginner plugins that teach the basics of mixing while you use them. Each volume covers one tool. You pick your instrument and the sound you're after, and the plugin walks you through it chapter by chapter, listening to your audio and telling you when you're on target.

It's the cheat-sheet book, built into the plugin.

**First volume in development: Vol. IV, Compression (First Edition).**

---

## How it works

1. **Load it** on a track.
2. **Set the scene:** pick the instrument (Lead Vocal, Bass, Kick…) and the goal (More Punch, Smooth & Even…).
3. **Follow the chapters.** Each one focuses on one control, explains it in plain English, and says what to listen for.
4. **Get live feedback.** The Reading panel measures what the compressor is doing to your audio and tells you if you're too fast, too slow or right on target.
5. **Compare honestly.** Level-matched A/B, plus "Hear what's removed" so you can hear exactly what the compressor is taking away.

### Compression chapters

| | Chapter | Control | Feedback |
|---|---|---|---|
| I | The Goal | none | Does this track even need compression? |
| II | Ratio & Threshold | Threshold (ratio auto-set by goal) | Gain reduction on the loud parts |
| III | Attack | Attack | How much of each note's front survives |
| IV | Release | Release | Does it recover before the next note? |
| V | Level & Compare | Makeup Gain | Output matched to input loudness |
| VI | Blend | Mix | Parallel goals only |

### Coverage (V1)

12 instruments, 25 goals. Full list with target ranges in [`content/compression.json`](content/compression.json).

Lead Vocal · Backing Vocals · Acoustic Guitar · Electric Guitar (Clean) · Electric Guitar (Distorted) · Bass · Kick · Snare · Drum Bus · Piano / Keys · Mix Bus · Spoken Voice / Podcast

> **Content status: DRAFT.** Every number is a standard starting point and must be checked against 2–3 sources and reviewed by an experienced engineer before release.

---

## The series

| Vol. | Topic | Status |
|---|---|---|
| I | Gain Staging | Planned |
| II | Balance & Panning | Planned |
| III | EQ | Planned |
| **IV** | **Compression** | **In development** |
| V | De-essing | Planned |
| VI | Gating | Planned |
| VII | Reverb | Planned |
| VIII | Delay | Planned |
| IX | Saturation | Planned |
| X | Limiting | Planned |

- **Volume number** = place in the set (learning order). Fixed forever.
- **Edition** = version. First Edition, Second Edition, and so on.
- Volumes can ship in any order.

---

## Tech stack

- **JUCE 8**, C++20, **CMake**
- Formats: **AU** first (testing in Logic Pro), **VST3** from the same build. Windows VST3 after the Mac version is solid.
- macOS universal binary (Apple Silicon + Intel), macOS 12+
- Builds on **GitHub Actions** (`macos-15` runner); artifacts are downloaded and tested in Logic on the home Mac
- Installer: `pkgbuild` / `productbuild` `.pkg` that drops the AU in `/Library/Audio/Plug-Ins/Components` and the VST3 in `/Library/Audio/Plug-Ins/VST3` (all users, same as the other Noveltee Audio plugins)
- Channel layouts: mono→mono, mono→stereo, stereo→stereo (Logic hides plugins that can't run on mono tracks)
- Fonts: Cinzel and EB Garamond (both OFL), embedded via `BinaryData`

### Plugin identity

| Field | Value |
|---|---|
| Company | Noveltee Audio |
| Manufacturer code | `Tywl` (shared by every Noveltee Audio plugin, so they sit together in Logic) |
| Product name | NA Encyclopedia IV Compression |
| Plugin code | `NE04` |
| Bundle ID | `com.novelteeaudio.naencyclopedia.compression` |
| Version | 1.0.x (First Edition). Bump the patch number for every build handed to Logic: Logic caches name and validation per version |

---

## Repo layout

Monorepo: shared code once, one folder per volume.

```
na-encyclopedia/
├── CMakeLists.txt
├── CLAUDE.md
├── README.md
├── content/
│   └── compression.json        # all copy, targets and goals for Vol. IV
├── docs/
│   ├── UI-REFERENCE.md         # visual spec
│   └── FEEDBACK-ENGINE.md      # metrics and state machine spec
├── assets/
│   └── fonts/                  # Cinzel, EB Garamond (static cuts) + OFL license files, see README there
├── shared/                     # NAEncyclopediaCore (INTERFACE library: compiled into each plugin)
│   ├── content/                # JSON loader, templating, goal lookup
│   ├── analysis/               # FIFOs, onset detector, loudness, metrics
│   ├── chapters/               # chapter state machine, feedback evaluation
│   ├── ui/                     # Style (palette, fonts), BookFrame (cloth, spine, pages), Controls (knob, meter, buttons, icons, LookAndFeel), MetricsOverlay
│   └── tests/                  # analysis, content and session unit tests
├── volumes/
│   └── 04-compression/
│       ├── CMakeLists.txt
│       ├── PluginProcessor.{h,cpp}
│       ├── PluginEditor.{h,cpp}
│       ├── Parameters.{h,cpp}
│       ├── Chapters.{h,cpp}    # per-chapter knob, target, start values, Mix forcing
│       ├── BookViews.{h,cpp}   # Title Page + chapter page (plain, until M6)
│       ├── Feedback.{h,cpp}    # zones, status colours and meters per chapter
│       ├── tools/TuningReport.cpp  # sweeps each chapter's knob over a real recording
│       ├── dsp/Compressor.{h,cpp}
│       └── tests/              # DSP unit tests + end-to-end processor tests (ctest in CI)
├── installer/
│   ├── build-pkg.sh
│   └── scripts/                # preinstall / postinstall
└── .github/workflows/
    └── build-macos.yml
```

---

## Architecture

```
Audio thread                 Analysis                     UI (message thread)
────────────                 ────────                     ───────────────────
input ─▶ Compressor ─▶ out   FIFO drain (30 Hz)           Book frame + pages
   │        │                ├ activity gate              ├ Contents (left)
   │        └─ GR ──┐        ├ dynamic range              ├ Chapter (right)
   └─ dry ──────────┼──▶     ├ GR percentile         ──▶  ├ Knob for chapter
                    │ FIFOs  ├ onsets + fronts kept       ├ The Reading
                    │        ├ residual GR / ripple       └ Compare / Delta
                    │        └ loudness in vs out
                    │                 │
                    │        Chapter engine ◀── content/compression.json
                    │        (targets, hysteresis, messages)
```

Full spec: [`docs/FEEDBACK-ENGINE.md`](docs/FEEDBACK-ENGINE.md).

### Compressor DSP (V1)

| Parameter | Range | Notes |
|---|---|---|
| Threshold | −60 to 0 dB | |
| Ratio | 1:1 to 20:1 | Auto-set from the goal on Chapter II entry |
| Attack | 0.1 to 100 ms | Log taper |
| Release | 10 to 1000 ms | Log taper |
| Knee | 6 dB soft, fixed | Not exposed in V1 |
| Makeup | −12 to +24 dB | |
| Mix | 0 to 100% | Shown only for parallel goals |

- Feed-forward, stereo-linked, peak detector with a smoothed log-domain envelope
- Zero latency (no lookahead in V1)
- All parameters automatable via `AudioProcessorValueTreeState`
- Saved state includes instrument, goal, current chapter and which chapters are done

### Listening tools
- **Compare:** dry signal, gain-matched to the processed output, so the A/B is honest
- **Hear what's removed:** `dry − wet (pre-makeup)`

---

## Milestones

1. **Skeleton.** CMake + JUCE, empty AU/VST3 builds on GitHub Actions, loads in Logic, passes `auval`.
2. **Compressor.** DSP working with a plain generic editor. Sounds right, no clicks, automation works.
3. **Analysis.** Taps, FIFOs and every metric in the feedback spec, shown in a debug overlay.
4. **Content + chapters.** JSON loader, templating, chapter flow with real text, no styling yet.
5. **Live feedback.** States, hysteresis and messages wired to the metrics. Tune thresholds on real tracks.
6. **The book UI.** Build to `docs/UI-REFERENCE.md`.
7. **Listening tools.** Compare, Hear what's removed, level matching.
8. **Ship prep.** Installer, signing and notarization, beta with real beginners, content review by an engineer.

---

## Building

Normally you don't build locally: push to `main` and GitHub Actions does it.

To build on a Mac anyway (needs Xcode, CMake and Ninja):

```bash
git clone <repo-url>
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is downloaded automatically (CMake FetchContent, pinned tag in `CMakeLists.txt`). Builds are universal (`arm64;x86_64`) by default.

On every push to `main`, GitHub Actions builds the AU and VST3, ad-hoc signs them, builds the `.pkg`, installs that `.pkg` on the runner, runs `auval` and `pluginval` on the installed plugins, and uploads the `.pkg` as the **`NAEncyclopedia-IV-Compression-pkg`** artifact.

### Testing in Logic
1. Download the `NAEncyclopedia-IV-Compression-pkg` artifact from the green run and unzip it.
2. Double-click the `.pkg`. The first time, macOS blocks it (not notarized yet): open **System Settings → Privacy & Security** and click **Open Anyway**, then install.
3. Open Logic and load it on a track under **Audio FX → Audio Units → Noveltee Audio → NA Encyclopedia IV Compression**. The version and build ID show in the bottom-right corner of the window.
4. If it's missing, open **Logic Pro → Settings → Plug-in Manager**, search "NA", select it and click **Reset & Rescan Selection**.

---

## Licensing notes

- JUCE: check the current JUCE license terms before selling.
- Cinzel and EB Garamond: SIL Open Font License. Ship the license files.
