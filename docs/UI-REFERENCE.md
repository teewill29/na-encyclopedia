# UI Reference

Visual mockups live on the design canvas: https://claude.ai/artifact/BVmfMtAA4NMUiXtu2J5EXV

- **Title Page:** setup (instrument + goal)
- **Chapter III, Attack:** the main chapter layout with live feedback
- **The Set:** all ten spines on a shelf

This doc is the build spec. When the canvas and this doc disagree, this doc wins.

---

## 1. Concept

The plugin is an open encyclopedia volume. A half-leather binding: a **shared leather spine** on the left, **cloth boards** around the edge in the volume's topic color, and two cream pages inside.

- **Left page:** Contents. Where you are, what's done, what you've set.
- **Right page:** the current chapter. Instructions, the one knob that matters, and The Reading (live feedback).

## 2. Window

- Base size **1120 × 760 px**.
- Resizable in fixed steps: 80%, 100%, 125%, 150%. Scale everything from the base layout; no reflow.
- Remember the last size per user.

## 3. Colors

| Token | Hex | Use |
|---|---|---|
| `cloth` | per volume (§8) | Outer frame, primary buttons, current chapter marker |
| `gold` | `#CDAE5E` | Foil: spine text, knob indicator, primary button text |
| `goldLine` | `#A88B4A` | Ornament rules on paper |
| `leatherDark` | `#1A100A` | Spine gradient edge |
| `leatherMid` | `#3A2618` | Spine gradient center |
| `paper` | `#F4ECDB` | Page background |
| `paperShade` | `#DDCFB3` | Page gutter shading |
| `paperPanel` | `#EFE5CF` | The Reading panel |
| `paperField` | `#FBF7EE` | Inputs, secondary buttons |
| `rule` | `#C2B08A` | Dividers, panel borders |
| `ink` | `#2A2118` | Body text |
| `inkSoft` | `#4A3C2C` | Secondary text |
| `inkMuted` | `#5E4E3B` | Captions, small labels (passes 4.5:1 on paper) |
| `labelInk` | `#6B5A45` | Small-caps eyebrows |
| `good` | `#2F5E3A` | On target |
| `warnLow` | `#9A2E1F` | Too fast / too much |
| `warnHigh` | `#8A5410` | Too slow / too little |
| `meterTrack` | `#DCCFB3` | Empty meter |
| `targetZone` | `#2F5E3A` at 22% | Shaded target band |

Status is never color alone: every status also has a text label ("Too fast", "Right on target").

## 4. Type

Both fonts are SIL Open Font License. Bundle the `.ttf` files in `assets/fonts/` with their license text and load them through `BinaryData`.

| Role | Font | Size / weight |
|---|---|---|
| Chapter title | Cinzel | 42 px, 600, letter-spacing 0.03em |
| Title page heading | Cinzel | 58 px, 600 |
| Eyebrows (CHAPTER III, CONTENTS, DO THIS) | Cinzel | 12–13 px, 500, letter-spacing 0.2–0.24em |
| Knob value | Cinzel | 26 px, 600, tabular numbers |
| Body copy | EB Garamond | 17–18 px, 400, line-height 1.45 |
| Status label | EB Garamond | 20 px, 600 |
| Captions | EB Garamond | 13–15 px, italic where shown |

Minimum text size anywhere: 13 px at 100% scale.

## 5. Spine anatomy (shared by every volume)

The spine is identical across the set. Only the topic word, the volume numeral and the label plate color change. Lined up, the volumes read as one set.

**Every element sits at a fixed position.** Bands, the series line, the label plate and the maker line must land at the same height on every volume, so the set lines up on a shelf. Only the topic zone stretches, and the topic word is centered inside it. Never use space-between to distribute spine elements.

96 px wide. Padding 18 px top, 16 px bottom. From top to bottom:

