# Fonts

Both fonts are under the SIL Open Font License 1.1 (license texts alongside).
Neither declares a Reserved Font Name, so modified copies may keep their names.

| File | Source | Weight |
|---|---|---|
| Cinzel-Medium / SemiBold / Bold.ttf | google/fonts `ofl/cinzel/Cinzel[wght].ttf` | 500 / 600 / 700 |
| EBGaramond-Regular / Medium / SemiBold.ttf | google/fonts `ofl/ebgaramond/EBGaramond[wght].ttf` | 400 / 500 / 600 |
| EBGaramond-Italic.ttf | google/fonts `ofl/ebgaramond/EBGaramond-Italic[wght].ttf` | 400 |

JUCE can't pick weights inside a variable font, so each weight was cut as a
static instance with fontTools (`varLib.instancer`) and subset to Latin-1,
Latin Extended-A and general punctuation (U+0020-007E, U+00A0-017F,
U+2010-202F, ‹ › − → ← € ™), keeping kerning and ligatures. To add a
character, re-run that with the wider range.

Embedded in the plugin through `juce_add_binary_data` (target `NAEncyclopediaFonts`).
