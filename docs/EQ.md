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
- **Build-up** in a range (mud, harshness): the range's energy above a straight line drawn between its two neighbouring octaves, in dB. A flat-ish track reads ~0 dB; a muddy one reads, say, +4 dB.
- **Rumble**: energy below the instrument's low-cut range, relative to the whole track.
- **Level match**: the existing short-term loudness in vs out (Vol. IV §2.7).

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

## 8. Questions for the owner / engineer review

1. **Goals**: two per instrument for Chapter V (e.g. Lead Vocal: "Clear & Present", "Warm & Smooth"; Mix Bus has one). See the content file. Right number, right names?
2. **Numbers**: low-cut ranges, mud and harsh ranges, cut depths and boost ranges per instrument are common starting points. Engineer to confirm.
3. **Copy**: all chapter text and messages are drafts in the Vol. I/IV voice.