| # | Element | Size / spacing |
|---|---|---|
| 1 | **Raised bands:** gold rule 2 px, dark ridge 4 px (`#0E0805`), gold rule 2 px, 3 px gaps, 72 px wide | flush with top padding |
| 2 | **Series zone:** "NA ENCYCLOPEDIA", vertical, Cinzel 10 px, letter-spacing 0.2em, gold at 80%, top-aligned | fixed 150 px tall, 14 px above |
| 3 | **Raised bands** | 14 px above |
| 4 | **Topic zone:** topic word, vertical, Cinzel 600, gold, centered | fills remaining height |
| 5 | **Raised bands** | directly under topic zone |
| 6 | **Label plate:** 64 × 78 px, volume `cloth` color, double gold border (1 px, 3 px gap). "VOL." Cinzel 10 px over the numeral, Cinzel 700 (30 px in the plugin, 26 px on the shelf view) | 16 px above |
| 7 | **Edition:** "FIRST" / "EDITION" on two lines, Cinzel 8 px, gold at 70% | 12 px above |
| 8 | **Raised bands** | 12 px above |
| 9 | **Maker:** "NOVELTEE" / "AUDIO" on two lines, Cinzel 9 px, letter-spacing 0.16em, gold at 75% | 12 px above |

Spine height is **724 px** everywhere (plugin window and shelf/marketing views), which leaves a topic zone of about 277 px.

Topic word rules:
- **Never wrap automatically.** `white-space: pre`, so a word can't break mid-name or spill out of the zone.
- **Up to 12 characters:** one line, 22 px, letter-spacing 0.24em.
- **13–14 characters:** one line, 19 px, 0.2em.
- **15+ characters:** two lines, split at the space closest to the middle (BALANCE & / PANNING), 20 px, 0.2em, line-height 1.2, **centered**.
- Every multi-line text on the spine (topic, FIRST / EDITION, NOVELTEE / AUDIO) is centered, never left-aligned.

Spine background: horizontal gradient `leatherDark → leatherMid (40%) → #2B1B12 (70%) → #120A06`, inset shadow on the page side.

## 6. Screens

### 6.1 Title Page (first launch, or "Change" from Contents)
- **Left page, centered:** "NOVELTEE AUDIO" eyebrow, ornament rule, *NA Encyclopedia* (italic), **Compression** (Cinzel 58), "Volume IV" eyebrow, ornament rule, *First Edition* (italic).
- **Left page, foot:** the gain-staging note ("FIRST, GAIN STAGE" eyebrow + italic body, `book.titlePage.gainNote`) between two `goldLine` rules. Added in Milestone 7 at the owner's request: compression targets only make sense on a track that's already at a sensible level.
- **Right page:** "BEFORE WE BEGIN" eyebrow, "Set the scene" heading, one line of intro.
  - Dropdown: **What are you compressing?** All instruments from the content file, in file order.
  - **What should it do?** Goal cards in a 2-column grid, one per goal for the chosen instrument (1 to 3 cards). Card = goal name + tagline. Selected card: 2 px `cloth` border, darker paper, check mark.
  - If the instrument has a `lesson` (distorted guitar), show it in italics under the cards.
  - Primary button: **BEGIN CHAPTER I**.

### 6.2 Chapter page (every chapter)
**Left page (330 px wide): Contents**
- "CONTENTS" eyebrow
- Current setup ("Lead Vocal · More Punch") with a **Change** text button
- Divider
- Chapter rows, 44 px tall: numeral column (30 px), title, status at right
  - Done: green check
  - Current: darker paper fill, 3 px `cloth` bar on the left edge, bold title, italic "reading now"
  - Not yet: muted ink, no icon
  - Rows are buttons. Clicking jumps to that chapter.
- Bottom, "SET SO FAR": small knobs (48 px) with name and value for each control already set

**Right page: the chapter**
- Top row: "CHAPTER III" eyebrow left, *3 of 5* right (italic)
- Title (Cinzel 42)
- Intro paragraph
- Two-column row:
  - **Knob block (170 px):** small-caps knob name, main knob (132 px), range labels under it, value (Cinzel 26), *Target: 15–30 ms* caption
  - **Instructions:** "DO THIS" eyebrow + text, "LISTEN FOR" eyebrow + text. Chapter II adds a "TRY THIS" line.
