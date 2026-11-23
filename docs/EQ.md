# Vol. III, EQ: spec (DRAFT for owner review)

Third volume to be built. Runs on the shared book (`shared/book`) like Vol. I and IV. Content: [`content/eq.json`](../content/eq.json); every number in it is a starting point for the engineer review.

Owner decisions (Vol. III kickoff): **full 5-band parametric**; chapters **cut first, then shape**; a **spectrum** in The Reading.

---

## 1. Identity

| Field | Value |
|---|---|
| Product name | NA Encyclopedia III EQ |
| Company / manufacturer code | Noveltee Audio / `Tywl` |
| Plugin code | `NE03` |
| Bundle ID | `com.novelteeaudio.naencyclopedia.eq` |
| Version | 1.0.x (First Edition) |
| Cloth | `#1C3354` deep blue (UI-REFERENCE §8), same colour on the spine plate |
| Spine topic | EQ |
| Channels | mono→mono, mono→stereo, stereo→stereo |

## 2. The tool: a 5-band parametric EQ

Every band has **Frequency**, **Gain** and **Width** (Q), and an on/off switch. Each band has a home role the chapters use, but all five are fully adjustable.

| Band | Shape | Frequency range | Default | Home role |
|---|---|---|---|---|
| 1 | **Low cut** (12 dB/oct) or low shelf | 20 Hz – 1 kHz | low cut, 20 Hz (off) | Chapter II: clean the low end |
| 2 | Bell | 40 Hz – 2 kHz | 300 Hz, 0 dB | Chapter III: mud / boxiness |
| 3 | Bell | 100 Hz – 8 kHz | 1 kHz, 0 dB | Chapter V: shaping (body, presence) |
| 4 | Bell | 500 Hz – 16 kHz | 3.5 kHz, 0 dB | Chapter IV: harshness |
| 5 | **High shelf** or bell | 1 kHz – 20 kHz | shelf, 10 kHz, 0 dB | Chapter V: air |

Gain ±18 dB, width 0.3 – 8 (Q), plus an **Output** level (±12 dB) for Chapter VI. Zero latency (minimum-phase biquads). All parameters automatable; changes ramp so they never click.

## 3. Chapters (cut first, then shape)

| | Chapter | Knobs on the page | What it teaches |
|---|---|---|---|
| I | **Listen First** | none: the spectrum | Where this track's energy sits; whether it has a muddy or harsh build-up worth fixing (a "need check" like Vol. IV) |
| II | **Clean the Low End** | Band 1 frequency | Low cut: remove rumble below the instrument's lowest useful note |
| III | **Cut the Mud** | Band 2 frequency, gain, width | Boost-and-sweep to find it, then cut it a few dB |
| IV | **Tame the Harshness** | Band 4 frequency, gain, width | Same technique, higher up; cut narrow and gently |
| V | **Shape the Tone** | the goal's band (3 or 5): frequency, gain, width | Small, wide boosts toward the goal (e.g. clearer, warmer, airier) |
| VI | **Level & Compare** | Output | Match the level, then compare at equal loudness |

Chapters III and IV have a **"Hear the band"** button: a temporary narrow boost that solos the band's region so the user can hear exactly what they're cutting (the classic boost-and-sweep move). Chapter VI has **Compare at equal level** and **Hear what's removed** (shared listening tools).

## 4. Measurements (new shared analysis)

- **Spectrum** of `in` and `out`: FFT on the analysis side (4096 points, 50 % overlap), smoothed to 1/6 octave, averaged over the last ~3 s of active audio. No work on the audio thread beyond the existing taps.
- **Band energy** in dB for any frequency range, from that spectrum.
- **Build-up** in a range (mud, harshness): how far the range stands out **compared with a typical track of that instrument**, in dB. A normal lead vocal reads ~0 dB; a muddy one reads, say, +4 dB. *(Changed after the first Logic test: the draft measured against a straight line between the neighbouring octaves, and a raw vocal read +11.7 dB, because every voice naturally has far more energy at 200–500 Hz than at 100–200 Hz. Even an average speech spectrum read about +3 dB, so every vocal would have been called muddy.)*
- **Rumble**: energy below the instrument's low-cut range, relative to the whole track.
- **Level match**: the existing short-term loudness in vs out (Vol. IV §2.7).

