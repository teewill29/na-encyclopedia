# Feedback Engine

How the plugin "listens" and tells the user whether each chapter is on target. There's no AI here: the plugin measures its own input and output, compares the numbers to the target ranges in `content/compression.json`, and shows the matching message.

The key fact: **the plugin is the compressor.** It knows every knob value, hears the signal before processing, and hears it after. Every metric below comes from those three things.

---

## 1. Signal taps

The audio thread writes four streams into lock-free FIFOs (no allocation, no locks, no logging on the audio thread):

| Tap | What it is |
|---|---|
| `in` | Input, mono-summed |
| `out` | Output after makeup, before Mix |
| `gr` | Gain reduction per sample, in dB (positive = reducing) |
| `grTarget` | The reduction an instant attack would apply right now (the static curve), in dB. Used by §2.5 |

Onsets (§2.4) are detected on the analysis side from `in`, not on the audio thread: the taps are sample-aligned, so the result is the same and the audio thread does less.

An analysis thread (or a 30 Hz `juce::Timer` on the message thread if that's fast enough) drains the FIFOs and updates the metrics. Use a rolling **analysis window of 4 seconds** unless noted.

## 2. Metrics

### 2.1 Activity gate
Everything ignores audio below **−50 dBFS short-term RMS (300 ms)**. If more than 80% of the window is below the gate, the state is `silence`. If less than 2 seconds of active audio have been collected, the state is `listening`.

### 2.2 Dynamic range (Chapter I need check)
- Compute momentary loudness (400 ms windows, K-weighted, 100 ms hop) on `in`.
- Range = 95th percentile minus 10th percentile, active windows only.
- If `needCheckMinRangeDb` is `null`, skip the check (drums, buses) and show `needCheck.skipped`.
- Chapter I is complete when any of `needed`, `notNeeded` or `skipped` has held for 2 s.
- If range < `needCheckMinRangeDb`, show `needCheck.notNeeded`. Otherwise show `needCheck.needed`. The user can continue either way.

### 2.3 Gain reduction on loud parts (Chapter II)
- From `gr`, take the **90th percentile** of gain reduction over active audio in the window.
- Compare to `grDb.min` and `grDb.max`.
- Meter shows the live GR value with the target zone shaded.

### 2.4 Onset detection
Needed for Chapters III and IV.
- Envelope follower on `in`: 1 ms attack, 50 ms release.
- Onset when the envelope rises more than **6 dB within 10 ms** and at least **50 ms** has passed since the last onset.
- Tune per instrument family later if needed (drums vs sustained sources).

### 2.5 Fronts kept (Chapter III, attack)
How much of each note's front gets past the compressor.

*Changed in Milestone 5.* The original measure (peak in the first 10 ms minus body RMS, out vs in) was tested on a real lead vocal and read ~100% at every attack setting: a sung word's first 10 ms is its quiet lead-in, so the compressor rarely touches it, and squashing the body only makes that "punch" number go up. It only worked on drums. The measure below works on any source and moves smoothly with the knob.

- For each onset, over the front window (2 ms before to 10 ms after the onset):
  - `before` = `gr` at the start of the window (reduction left over from the last note; that's Release's business)
  - `added` = peak `gr` in the window − `before`
  - `wouldAdd` = peak `grTarget` in the window − `before`
- Skip onsets where `wouldAdd < 1 dB` (the front isn't over the threshold, so attack doesn't matter).
- `frontsKept = (1 − added / wouldAdd) × 100`, clamped to 0–100%. Fast attack catches the front (low %), slow attack lets it through (high %).
- Report the **median** over the last 8 counted onsets.

On the test vocal (Lead Vocal · More Punch, threshold for ~4.5 dB GR, 4:1): 0.3 ms → 53%, 3 ms → 88%, 10 ms → 95%, 15–30 ms → 97–98%, 100 ms → 99.7%.

**Status (owner decision, Milestone 5):** until the content targets are reviewed, Chapter III's status comes from the **Attack knob against `attackMs`** (below min → `attack.low`, "too fast"; above max → `attack.high`, "too slow"). The fronts-kept meter shows the live measurement with the `frontsKeptPct` band for comparison. The two content targets don't agree on real audio yet (More Punch: 15–30 ms gives ~97%, but `frontsKeptPct` says 70–90%).

### 2.6 Recovery (Chapter IV, release)
Two checks, either can fail:
- **Too slow:** for each onset, read `gr` in the **5 ms just before** it. Median over the last 8 onsets = `residualGr`. If `residualGr > residualGrMaxDb`, show `release.tooSlow`.
- **Too fast:** if release time is below `releaseMs.min` **or** GR ripple (peak-to-peak GR within any 20 ms span during sustained audio) exceeds 3 dB, show `release.tooFast`.
- If the release knob sits inside `releaseMs` and both checks pass, show `release.onTarget`. A knob above `releaseMs.max` shows `release.tooSlow`.
- On the test vocal, ripple stayed near 1 dB even at a 10 ms release, so "too fast" currently comes from the knob check. Revisit the 3 dB ripple limit on drums and bass.

### 2.7 Level match (Chapter V)
- Short-term loudness (3 s, K-weighted) of `in` vs `out`.
- `diff = out - in`.
- `diff > +1 dB` → `level.high`. `diff < -1 dB` → `level.low`. Otherwise `level.onTarget`.

### 2.8 Blend (Chapter VI, parallel only)
- Simple setting check: Mix value vs `mixPct.min` and `mixPct.max`.
- During Chapters II–IV of a parallel goal, Mix is forced to 100% so the user hears the smashed copy alone.

## 3. State machine per chapter

```
silence ──audio──▶ listening ──2 s active──▶ evaluating
                                               │
                         ┌─────────────────────┼─────────────────────┐
                         ▼                     ▼                     ▼
                        low                onTarget                 high
```

Rules:
- **Clock:** all timings below are in seconds of *audio analysed*, not wall-clock time, so "hold for 2 s" means 2 s of the track and pauses when playback stops.
- **First reading:** the first zone after `listening` shows at once (the user has already waited 2 s); later changes need hysteresis.
- **Hysteresis:** a metric must sit in a new zone for **1.5 seconds** before the state changes. This stops the status flickering while the user turns a knob.
- **Green needs 2 seconds of stability.** The chapter's check mark appears after `onTarget` holds for 2 seconds.
- **Knob moved → short reset.** When the chapter's main knob moves, hold the current message, dim the status for 300 ms, then resume evaluation. Don't wipe the reading.
- **Never block navigation.** The user can always move on. An unfinished chapter shows as unchecked in Contents.

## 4. Chapter wiring

| Chapter | Main control | Metric | Message keys |
|---|---|---|---|
| I. The Goal | none | Dynamic range | `needCheck.*` |
| II. Ratio & Threshold | Threshold (ratio auto-set to `ratio.start`) | GR 90th pct | `ratioThreshold.*` |
| III. Attack | Attack | Knob vs `attackMs` (status); fronts kept (meter, §2.5) | `attack.*` |
| IV. Release | Release | Residual GR + ripple | `release.*` |
| V. Level & Compare | Makeup | Loudness diff | `level.*` |
| VI. Blend (parallel only) | Mix | Mix setting | `blend.*` |

When the user enters a chapter for the first time, set that chapter's knob to its `start` value (e.g. `attackMs.start`). Don't reset it again if they come back.

If the user changes the ratio in Chapter II, keep the GR target as written. It already describes the outcome, not the setting.

## 5. Text templating

Content strings use `{path}` placeholders, resolved against the selected goal:
- `{ratio.start}`, `{attack.start}`, `{release.start}` → numbers from the goal (map `attack` → `attackMs`, `release` → `releaseMs`)
- `{goal.listenFor.attack}` etc. → the goal's `listenFor` object

Chapter III uses `doThisFromFast` or `doThisFromSlow` based on `attackMs.approach`.

All user-facing text lives in the content JSON. **Never hard-code copy in C++.**

## 6. Compare and "Hear what's removed"

- **Compare (hold or toggle):** plays the dry input, delayed to match any latency, at a gain equal to the measured `diff` from §2.7 so the two versions are volume matched. Without this, louder always sounds better.
- **Hear what's removed:** plays `dry - wet_pre_makeup`. That's the part the compressor is taking away, which is what beginners need to hear to understand the knobs.
- Built in Milestone 7 as `shared/dsp/ListeningTools` (any gain-based processor can use it). Switching crossfades over 20 ms; the Compare gain glides over 50 ms. There's no latency, so no delay is needed. The analysis taps are unaffected: The Reading keeps measuring the real compressed signal while you listen. Compare matches §2.7's `out` (before Mix); with Mix below 100% the match is approximate.

## 7. Debug view

Build a hidden debug overlay (toggle with a keyboard shortcut in debug builds) that prints every metric live. Tune the thresholds against real tracks before polishing the UI.