- **The Reading panel** (`paperPanel`, 1 px `rule` border, 6 px radius):
  - Header: "THE READING" eyebrow left, live dot + *Listening live* right
  - Status: 12 px status dot + status label (20 px, status color) + coaching message (17 px)
  - Two meters side by side (see §7). Chapter-specific: Chapter II shows GR; III shows GR + Fronts kept; IV shows GR + Recovery; V shows In vs Out loudness.
- Bottom row: **Hear what's removed** (toggle), **Compare at equal level** (hold/toggle), spacer, *‹ Chapter II* (text button), **CHAPTER IV ›** (primary)
  - All four don't fit in the 570 px row at these font sizes, so *‹ Chapter II* moves to the top row, left of *3 of 5*, whenever the row is too narrow.
  - Compare: a quick click toggles; holding longer than 350 ms is momentary (off on release). The two tools are exclusive.
- Long chapters: if the right page doesn't fit, the layout steps down in this order: instructions text (to 15.5 px), section gaps, The Reading's padding, intro (to 16 px), instructions (to 14 px), message, intro (to 15 px). The knob shrinks (to 96 px) before any text does.

Chapter numbering: I The Goal, II Ratio & Threshold, III Attack, IV Release, V Level & Compare, VI Blend (parallel goals only). The *x of y* counter uses the goal's real chapter count.

### 6.3 Chapter I specifics
No main knob. The knob block becomes a large **dynamic range** readout ("9.4 dB of movement"). The Reading shows the need-check message. A tooltip-style aside explains what dynamic range means.

## 7. Components

**Knob**
- Knurled ring: `repeating-conic-gradient` look, alternating `#1C1C1C` / `#3A3A3A` every 4°
- Cap: radial gradient, highlight top-left (`#474747` → `#121212`)
- Indicator: gold bar, 5 × 28 px at 132 px size
- Active chapter knob gets a 3 px gold ring and a drop shadow. Knobs from earlier chapters are 48 px and have no ring.
- Rotation range: −135° to +135°. Time knobs use a **log** taper.
- Drag vertically, fine adjust with Shift, double-click to type a value, scroll wheel works.

**Meter**
- 12 px tall track, 6 px radius, `meterTrack`
- Target zone: `targetZone` fill with 1 px `good` borders at both edges
- Fill bar: inset 3 px top and bottom, colored by the current status
- Label row above (name left, value right, tabular numbers), target caption below

**Buttons**
- Primary: `cloth` background, `gold` Cinzel 14–17 px text, letter-spacing 0.14em, 46–54 px tall
- Secondary: `paperField` background, 1.5 px `#A8966F` border, ink EB Garamond 16 px semibold, optional line icon
- Text button: no background, italic EB Garamond, ink-soft
- Every clickable target is at least 44 px tall

**Icons:** simple line icons, 2 px stroke, round caps. Check, headphones (hear what's removed), swap arrows (compare). No emoji.

## 8. Volume colors (cloth + spine plate)

| Vol. | Topic | Cloth |
|---|---|---|
| I | Gain Staging | `#3A3F44` slate |
| II | Balance & Panning | `#1E4A4A` teal |
| III | EQ | `#1C3354` deep blue |
| **IV** | **Compression** | **`#213B2F` forest green** |
| V | De-essing | `#3E2440` plum |
| VI | Gating | `#3B2A1E` walnut |
| VII | Reverb | `#26244A` indigo |
| VIII | Delay | `#5A2E1C` rust |
| IX | Saturation | `#4A1C1C` oxblood |
| X | Limiting | `#1A1A1A` black |

Cloth texture: two faint diagonal weaves layered over the base color (white at 3.5% and black at 9%, 2 px lines every 5 px, ±45°). Subtle. It should read as cloth up close and a flat color from across the room.

## 9. Don'ts

- No glossy gradients, glows or drop shadows beyond the knob and page edge
- No emoji
- No copy hard-coded in the UI. Every string comes from the content file.
- No status communicated by color alone