*Built (step 2)* in `shared/analysis/SpectrumAnalyser` (plain C++, its own radix-2 FFT, tests in `shared/tests/SpectrumTests.cpp`):
- Bands every 1/6 octave from 10 Hz to 20.5 kHz (the display uses 20 Hz–20 kHz; the bands below 20 Hz count towards rumble). Each band takes its share of every FFT bin it overlaps, so the narrow low bands still read sensibly.
- Level = the signal's mean-square power in the band, in dB: a full-scale sine reads −3 dB in its band, pink noise reads flat.
- Frames whose `in` level is under −50 dBFS are skipped, so the reading holds when playback stops. A reading needs 1 s of active audio; the average is the last 3 s.
- Build-up = the track minus the instrument's **typical spectrum** (`typicalSpectrum` in the content file: (Hz, dB) points, interpolated in log frequency), then the mean of that difference over the range minus a straight line (least squares, log frequency) fitted to the difference around the range. The fit span is half the range's low edge (never below the instrument's `lowCutHz.max`) to four times its high edge (never above 16 kHz). The line removes overall tilt, so a darker or brighter recording of a normal track still reads ~0; only a bump in the range counts.
- Tests: a track shaped like the reference reads 0 ± 0.75 dB even though its body stands +3 dB or more above a flat line; the same track with a +6 dB, Q 1.4 bell at 300 Hz reads +2.5 to +6 dB; a broad −9 dB darkening reads 0 ± 1 dB. Against a flat reference, pink and white noise read 0 ± 0.75 dB.
- **After the EQ** (`out`): the `in` build-up plus `changeDb`: the mean out − in change over the range minus the median change from the instrument's `lowCutHz.max` to 16 kHz. *(Changed after the owner's Logic test of 1.0.2, where "out" sometimes read higher than "in" after a 6 dB wide cut. Measuring `out` against its own surroundings let a wide cut lower the surroundings too, and a low cut lower the area just below the range, so the cut looked smaller or even negative. Simulated on a voice-like track at +6.7 dB: a 6 dB cut at 300 Hz now reads +2.7 (Q 1.4), +2.2 (Q 0.5), +3.0 (Q 0.3), was +3.6 / +4.6 / +5.5; a 100 Hz low cut alone now reads +6.7, was +7.1.)* A level change (Output) reads 0.
- Rumble = power below the instrument's `lowCutHz.min` ÷ the whole track's power, in dB.
- The book runs it only for volumes that ask (`enableSpectrum()`), on the analysis timer, never the audio thread. The debug overlay shows the readings for the chosen instrument.

## 5. Feedback rules (draft)

- **I. Listen First**: build-up in the instrument's mud range ≥ `mudNeedDb` or harsh range ≥ `harshNeedDb` → `needed` (which one is named in the message); otherwise `notNeeded` ("already fairly balanced; you can still continue"). Any verdict ticks the chapter.
- **II. Clean the Low End**: Band 1 frequency vs the instrument's `lowCutHz` range: below → `low` ("still letting rumble through"), above → `high` ("cutting into the body"), inside → `onTarget`.
- **III. Cut the Mud**: band 2 must be **inside the instrument's mud range** and **cutting** by `cutDb` (e.g. −2 to −6 dB): out of range → `wrongPlace`; boosting or no cut → `low`; cutting too deep → `high`; the measured build-up after the cut ≤ `mudAfterMaxDb` and cut depth in range → `onTarget`. If Chapter I found no mud, this chapter says so and ticks on any gentle setting ("nothing to fix: leave it flat").
- **IV. Tame the Harshness**: same rules with band 4, the harsh range, `harshCutDb` and `harshAfterMaxDb`; width must be narrow-ish (Q ≥ `harshMinQ`).
- **V. Shape the Tone**: the goal's band, frequency inside the goal's range, gain inside the goal's boost range (e.g. +1 to +4 dB), width wide (Q ≤ 1.5). Boosting more than the range → `high` ("subtle beats loud").
- **VI. Level & Compare**: Output so out − in loudness is within ±1 dB (same as Vol. IV Chapter V).

Same state machine, timing and ticks as the other volumes.

