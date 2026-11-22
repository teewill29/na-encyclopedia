# Vol. I, Gain Staging: spec (DRAFT for owner review)

Second volume to be built (first in the set). Shares the book, the chapter engine, the analysis and the UI with Vol. IV. Content: [`content/gain-staging.json`](../content/gain-staging.json). Every number in it is a starting point for the engineer review, like Vol. IV's.

Owner decisions (Vol. I kickoff): the plugin is **a trim knob plus meters**; it teaches **peaks and average**; content drafted by Claude for review; Vol. IV gets a **level check** in its Chapter I.

---

## 1. Identity

| Field | Value |
|---|---|
| Product name | NA Encyclopedia I Gain Staging |
| Company | Noveltee Audio |
| Manufacturer code | `Tywl` |
| Plugin code | `NE01` |
| Bundle ID | `com.novelteeaudio.naencyclopedia.gainstaging` |
| Version | 1.0.x (First Edition) |
| Cloth | `#3A3F44` slate (UI-REFERENCE §8) |
| Spine topic | GAIN STAGING (12 characters: one line, 22 px) |
| Channels | mono→mono, mono→stereo, stereo→stereo (same as Vol. IV) |

## 2. The tool

One parameter, **Trim**, −24 to +24 dB, default 0 dB, ramped like Vol. IV's makeup so it never clicks. Zero latency. Nothing else touches the audio. Saved state: trim plus the book session (instrument, goal, chapter, ticks).

## 3. Chapters

| | Chapter | Knob | Measures | Status |
|---|---|---|---|---|
| I | Where You Are | none (shows the incoming peak and average) | peaks and average, **before** the trim | too quiet / about right / too hot |
| II | Peaks | Trim | peak level after the trim | low / onTarget / high vs `peakDb` |
| III | Average | Trim (nudge only) | average level after the trim, and the gap to the peaks | very dynamic / onTarget / very dense vs `averageDb` |
| IV | Headroom | Trim | loudest peak since the chapter opened | clipping / too tight / onTarget vs `headroomMinDb` |

No Compare / Hear what's removed in this volume: trim only changes level, so a level-matched comparison would sound identical. That *is* the lesson, and Chapter I's "Listen for" says it.

Chapter III is deliberately a **teaching** chapter: one knob can't always put both peaks and average in their zones. When the peaks are right but the average is low, the message says the track is very dynamic and that a compressor (Vol. IV) is how you bring the average up. That's the link for the demo: Vol. I → Vol. IV.

## 4. Metrics (added to the shared analysis)

Measured on the same taps as Vol. IV (`in` before the trim, `out` after), same 4-second window and activity gate (FEEDBACK-ENGINE §2.1).

- **Peak level:** highest sample peak per 400 ms window, active audio only; report the **95th percentile** of those over the window, so one stray spike doesn't decide the level. (Sample peak, not true peak: at −6 dBFS true peak doesn't matter.)
- **Average level:** RMS over active audio in the window (unweighted, so "−18 dBFS RMS" means what mixing courses mean).
- **Peak to average** (crest factor): peak − average, in dB. Shown as information.
- **Loudest peak so far:** the highest sample peak since the chapter opened (reset when the chapter is opened or the trim moves by more than 1 dB).

Chapter I uses the `in` measurements; Chapters II–IV use `out`.

## 5. Feedback rules

Same state machine as Vol. IV (silence → listening → evaluating, 1.5 s hysteresis, 2 s hold for the tick, knob-moved dim, audio-time clock).

- **I. Where You Are** (`in` peak): above −1 dBFS → `tooHot`; below the goal's `peakDb.min` − 10 → `tooQuiet`; else `aboutRight`. Any verdict ticks the chapter.
- **II. Peaks** (`out` peak): below `peakDb.min` → `low`; above `peakDb.max` → `high`; else `onTarget`.
- **III. Average** (`out` average): below `averageDb.min` → `low` (very dynamic); above `averageDb.max` → `high` (very dense); else `onTarget`. **Ticks on `onTarget`, or on `low`/`high` while the peaks are still on target**, because then the advice is "leave it, compress later", and that's a correct finish.
- **IV. Headroom** (loudest peak so far): ≥ −0.1 dBFS → `clipping`; above −`headroomMinDb` → `tight`; else `onTarget` (needs at least 10 s of active audio).

## 6. Vol. IV level check (the tie-in)

Vol. IV's Chapter I adds one line to The Reading when the incoming level is far off, using the shared peak metric on `in`:
- peak above `hotAbovePeakDb` (−1 dBFS): "This track is coming in very hot. Gain stage it first (Volume I) so the compressor's targets line up."
- peak below `quietBelowPeakDb` (−20 dBFS): "This track is coming in very quiet. Gain stage it first (Volume I) so the threshold sits where the book expects."

Copy goes in `content/compression.json` (`messages.levelCheck`). It's a hint, not a status: it doesn't change Chapter I's verdict or block anything.

## 7. UI

Exactly the Vol. IV book (UI-REFERENCE), slate cloth, Vol. I plate. The Reading's meters:
- I: Peaks and Average (incoming), no target zones
- II: Peaks (target zone `peakDb`)
- III: Average (target zone `averageDb`) + Peak to average
- IV: Loudest peak so far (zone from −60 to −`headroomMinDb`)

Meter range −48 to 0 dBFS.

## 8. Build plan

Most of the work is moving Vol. IV's book screens into `shared/` so both volumes use them, which means **Vol. IV gets retested** in the same build.

1. **Shared book + Vol. I skeleton.** Move the Title Page / chapter page / editor into `shared/ui`, driven by a per-volume rules interface (knob per chapter, zones, meters). Vol. I plugin loads with the book; Vol. IV unchanged on screen. CI builds and packages both (one `.pkg` each).
2. **Trim + metrics.** Trim DSP, the four new metrics, unit tests, tuning report on the test vocal.
3. **Chapters + feedback.** Vol. I's chapters, zones and meters; Vol. IV's level check.
4. **Demo pass.** Both volumes side by side in Logic.

## 9. Questions for the owner / engineer review

1. **Goals:** every instrument has one goal ("Ready to Mix"; Mix Bus: "Into the Master"), so the Title Page shows a single card, already selected. OK, or do you want a second goal (e.g. "Ready to Record" with more headroom)?
2. **Numbers:** peaks −10 to −6 dBFS for most sources (backing vocals −12 to −8, drum bus −8 to −4, mix bus −6 to −3), averages per instrument as in the content file, headroom at least 3 dB. Engineer to confirm.
3. **Copy:** all chapter text and messages are drafts in the Vol. IV voice.
