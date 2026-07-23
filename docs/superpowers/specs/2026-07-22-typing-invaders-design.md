# Typing Invaders — Design

**Date:** 2026-07-22
**Target:** FreeWili 2 (RP2350B), native C firmware against [wilibsp](https://github.com/freewili/wilibsp)
**Goal:** A fun arcade game that teaches proficiency on the FreeWili two-press chord keyboard ("chordboard").

## Concept

Aliens — colored ASCII-art sprites carrying a word banner — fall from the top of the 480×320 screen toward a defense line at the bottom. The player types each alien's word on the chordboard to destroy it before it lands. Typing-of-the-Dead mechanics meet Space Invaders presentation, with a difficulty curve engineered to take a player from "never used the chordboard" to touch-chording.

## The chordboard

The FreeWili keyboard (`bsp/keyboard/fw2kb`) uses 5 colored buttons (Gray, Yellow, Green, Blue, Red). Typing one letter takes two presses: the first selects a group of 5 characters, the second selects the letter within the group. Every letter therefore maps to an ordered two-color chord. The game exploits this: letters can display their chord as a pair of colored dots, and the LED strip echoes chord colors, reinforcing color-pair memory.

## Gameplay

### Core loop

- Aliens spawn at the top and fall toward the defense line.
- Completing a letter (via `fw2kb_next_event()`) that matches the *next letter of some alien's word* locks targeting onto that alien (targeting bracket appears) and fires a one-frame laser from the player's cannon at bottom center.
- Subsequent letters must continue that alien's word until it is destroyed.
- Wrong letter: error blip + haptic tick, breaks the streak, unlocks the target. Wrong letters are never entered — no backspace needed.
- Finishing a word destroys the alien (explosion animation + SFX + score).
- An alien crossing the defense line costs a life. 3 lives; 0 = game over.

### Levels

Each level is a wave with a fixed spawn budget; clearing the budget advances the level (level intro card between waves).

| Levels | Content | Notes |
|---|---|---|
| 1–2 | Single letters | Learn the chords |
| 3–5 | 3–4 letter words | |
| 6+ | Longer/rarer words, faster falls, more simultaneous aliens | Speed and concurrency scale per level |
| Every 5th | Boss: one large alien with a phrase (includes spaces) | Slow descent; landing costs a life |

Word lists are baked into flash as C arrays, bucketed by length. The picker interface is a module boundary so an adaptive picker (serving words containing the player's fumbled chords) can be swapped in later.

### Fading hints

- **Levels 1–3:** every letter in a banner shows its two-color chord as two small colored dots beneath it.
- **Levels 4–6:** only the next letter to type shows its dots.
- **Level 7+:** no hints.
- **Mercy rule:** any letter fumbled twice in a row temporarily regains its dots.

### Score, streak, lives

- Score = base points per letter × word-length bonus × streak multiplier.
- Streak multiplier grows with consecutive flawless words, caps at ×8, resets on any wrong press.
- 3 lives. Life lost when any alien crosses the defense line.

### Smart bomb

Once per level: shaking the device (IMU) triggers a screen flash, 300 ms haptic thump, white LED burst, and destroys all on-screen aliens **for zero points** — a panic tool, not a scoring tool.

### WPM counter

- **Live HUD:** WPM = (correct characters ÷ 5) per minute over a rolling 15-second window, updated once per second. Wrong presses don't count.
- **Game-over stats card:** average WPM for the run, accuracy %, best streak, final score.
- **Records:** each high-score entry stores its WPM; a separate all-time Best WPM record persists in flash and appears in the title-screen ticker.

## Aliens

ASCII art drawn glyph-by-glyph with BSP gfx text rendering; each species has a 2-frame wiggle animation.

| Species | Color | Shape/size | Behavior |
|---|---|---|---|
| Drifter | Green | Small squid, ~3 rows | Slow, common |
| Zigzag | Cyan | Crab | Sine-wave sway, slightly faster |
| Diver | Magenta | Narrow rocket | Rare, fast fall, short word — urgency spike |
| Shielded | Orange | Chunky tank | Word must be typed twice (banner refills after shield pops) |
| Boss | Red | Large multi-row monster with health bar | Every 5th level; phrase typed word-by-word (a chunk = one whitespace-separated word, the space typed to confirm it), each completed chunk knocks off a visible piece of its body |

Word banner sits under each alien in a high-contrast box: typed letters dimmed, next letter bright (with chord dots per hint schedule).

## Screens

One state machine: **Title** (logo, "press any chord to start", high-score + Best WPM ticker) → **Level intro card** → **Game** → **Game Over** (stats card) → **Initials entry** (3 chars via chordboard — deliberate final practice) → **High-score table** → Title.

## Audio

Synthesized chiptune, no asset files. BSP audio is fixed 16 kHz I2S with DMA (`audio_i2s_duplex_play_stream_loop`, 8192-frame chunks). A simple 2-channel mixer renders square/noise voices into the stream buffer.

- Letter zap: square wave, pitch rises with streak (audible combo feedback)
- Explosion: noise burst; deeper boom for bosses
- Wrong letter: error blip
- Level-up: arpeggio jingle; game over: descending sting
- Title screen: short looping bassline; in-game is music-free so SFX pitch cues stay readable

## LEDs (16-pixel WS2812)

Ambient layer only — never required to play:

- Idle: dim glow in the level's color
- Chord press: strip segment flashes that button's color (color-memory reinforcement)
- Alien within ⅓ of defense line: pulsing red warning
- Level-up: rainbow sweep; smart bomb: white burst

## Haptics

Haptic motor is GPIO 46; the BSP driver is TODO, so the game drives the pin directly (on/off timing, no PWM in v1) via a small pattern-queue helper.

- Wrong letter: short tick
- Life lost: medium pulse
- Smart bomb: 300 ms thump
- Boss kill: double buzz

## Architecture

### Project layout

Standalone repo; `wilibsp` as a git submodule. The game is an app linking the `freewili2_bsp` static library — same CMake + Pico SDK 2.x + ARM GCC pattern as wilibsp's `hello_*` apps.

### Modules

Pure-logic modules are hardware-free and host-testable.

| Module | Purpose |
|---|---|
| `main.c` | Init, fixed-timestep loop (~30 fps), screen state machine |
| `game.c` | Rules: spawning, falling, targeting, scoring, streaks, lives, levels, WPM stats |
| `aliens.c` | Species tables: ASCII frames, colors, speeds, word-length buckets |
| `words.c` | Word lists + picker behind a small interface (swappable later) |
| `hints.c` | Letter → two-color chord lookup, derived from `fw2kb` layout tables |
| `render.c` | All drawing via BSP gfx: aliens, banners, HUD, screens |
| `sfx.c` | 2-channel chiptune synth filling the I2S DMA stream |
| `ledfx.c` | WS2812 effects |
| `haptic.c` | GPIO 46 pulse helper with pattern queue |
| `hiscore.c` | Top-10 (score, initials, WPM) + Best WPM in last flash sector (magic + version + CRC) |
| `shake.c` | BMI323 accel polling → debounced shake event |

### Data flow

Input events (`fw2kb` letters, shake) feed `game.c`, which owns all game state. Each tick, `game.c` emits events; `render.c`, `sfx.c`, `ledfx.c`, and `haptic.c` are read-only consumers. Gameplay logic never touches hardware directly.

### Error handling

- Flash read with bad CRC → empty high-score table, never a crash.
- Audio or LED init failure → game runs silent/dark rather than refusing to boot.
- Word bucket exhaustion → recycle with reshuffle.

## Testing

- **Host unit tests** (wilibsp `tests/` pattern, no hardware): game rules, scoring math, streak/WPM calculations, word picker, chord-hint mapping, hiscore serialization round-trips.
- **On-target:** BSP `hello_*` apps to sanity-check each peripheral, then a manual playtest checklist per screen.

## Out of scope (v1)

- Adaptive word picker (serve words with fumbled chords) — designed-for via `words.c` interface, not built.
- Practice drill modes / WPM test menu.
- Music during gameplay; PWM haptic intensity; touchscreen input.