**Content note for the build:** unlike Vol. IV, most EQ targets belong to the *instrument* (low-cut, mud and harsh ranges), and only the Chapter V shaping belongs to the *goal*. Templates therefore also read instrument fields (`{lowCutHz.start}`, `{instrumentName}`) and the goal's name (`{goalName}`): a small extension to `shared/content/Template`.

## 6. UI additions (shared, reusable)

- **Several knobs per chapter.** The knob block shows the chapter's main knob (132 px) plus up to two smaller knobs (72 px) beside it, each with its own name and value. Volumes pick which knobs a chapter shows. The page's long-chapter rules still apply.
- **Spectrum view** in The Reading (replaces the meters in Chapters I–V): a calm filled curve of `out` (in `inkMuted`, 30 % alpha), the faint `in` curve behind it, the EQ curve in `cloth`, the chapter's problem range shaded `targetZone`, frequency labels at 100 Hz / 1 kHz / 10 kHz. 30 fps maximum, no flicker (it's the averaged spectrum, not a live analyser).
- **"Hear the band"** toggle in Chapters III–IV, in the bottom row where the listening tools sit in Vol. IV.

## 7. Build plan

1. **Shared UI additions + Vol. III skeleton**: multi-knob chapters and the spectrum view in `shared/`; the Vol. III plugin with the 5-band EQ DSP and its parameters; CI builds and packages all three volumes.
2. **Spectrum analysis + metrics**: FFT, band energy, build-up, rumble; unit tests with known signals; tuning report on the test vocal (and drums if you have a stem).
3. **Chapters + feedback**: Vol. III's rules, "Hear the band", Compare / Hear what's removed in Chapter VI.
4. **Demo pass**: all three volumes in one chain (I → III → IV).

## 8. Tuning on real recordings

`NAEncyclopediaSpectrumReport <file.wav> [instrumentId]` (built with the tests, not shipped) runs the analysis over a recording and prints the spectrum plus, for every instrument in the content file, the median and 90th-percentile build-up in its mud and harsh ranges and its rumble, next to `mudNeedDb` / `harshNeedDb`.

- **Test vocal (Ride 'Em Cowboy `raw vox.wav`), read in Logic by the owner on 1.0.1:** mud build-up +11.7 dB on the old straight-line measure (owner: "it does sound a bit muddy"), harsh build-up −1 to +1 dB. The old measure counted the voice's natural body as mud, so the measure was changed (§4). The new reading of the same vocal is still to come; it should land clearly above `mudNeedDb` (3) since the track does sound muddy, and harshness should stay near 0.
- **Same vocal on 1.0.2** (typical-spectrum measure): mud build-up in about +6 to +7 dB on average, peaks near +10. Above `mudNeedDb` (3), so Chapter I says it needs a cut, matching the owner's ear. Note for step 3: with the draft `mudCutDb` (−2 to −6 dB) a +6.5 dB track lands around +2 to +3 after the deepest allowed cut, above the draft `mudAfterMaxDb` (1.5). Chapter III must stay finishable, so the rule should judge the *improvement* or the target needs review.
- **Typical spectra** (`typicalSpectrum`, one per instrument) are drafts. The voices follow published long-term average speech spectra (Byrne et al. 1994), with a little more 2–4 kHz for lead vocal; the others are common reference shapes. Engineer to review, ideally by running this report over a few well-mixed stems of each instrument and averaging.
- Seen on a sustained single-note instrument (JUCE's cello sample, looped): build-up swings by several dB depending on whether a strong harmonic lands inside the range (+5 to +8 dB in 200–500 Hz, −2.5 dB in 400–800 Hz). Real parts with many notes average this out over 3 s, but a held note can read as "muddy". Something to watch when tuning `mudNeedDb`.

## 9. Questions for the owner / engineer review

1. **Goals**: two per instrument for Chapter V (e.g. Lead Vocal: "Clear & Present", "Warm & Smooth"; Mix Bus has one). See the content file. Right number, right names?
2. **Numbers**: low-cut ranges, mud and harsh ranges, cut depths and boost ranges per instrument are common starting points. Engineer to confirm.
3. **Copy**: all chapter text and messages are drafts in the Vol. I/IV voice.
