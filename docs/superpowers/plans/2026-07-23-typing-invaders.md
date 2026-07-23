# Typing Invaders Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A native-C arcade typing game for the FreeWili 2 that teaches the two-press chord keyboard: colored ASCII aliens fall, you type their words to kill them.

**Architecture:** Standalone repo with `wilibsp` as a git submodule; the game is one app (`apps/typing_invaders`) linking the `freewili2_bsp` static library. All game rules (`game.c`, `words.c`, `hints.c`, `sfx.c` synth, `hiscore.c` codec, `shake.c`, `fb.c`) are pure C with no hardware includes, unit-tested on the host via a standalone CTest tree; hardware bindings (`main.c`, `render.c` flush, `sfx_ring.c`, `ledfx.c`, `haptic.c`, `hiscore_flash.c`) are thin consumers.

**Tech Stack:** Pico SDK 2.x, ARM GCC, CMake + Ninja, wilibsp BSP (ST7796 display, fw2kb chord keyboard via uartkbd, WS2812 LEDs, NAU88C10 I2S audio, BMI323 IMU), host tests with MinGW GCC + CTest.

## Global Constraints

Copied from the spec and `wilibsp/AGENTS.md` — every task implicitly includes these:

- **Never pass `-DPICO_BOARD` on any cmake command line.** Board selection is `set(PICO_BOARD freewili2)` in the top-level CMakeLists only.
- **Diagnostics via `DIAG(...)` (SEGGER RTT) only** — no printf/stdio, no `%f` (RTT printf has no float support).
- **Any new DMA_IRQ_0 user must use `irq_add_shared_handler`** — never exclusive (display flush shares it).
- **Every app binary is `pico_set_binary_type(<app> copy_to_ram)`** — all code+data in 512 KB SRAM. Large buffers (the two 307 KB framebuffers) live in PSRAM (`PSRAM_BASE 0x11000000`, 8 MB). The 32 KB audio ring lives in SRAM (flash writes stall the QMI that serves PSRAM).
- **Audio is fixed 16 kHz I2S** (actual rate 16009 Hz); stream buffers are multiples of 8192 frames; frame = uint32 `[L16|R16]`.
- **LED count = 16** (`FW2_LED_COUNT`), WS2812 on pio1, `PIN_LED_DATA` (GPIO 21).
- **Haptic motor = GPIO 46**, no BSP driver — the game drives the pin directly (on/off only, no PWM).
- **High-score flash sector = last 4 KB** of the 16 MB flash. Write only while audio is stopped and no display flush is in progress (flash ops stall the QMI → PSRAM reads).
- **Host tests are a standalone CMake tree** (`tests/`) with no Pico SDK; pure modules must compile with only libc.
- **Game text is lowercase `a`–`y` plus space** — `z` is not on the fw2kb LOWER page; no word or phrase may contain `z`.
- **Conventional Commits**: `feat:`, `fix:`, `test:`, `docs:`, imperative subject.
- Commands: `python tools/fw.py build|flash|rtt|test` from the repo root (our patched copy of wilibsp's runner). `fw test` needs MinGW GCC + Ninja on PATH (Windows); `fw build` needs ARM GCC + Pico SDK (set `PICO_SDK_PATH`, or the build fetches it from git).

---

### Task 1: Repo scaffold and bootable app skeleton

**Files:**
- Create: `.gitignore`
- Create: `CMakeLists.txt`
- Create: `CMakePresets.json`
- Create: `tools/fw.py` (patched copy of `wilibsp/tools/fw.py`), `tools/fw`, `tools/fw.cmd`
- Create: `apps/typing_invaders/CMakeLists.txt`
- Create: `apps/typing_invaders/main.c`

**Interfaces:**
- Consumes: `wilibsp/bsp` (`freewili2_bsp` CMake target, `fw2.h`), `wilibsp/pico_sdk_import.cmake`
- Produces: a building, flashable app target named `typing_invaders`; `python tools/fw.py build` / `flash` / `test` working from our repo root. Later tasks add sources to `apps/typing_invaders/CMakeLists.txt`.

- [ ] **Step 1: Write `.gitignore`**

```gitignore
build/
build-tests/
*.uf2
*.elf
*.bin
__pycache__/
.venv/
```

- [ ] **Step 2: Write top-level `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.13)

# RP2350B board header (PICO_RP2350A=0 -> 48 GPIO, 16MB flash).
# Do NOT pass -DPICO_BOARD on the command line - that overrides this.
set(PICO_BOARD freewili2 CACHE STRING "Board type")
list(APPEND PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/wilibsp/bsp/boards")

# Fall back to fetching the Pico SDK when no local install is configured.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT PICO_SDK_PATH)
    set(PICO_SDK_FETCH_FROM_GIT on)
endif()
include(wilibsp/pico_sdk_import.cmake)

project(typinginvaders C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_subdirectory(wilibsp/bsp)
add_subdirectory(apps/typing_invaders)
```

- [ ] **Step 3: Write `CMakePresets.json`** (same shape as wilibsp's)

```json
{
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 21, "patch": 0 },
  "configurePresets": [
    {
      "name": "target",
      "displayName": "FreeWili2 target (RP2350B)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" }
    }
  ],
  "buildPresets": [
    { "name": "target", "configurePreset": "target" }
  ]
}
```

- [ ] **Step 4: Copy and patch the `fw` runner**

Copy three files, then patch two constants in the copy:

```powershell
Copy-Item wilibsp\tools\fw.py tools\fw.py
Copy-Item wilibsp\tools\fw tools\fw
Copy-Item wilibsp\tools\fw.cmd tools\fw.cmd
```

In `tools/fw.py`, change exactly these two lines (near the top):

```python
DEFAULT_APP = "hello_display"
OPENOCD_CFG = str(REPO_ROOT / "tools" / "openocd" / "freewili2.cfg")
```

to:

```python
DEFAULT_APP = "typing_invaders"
OPENOCD_CFG = str(REPO_ROOT / "wilibsp" / "tools" / "openocd" / "freewili2.cfg")
```

`REPO_ROOT` self-resolves to our repo (it is `parents[1]` of the script), so `fw build`, `fw flash`, `fw test` (uses `tests/`, `build-tests/`) all work unmodified. The `new-app` subcommand references `apps/template`, which we don't have — it is unused; leave it.

- [ ] **Step 5: Write `apps/typing_invaders/CMakeLists.txt`**

```cmake
add_executable(typing_invaders
    main.c)
target_link_libraries(typing_invaders freewili2_bsp hardware_flash)
pico_set_binary_type(typing_invaders copy_to_ram)   # required: firmware runs from SRAM
pico_add_extra_outputs(typing_invaders)             # .uf2 / .bin / .map
```

- [ ] **Step 6: Write skeleton `apps/typing_invaders/main.c`** (boot proof: title text on screen, DIAG heartbeat)

```c
// typing_invaders - main entry. Task 1 skeleton: bring the board up, prove
// display + PSRAM + RTT, and show a placeholder title screen.
#include "fw2.h"
#include "platform/diag.h"
#include "platform/psram.h"
#include "pico/stdlib.h"

static inline uint16_t rgb565_be(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

int main(void) {
    board_init();
    size_t psram_bytes = psram_init();
    st7796_init();
    board_backlight_set(1);
    st7796_fill_screen(rgb565_be(0, 0, 24));
    st7796_draw_text(60, 120, 4, rgb565_be(120, 255, 120), rgb565_be(0, 0, 24),
                     "TYPING");
    st7796_draw_text(60, 160, 4, rgb565_be(255, 80, 80), rgb565_be(0, 0, 24),
                     "INVADERS");
    DIAG("typing_invaders skeleton up, psram=%u bytes\n", (unsigned)psram_bytes);
    for (;;) {
        sleep_ms(1000);
        DIAG("tick\n");
    }
}
```

- [ ] **Step 7: Configure and build**

Run: `cmake --preset target` then `python tools/fw.py build`
Expected: configures (fetches Pico SDK on first run if `PICO_SDK_PATH` unset — slow once), builds `build/apps/typing_invaders/typing_invaders.elf` with zero errors. If ARM GCC is missing, install `arm-none-eabi-gcc` first and re-run; do not proceed until the build passes.

- [ ] **Step 8: Commit**

```bash
git add .gitignore CMakeLists.txt CMakePresets.json tools apps
git commit -m "feat: scaffold typing_invaders app against wilibsp BSP"
```

---

### Task 2: Host test harness + chord-hint module

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_util.h` (copy of `wilibsp/tests/test_util.h`)
- Create: `apps/typing_invaders/hints.h`, `apps/typing_invaders/hints.c`
- Create: `tests/test_hints.c`
- Modify: `apps/typing_invaders/CMakeLists.txt` (add `hints.c`)

**Interfaces:**
- Consumes: fw2kb LOWER page layout fact: groups `abcde|fghij|klmno|pqrst|uvwxy` on buttons 0..4 (GRAY, YELLOW, GREEN, BLUE, RED).
- Produces: `bool hints_chord(char c, int *first, int *second)` — both out-params in 0..4; returns false for anything outside `'a'..'y'`. Used by `render.c` (dots) and tests.

- [ ] **Step 1: Copy the assert helper**

```powershell
Copy-Item wilibsp\tests\test_util.h tests\test_util.h
```

- [ ] **Step 2: Write `tests/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.13)
project(typinginvaders_tests C)
set(CMAKE_C_STANDARD 11)
enable_testing()

# Standalone tree: no Pico SDK. Pure-logic game modules are compiled directly
# against the host C compiler so tests run natively with no hardware.
set(APPSRC ${CMAKE_CURRENT_SOURCE_DIR}/../apps/typing_invaders)
set(BSPDIR ${CMAKE_CURRENT_SOURCE_DIR}/../wilibsp/bsp)

add_executable(test_hints test_hints.c ${APPSRC}/hints.c)
target_include_directories(test_hints PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME hints COMMAND test_hints)
```

- [ ] **Step 3: Write the failing test `tests/test_hints.c`**

```c
#include "test_util.h"
#include "hints.h"

int main(void) {
    int a, b;
    ASSERT_TRUE(hints_chord('a', &a, &b)); ASSERT_EQ(a, 0); ASSERT_EQ(b, 0);
    ASSERT_TRUE(hints_chord('e', &a, &b)); ASSERT_EQ(a, 0); ASSERT_EQ(b, 4);
    ASSERT_TRUE(hints_chord('f', &a, &b)); ASSERT_EQ(a, 1); ASSERT_EQ(b, 0);
    ASSERT_TRUE(hints_chord('m', &a, &b)); ASSERT_EQ(a, 2); ASSERT_EQ(b, 2);
    ASSERT_TRUE(hints_chord('t', &a, &b)); ASSERT_EQ(a, 3); ASSERT_EQ(b, 4);
    ASSERT_TRUE(hints_chord('y', &a, &b)); ASSERT_EQ(a, 4); ASSERT_EQ(b, 4);
    ASSERT_TRUE(!hints_chord('z', &a, &b));   /* z is not on the LOWER page */
    ASSERT_TRUE(!hints_chord('A', &a, &b));
    ASSERT_TRUE(!hints_chord(' ', &a, &b));
    ASSERT_TRUE(!hints_chord(0, &a, &b));
    TEST_RETURN();
}
```

- [ ] **Step 4: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `hints.h` not found / link error (module doesn't exist yet).

- [ ] **Step 5: Write `apps/typing_invaders/hints.h` and `hints.c`**

```c
// hints.h - letter -> two-press chord on the fw2kb LOWER page.
// Buttons: 0=GRAY 1=YELLOW 2=GREEN 3=BLUE 4=RED. First press picks the group
// (abcde|fghij|klmno|pqrst|uvwxy), second picks the letter within the group.
// 'z' lives on another page and is never used by the game.
#ifndef HINTS_H
#define HINTS_H
#include <stdbool.h>

bool hints_chord(char c, int *first, int *second);

#endif
```

```c
#include "hints.h"

bool hints_chord(char c, int *first, int *second)
{
    if (c < 'a' || c > 'y') return false;
    int i = c - 'a';
    *first  = i / 5;
    *second = i % 5;
    return true;
}
```

- [ ] **Step 6: Add `hints.c` to the app**

In `apps/typing_invaders/CMakeLists.txt` change the source list to:

```cmake
add_executable(typing_invaders
    main.c hints.c)
```

- [ ] **Step 7: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (1 test: hints).

- [ ] **Step 8: Commit**

```bash
git add tests apps/typing_invaders/hints.h apps/typing_invaders/hints.c apps/typing_invaders/CMakeLists.txt
git commit -m "feat: host test harness and chord-hint lookup"
```

---

### Task 3: Word lists and picker

**Files:**
- Create: `apps/typing_invaders/words.h`, `apps/typing_invaders/words.c`
- Create: `tests/test_words.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `void words_init(uint32_t seed)`, `const char *words_pick(word_bucket b)` (never repeats a bucket's previous pick), `int words_count(word_bucket b)`, enum `word_bucket { WB_LETTERS, WB_SHORT, WB_LONG, WB_PHRASE, WB_COUNT }`. `game.c` calls these; the interface is the swap point for a future adaptive picker.

- [ ] **Step 1: Write the failing test `tests/test_words.c`**

```c
#include "test_util.h"
#include "words.h"
#include "hints.h"
#include <string.h>

/* every word char must be typeable on the LOWER page (a-y), or a space in
 * phrases; no word may exceed 39 chars (game word buffer is 40). */
static void check_bucket(word_bucket b, int allow_space) {
    int a, s;
    for (int k = 0; k < 200; k++) {
        const char *w = words_pick(b);
        ASSERT_TRUE(w != 0);
        ASSERT_TRUE(strlen(w) >= 1 && strlen(w) <= 39);
        for (const char *p = w; *p; p++)
            ASSERT_TRUE(hints_chord(*p, &a, &s) || (allow_space && *p == ' '));
    }
}

int main(void) {
    words_init(1234);
    ASSERT_EQ(words_count(WB_LETTERS), 25);
    ASSERT_TRUE(words_count(WB_SHORT) >= 30);
    ASSERT_TRUE(words_count(WB_LONG) >= 30);
    ASSERT_TRUE(words_count(WB_PHRASE) >= 8);
    check_bucket(WB_LETTERS, 0);
    check_bucket(WB_SHORT, 0);
    check_bucket(WB_LONG, 0);
    check_bucket(WB_PHRASE, 1);
    /* no immediate repeats */
    const char *prev = words_pick(WB_SHORT);
    for (int k = 0; k < 100; k++) {
        const char *w = words_pick(WB_SHORT);
        ASSERT_TRUE(w != prev);
        prev = w;
    }
    /* word length ranges */
    for (int k = 0; k < 100; k++) {
        ASSERT_EQ((int)strlen(words_pick(WB_LETTERS)), 1);
        size_t ls = strlen(words_pick(WB_SHORT));
        ASSERT_TRUE(ls >= 3 && ls <= 4);
        size_t ll = strlen(words_pick(WB_LONG));
        ASSERT_TRUE(ll >= 5 && ll <= 8);
    }
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_words test_words.c ${APPSRC}/words.c ${APPSRC}/hints.c)
target_include_directories(test_words PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME words COMMAND test_words)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `words.h` not found.

- [ ] **Step 3: Write `apps/typing_invaders/words.h` and `words.c`**

```c
// words.h - baked-in word lists + picker. All content lowercase a-y (no 'z',
// not typeable on the fw2kb LOWER page); phrases may contain spaces.
// This interface is the swap point for a future adaptive picker.
#ifndef WORDS_H
#define WORDS_H
#include <stdint.h>

typedef enum { WB_LETTERS = 0, WB_SHORT, WB_LONG, WB_PHRASE, WB_COUNT } word_bucket;

void        words_init(uint32_t seed);
const char *words_pick(word_bucket b);
int         words_count(word_bucket b);

#endif
```

```c
#include "words.h"

static const char *k_letters[] = {
    "a","b","c","d","e","f","g","h","i","j","k","l","m",
    "n","o","p","q","r","s","t","u","v","w","x","y" };

static const char *k_short[] = {
    "cat","dog","sun","map","key","run","jet","fox","owl","bat",
    "ray","ice","ant","bee","cow","egg","fig","gum","hat","ink",
    "jar","kit","log","mud","net","oak","pig","sky","tin","van",
    "wax","yak","star","moon","fire","wind","leaf","rain","snow","bolt",
    "gear","wire","code","byte","chip","ship","warp","beam" };

static const char *k_long[] = {
    "planet","rocket","galaxy","cosmic","photon","meteor","gravity","stellar",
    "nebula","quasar","plasma","asteroid","starship","invader","defense","capture",
    "blaster","cannon","charge","combat","crater","docking","engine","fusion",
    "launch","module","orbits","payload","reactor","saucer","shields","thruster",
    "vector","voyage","weapons","airlock" };

static const char *k_phrase[] = {
    "the quick brown fox",
    "type or be typed",
    "aliens hate typos",
    "chord your way out",
    "fast fingers win",
    "the mother ship arrives",
    "words are your weapon",
    "the final wave is here",
    "press it like you mean it",
    "boss battle commence" };

static const char **k_lists[WB_COUNT] = { k_letters, k_short, k_long, k_phrase };
static const int k_counts[WB_COUNT] = {
    sizeof k_letters / sizeof *k_letters,
    sizeof k_short   / sizeof *k_short,
    sizeof k_long    / sizeof *k_long,
    sizeof k_phrase  / sizeof *k_phrase };

static uint32_t s_rng = 1;
static int s_prev[WB_COUNT] = { -1, -1, -1, -1 };

static uint32_t next_rnd(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

void words_init(uint32_t seed)
{
    s_rng = seed ? seed : 1;
    for (int i = 0; i < WB_COUNT; i++) s_prev[i] = -1;
}

int words_count(word_bucket b) { return k_counts[b]; }

const char *words_pick(word_bucket b)
{
    int n = k_counts[b];
    int i = (int)(next_rnd() % (uint32_t)n);
    if (n > 1 && i == s_prev[b]) i = (i + 1) % n;
    s_prev[b] = i;
    return k_lists[b][i];
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (2 tests).

- [ ] **Step 5: Add `words.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c)
```

```bash
git add apps/typing_invaders/words.h apps/typing_invaders/words.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: word lists and non-repeating picker"
```

---

### Task 4: Alien species tables

**Files:**
- Create: `apps/typing_invaders/aliens.h`, `apps/typing_invaders/aliens.c`
- Create: `tests/test_aliens.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `species_t { SP_DRIFTER, SP_ZIGZAG, SP_DIVER, SP_SHIELDED, SP_BOSS, SP_COUNT }`, `const alien_def_t *alien_def(species_t)`, `species_t alien_choose(int level, uint32_t rnd)`. `alien_def_t` fields used by `game.c` (speed_pct, score_pct, rows, cols) and `render.c` (art, r/g/b).

- [ ] **Step 1: Write the failing test `tests/test_aliens.c`**

```c
#include "test_util.h"
#include "aliens.h"
#include <string.h>

int main(void) {
    /* art integrity: both frames, every row exactly `cols` chars, `rows` rows */
    for (int sp = 0; sp < SP_COUNT; sp++) {
        const alien_def_t *d = alien_def((species_t)sp);
        ASSERT_TRUE(d != 0);
        ASSERT_TRUE(d->rows >= 2 && d->rows <= 4);
        ASSERT_TRUE(d->cols >= 3 && d->cols <= 11);
        for (int f = 0; f < 2; f++)
            for (int r = 0; r < d->rows; r++) {
                ASSERT_TRUE(d->art[f][r] != 0);
                ASSERT_EQ((int)strlen(d->art[f][r]), d->cols);
            }
        ASSERT_TRUE(d->speed_pct > 0 && d->score_pct > 0);
    }
    /* level gating: level 1 only drifters; boss never chosen */
    for (uint32_t r = 0; r < 500; r++) {
        ASSERT_EQ(alien_choose(1, r * 2654435761u), SP_DRIFTER);
        species_t s = alien_choose(9, r * 2654435761u);
        ASSERT_TRUE(s != SP_BOSS);
    }
    /* by level 5 all four regular species appear */
    int seen[SP_COUNT] = { 0 };
    for (uint32_t r = 0; r < 2000; r++) seen[alien_choose(5, r * 2654435761u)]++;
    ASSERT_TRUE(seen[SP_DRIFTER] > 0);
    ASSERT_TRUE(seen[SP_ZIGZAG] > 0);
    ASSERT_TRUE(seen[SP_DIVER] > 0);
    ASSERT_TRUE(seen[SP_SHIELDED] > 0);
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_aliens test_aliens.c ${APPSRC}/aliens.c)
target_include_directories(test_aliens PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME aliens COMMAND test_aliens)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `aliens.h` not found.

- [ ] **Step 3: Write `apps/typing_invaders/aliens.h` and `aliens.c`**

```c
// aliens.h - species tables: 2-frame ASCII art, color, speed/score scaling,
// spawn gating. Pure data, host-testable.
#ifndef ALIENS_H
#define ALIENS_H
#include <stdint.h>

typedef enum {
    SP_DRIFTER = 0, SP_ZIGZAG, SP_DIVER, SP_SHIELDED, SP_BOSS, SP_COUNT
} species_t;

#define ALIEN_ART_ROWS 4

typedef struct {
    const char *art[2][ALIEN_ART_ROWS];  /* [frame][row]; rows..ALIEN_ART_ROWS-1 unused */
    int     rows, cols;                  /* art size in character cells */
    uint8_t r, g, b;                     /* species color */
    int     speed_pct;                   /* % of level base fall speed */
    int     score_pct;                   /* % of base word score */
    int     min_level;                   /* first level this species may spawn */
    int     weight;                      /* spawn weight (SP_BOSS: unused) */
} alien_def_t;

const alien_def_t *alien_def(species_t sp);
species_t alien_choose(int level, uint32_t rnd);   /* weighted non-boss pick */

#endif
```

```c
#include "aliens.h"

static const alien_def_t k_defs[SP_COUNT] = {
    [SP_DRIFTER] = {
        .art = { { " /^\\ ", "(o o)", " )~( " },
                 { " /^\\ ", "(o o)", " (~) " } },
        .rows = 3, .cols = 5, .r = 0, .g = 255, .b = 80,
        .speed_pct = 100, .score_pct = 100, .min_level = 1, .weight = 50 },
    [SP_ZIGZAG] = {
        .art = { { "{o.o}", "/|_|\\" },
                 { "{o.o}", "\\|_|/" } },
        .rows = 2, .cols = 5, .r = 0, .g = 220, .b = 220,
        .speed_pct = 120, .score_pct = 120, .min_level = 2, .weight = 30 },
    [SP_DIVER] = {
        .art = { { " ^ ", "|||", " W " },
                 { " ^ ", "|||", " M " } },
        .rows = 3, .cols = 3, .r = 255, .g = 60, .b = 255,
        .speed_pct = 220, .score_pct = 200, .min_level = 5, .weight = 10 },
    [SP_SHIELDED] = {
        .art = { { "[###]", "(o_o)", "[###]" },
                 { "[###]", "(o-o)", "[###]" } },
        .rows = 3, .cols = 5, .r = 255, .g = 150, .b = 0,
        .speed_pct = 70, .score_pct = 180, .min_level = 4, .weight = 20 },
    [SP_BOSS] = {
        .art = { { " /MMMMMMM\\ ", "|  O   O  |", " \\_______/ ", "  v v v v  " },
                 { " /MMMMMMM\\ ", "|  -   -  |", " \\_______/ ", "  w w w w  " } },
        .rows = 4, .cols = 11, .r = 255, .g = 40, .b = 40,
        .speed_pct = 25, .score_pct = 300, .min_level = 1, .weight = 0 },
};

const alien_def_t *alien_def(species_t sp)
{
    return (sp >= 0 && sp < SP_COUNT) ? &k_defs[sp] : 0;
}

species_t alien_choose(int level, uint32_t rnd)
{
    int total = 0;
    for (int sp = 0; sp < SP_BOSS; sp++)
        if (k_defs[sp].min_level <= level) total += k_defs[sp].weight;
    int pick = (int)(rnd % (uint32_t)total);
    for (int sp = 0; sp < SP_BOSS; sp++) {
        if (k_defs[sp].min_level > level) continue;
        pick -= k_defs[sp].weight;
        if (pick < 0) return (species_t)sp;
    }
    return SP_DRIFTER;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (3 tests).

- [ ] **Step 5: Add `aliens.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c)
```

```bash
git add apps/typing_invaders/aliens.h apps/typing_invaders/aliens.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: alien species tables with 2-frame ascii art"
```

---

### Task 5: Game core — waves, falling, lives

**Files:**
- Create: `apps/typing_invaders/game.h` (complete header, includes API used by Tasks 6-7)
- Create: `apps/typing_invaders/game.c` (init/start/spawn/tick/lives/waves + event ring)
- Create: `tests/test_game_core.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: `words_pick/words_init` (Task 3), `alien_def/alien_choose` (Task 4).
- Produces (used by Tasks 6-7, `render.c`, `main.c`): everything in `game.h` below. Positions are 24.8 fixed point (`x_q8 >> 8` = pixels). `game_tick` advances exactly one 33 ms step. Events pop via `game_next_event`.

- [ ] **Step 1: Write the complete `apps/typing_invaders/game.h`**

```c
// game.h - pure game rules for Typing Invaders. No hardware includes.
// Fixed timestep: one game_tick() == GAME_TICK_MS. Positions are 24.8 fixed
// point pixels. Consumers (render/sfx/led/haptic) read state + pop events.
#ifndef GAME_H
#define GAME_H
#include <stdbool.h>
#include <stdint.h>
#include "aliens.h"

#define GAME_TICK_MS        33
#define GAME_TICKS_PER_SEC  30
#define GAME_MAX_ALIENS     8
#define GAME_WORD_MAX       40
#define GAME_EVENT_RING     16
#define GAME_LIVES          3
#define GAME_FIELD_W        480
#define GAME_DEFENSE_Y      276     /* aliens crossing this line cost a life */
#define GAME_CHAR_RING      64
#define GAME_WPM_WINDOW     450     /* 15 s of ticks for the live WPM figure */

typedef enum {
    GE_NONE = 0,
    GE_ZAP,          /* correct letter; arg = streak multiplier 1..8 */
    GE_KILL,         /* alien destroyed by typing; arg = species_t */
    GE_WRONG,        /* wrong letter; arg = 0 */
    GE_SHIELD_POP,   /* shielded alien lost its shield; arg = 0 */
    GE_BOSS_CHUNK,   /* boss phrase word completed; arg = words remaining */
    GE_BOSS_KILL,    /* boss destroyed; arg = 0 */
    GE_LIFE_LOST,    /* arg = lives remaining */
    GE_LEVEL_UP,     /* wave cleared; arg = new level */
    GE_BOMB,         /* smart bomb fired; arg = aliens destroyed */
    GE_GAME_OVER,    /* arg = 0 (read score from game_t) */
} game_event_type;

typedef struct { game_event_type type; int32_t arg; } game_event_t;

typedef struct {
    bool      active;
    species_t sp;
    int32_t   x_q8, y_q8;       /* top-left, 24.8 fixed-point pixels */
    int32_t   home_x_q8;        /* sway centre (zigzag) */
    int32_t   vy_q8;            /* fall per tick */
    int32_t   phase;            /* sway phase, ticks */
    char      word[GAME_WORD_MAX];
    int       typed;            /* chars completed */
    bool      shield;           /* SP_SHIELDED: armor still up */
    int       w_px, h_px;       /* footprint incl. banner (set at spawn) */
} alien_t;

typedef struct {
    int      level;
    int      lives;
    uint32_t score;
    int      streak_mult;       /* 1..8 */
    int      flawless_words;
    bool     word_dirty;        /* current word had a miss */
    int      target;            /* locked alien index, -1 = none */
    bool     bomb_armed;
    int      spawn_left;
    int      spawn_cooldown;    /* ticks to next spawn */
    uint32_t ticks;
    uint32_t chars_typed;       /* correct chars, whole run */
    uint32_t chars_wrong;
    uint32_t char_ticks[GAME_CHAR_RING];  /* stamps of recent correct chars */
    int      char_head;
    char     fumble_ch;         /* mercy-hint tracking */
    int      fumble_n;
    bool     game_over;
    alien_t  aliens[GAME_MAX_ALIENS];
    game_event_t ev[GAME_EVENT_RING];
    int      ev_head, ev_count;
    uint32_t rng;
} game_t;

void game_init(game_t *g, uint32_t seed);
void game_start(game_t *g);                    /* new run at level 1 */
void game_tick(game_t *g);
void game_char(game_t *g, char c);             /* completed chord letter */
void game_shake(game_t *g);                    /* smart bomb request */
bool game_next_event(game_t *g, game_event_t *out);

/* exposed for internal reuse + tests: spawn a specific alien at column x_px
 * (word=NULL -> pick from the level's bucket). Returns NULL if field full. */
alien_t *game_force_spawn(game_t *g, species_t sp, const char *word, int x_px);

/* pure HUD queries */
int  game_wpm(const game_t *g);                /* rolling 15 s window */
int  game_avg_wpm(const game_t *g);            /* whole run */
int  game_accuracy_pct(const game_t *g);
bool game_hint_all(const game_t *g);           /* level <= 3: dots everywhere */
bool game_hint_next(const game_t *g);          /* level <= 6: dots on next char */
bool game_hint_mercy(const game_t *g, char c); /* fumbled-twice override */
bool game_is_boss_level(int level);

#endif
```

- [ ] **Step 2: Write the failing test `tests/test_game_core.c`**

```c
#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static int active_count(const game_t *g) {
    int n = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) n += g->aliens[i].active;
    return n;
}
static int drain_count(game_t *g, game_event_type t) {
    game_event_t e; int n = 0;
    while (game_next_event(g, &e)) if (e.type == t) n++;
    return n;
}

int main(void) {
    game_t g;
    words_init(42);
    game_init(&g, 42);
    game_start(&g);
    ASSERT_EQ(g.level, 1); ASSERT_EQ(g.lives, GAME_LIVES);
    ASSERT_TRUE(g.bomb_armed); ASSERT_TRUE(g.spawn_left > 0);

    /* aliens appear over time and respect the concurrency cap. 300 ticks
     * (10 s) is long enough to hit the cap but too short for any alien to
     * reach the defense line (~456 ticks at level-1 speed). */
    for (int t = 0; t < 300; t++) game_tick(&g);
    ASSERT_TRUE(active_count(&g) >= 1);
    ASSERT_TRUE(active_count(&g) <= 3);   /* level 1 cap = 2 + (1+1)/2 = 3 */

    /* falling: y grows each tick */
    int idx = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) if (g.aliens[i].active) { idx = i; break; }
    int32_t y0 = g.aliens[idx].y_q8;
    game_tick(&g);
    ASSERT_TRUE(g.aliens[idx].y_q8 > y0);

    /* crossing the defense line costs a life */
    (void)drain_count(&g, GE_NONE);   /* clear ring */
    g.aliens[idx].y_q8 = (GAME_DEFENSE_Y - 1) << 8;
    game_tick(&g);
    ASSERT_TRUE(!g.aliens[idx].active);
    ASSERT_EQ(g.lives, GAME_LIVES - 1);

    /* forcing all remaining lives away ends the game */
    game_t g2;
    game_init(&g2, 7); game_start(&g2);
    for (int life = 0; life < GAME_LIVES; life++) {
        alien_t *a = game_force_spawn(&g2, SP_DRIFTER, "cat", 100);
        ASSERT_TRUE(a != 0);
        a->y_q8 = (GAME_DEFENSE_Y - 1) << 8;
        game_tick(&g2);
    }
    ASSERT_TRUE(g2.game_over);
    ASSERT_EQ(drain_count(&g2, GE_GAME_OVER), 1);

    /* wave clear -> level up: exhaust spawns, kill everything via state */
    game_t g3;
    game_init(&g3, 9); game_start(&g3);
    g3.spawn_left = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) g3.aliens[i].active = false;
    game_tick(&g3);
    ASSERT_EQ(g3.level, 2);
    ASSERT_EQ(drain_count(&g3, GE_LEVEL_UP), 1);
    ASSERT_TRUE(g3.bomb_armed);

    /* boss level flag */
    ASSERT_TRUE(game_is_boss_level(5));
    ASSERT_TRUE(game_is_boss_level(10));
    ASSERT_TRUE(!game_is_boss_level(4));
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_game_core test_game_core.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_core PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_core COMMAND test_game_core)
```

- [ ] **Step 3: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `game.h`/`game.c` missing.

- [ ] **Step 4: Write `apps/typing_invaders/game.c` (core half)**

`game_char`, `game_shake`, and the stats/hint queries get real bodies in Tasks 6-7; this task stubs them so the file links (`game_char`/`game_shake` empty, queries return 0/false). Everything below is final code.

```c
#include "game.h"
#include "words.h"
#include <string.h>

#define CELL_W 12               /* render cell: 6*2 px */
#define CELL_H 16               /* 8*2 px */
#define BANNER_H 18             /* word banner strip under the art */

/* 32-step sway table, amplitude 24 px (zigzag species) */
static const int8_t k_sway[32] = {
      0,  5,  9, 13, 17, 20, 22, 24, 24, 24, 22, 20, 17, 13,  9,  5,
      0, -5, -9,-13,-17,-20,-22,-24,-24,-24,-22,-20,-17,-13, -9, -5 };

static uint32_t next_rnd(game_t *g)
{
    g->rng ^= g->rng << 13;
    g->rng ^= g->rng >> 17;
    g->rng ^= g->rng << 5;
    return g->rng;
}

void game_push_event(game_t *g, game_event_type t, int32_t arg)
{
    if (g->ev_count == GAME_EVENT_RING) {            /* drop oldest */
        g->ev_head = (g->ev_head + 1) % GAME_EVENT_RING;
        g->ev_count--;
    }
    int tail = (g->ev_head + g->ev_count) % GAME_EVENT_RING;
    g->ev[tail].type = t;
    g->ev[tail].arg = arg;
    g->ev_count++;
}

bool game_next_event(game_t *g, game_event_t *out)
{
    if (!g->ev_count) return false;
    *out = g->ev[g->ev_head];
    g->ev_head = (g->ev_head + 1) % GAME_EVENT_RING;
    g->ev_count--;
    return true;
}

bool game_is_boss_level(int level) { return level % 5 == 0; }

/* ---- level tuning ---- */
static int level_budget(int level)
{
    if (game_is_boss_level(level)) return 1;
    int n = 6 + 2 * level;
    return n > 20 ? 20 : n;
}
static int level_concurrent(int level)
{
    if (game_is_boss_level(level)) return 1;
    int n = 2 + (level + 1) / 2;
    return n > 6 ? 6 : n;
}
static int32_t level_base_vy_q8(int level)
{
    int32_t v = 118 + 12 * (level - 1);
    return v > 512 ? 512 : v;
}
static word_bucket level_bucket(int level)
{
    if (level <= 2) return WB_LETTERS;
    if (level <= 5) return WB_SHORT;
    return WB_LONG;
}
static int spawn_gap_ticks(game_t *g)
{
    int base = 45 - 2 * g->level;
    if (base < 15) base = 15;
    return base + (int)(next_rnd(g) % 21) - 10;      /* +/- 10 tick jitter */
}

static void wave_setup(game_t *g)
{
    g->spawn_left = level_budget(g->level);
    g->spawn_cooldown = 20;
    g->bomb_armed = true;
}

void game_init(game_t *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->rng = seed ? seed : 1;
    g->target = -1;
}

void game_start(game_t *g)
{
    uint32_t rng = g->rng;
    memset(g, 0, sizeof *g);
    g->rng = rng;
    g->level = 1;
    g->lives = GAME_LIVES;
    g->streak_mult = 1;
    g->target = -1;
    wave_setup(g);
}

alien_t *game_force_spawn(game_t *g, species_t sp, const char *word, int x_px)
{
    int idx = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++)
        if (!g->aliens[i].active) { idx = i; break; }
    if (idx < 0) return 0;

    alien_t *a = &g->aliens[idx];
    const alien_def_t *d = alien_def(sp);
    memset(a, 0, sizeof *a);
    a->active = true;
    a->sp = sp;
    if (!word) {
        word_bucket b = (sp == SP_BOSS) ? WB_PHRASE
                       : (sp == SP_DIVER) ? WB_SHORT
                       : level_bucket(g->level);
        word = words_pick(b);
    }
    strncpy(a->word, word, GAME_WORD_MAX - 1);
    int wlen = (int)strlen(a->word);
    int cells = d->cols > wlen ? d->cols : wlen;
    a->w_px = cells * CELL_W;
    a->h_px = d->rows * CELL_H + BANNER_H;
    a->shield = (sp == SP_SHIELDED);
    a->vy_q8 = level_base_vy_q8(g->level) * d->speed_pct / 100;
    int max_x = GAME_FIELD_W - a->w_px - 16;
    if (x_px < 8) x_px = 8;
    if (x_px > max_x) x_px = max_x;
    a->x_q8 = a->home_x_q8 = x_px << 8;
    a->y_q8 = 0;
    return a;
}

static void spawn_from_wave(game_t *g)
{
    species_t sp = game_is_boss_level(g->level) ? SP_BOSS
                                                : alien_choose(g->level, next_rnd(g));
    int x = 8 + (int)(next_rnd(g) % 400);
    if (game_force_spawn(g, sp, 0, x)) {
        g->spawn_left--;
        g->spawn_cooldown = spawn_gap_ticks(g);
    }
}

static void alien_deactivate(game_t *g, int i)
{
    g->aliens[i].active = false;
    if (g->target == i) g->target = -1;
}

void game_tick(game_t *g)
{
    if (g->game_over) return;
    g->ticks++;

    int active = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) active += g->aliens[i].active;

    if (g->spawn_left > 0 && active < level_concurrent(g->level)) {
        if (--g->spawn_cooldown <= 0) { spawn_from_wave(g); active++; }
    }

    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        alien_t *a = &g->aliens[i];
        if (!a->active) continue;
        a->y_q8 += a->vy_q8;
        if (a->sp == SP_ZIGZAG) {
            a->phase++;
            int32_t sway = (int32_t)k_sway[(a->phase >> 2) & 31] << 8;
            a->x_q8 = a->home_x_q8 + sway;
            if (a->x_q8 < 8 << 8) a->x_q8 = 8 << 8;
            int32_t max_x = (GAME_FIELD_W - a->w_px - 8) << 8;
            if (a->x_q8 > max_x) a->x_q8 = max_x;
        }
        if ((a->y_q8 >> 8) + a->h_px >= GAME_DEFENSE_Y) {
            alien_deactivate(g, i);
            g->lives--;
            game_push_event(g, GE_LIFE_LOST, g->lives);
            g->streak_mult = 1;
            g->flawless_words = 0;
            if (g->lives <= 0) {
                g->game_over = true;
                game_push_event(g, GE_GAME_OVER, 0);
                return;
            }
        }
    }

    active = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) active += g->aliens[i].active;
    if (g->spawn_left == 0 && active == 0) {
        g->level++;
        wave_setup(g);
        game_push_event(g, GE_LEVEL_UP, g->level);
    }
}

/* ---- Task 6/7 fill these in ---- */
void game_char(game_t *g, char c) { (void)g; (void)c; }
void game_shake(game_t *g) { (void)g; }
int  game_wpm(const game_t *g) { (void)g; return 0; }
int  game_avg_wpm(const game_t *g) { (void)g; return 0; }
int  game_accuracy_pct(const game_t *g) { (void)g; return 100; }
bool game_hint_all(const game_t *g) { return g->level <= 3; }
bool game_hint_next(const game_t *g) { return g->level <= 6; }
bool game_hint_mercy(const game_t *g, char c) { (void)g; (void)c; return false; }
```

- [ ] **Step 5: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (4 tests).

- [ ] **Step 6: Add `game.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c)
```

```bash
git add apps/typing_invaders/game.h apps/typing_invaders/game.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: game core - waves, falling aliens, lives, level-up"
```

---

### Task 6: Game typing — targeting, scoring, streaks

**Files:**
- Modify: `apps/typing_invaders/game.c` (replace the `game_char` stub; add statics)
- Create: `tests/test_game_typing.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 5's `game_t`, `game_push_event`, `alien_deactivate` (file-local), `game_force_spawn`.
- Produces: working `game_char` semantics relied on by Task 7 and `main.c`: lock-on, GE_ZAP/GE_WRONG/GE_KILL/GE_SHIELD_POP events, scoring `10 * len * score_pct/100 * streak_mult`, streak `min(8, 1 + flawless_words)`.

- [ ] **Step 1: Write the failing test `tests/test_game_typing.c`**

```c
#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static game_event_t last_of(game_t *g, game_event_type t, int *count) {
    game_event_t e, found = { GE_NONE, 0 };
    if (count) *count = 0;
    while (game_next_event(g, &e))
        if (e.type == t) { found = e; if (count) (*count)++; }
    return found;
}
static void type_word(game_t *g, const char *w) {
    for (const char *p = w; *p; p++) game_char(g, *p);
}

int main(void) {
    game_t g; int n;
    words_init(1); game_init(&g, 1); game_start(&g);
    g.spawn_left = 0;   /* manual spawns only */

    /* lock-on: first matching letter targets the alien and zaps */
    alien_t *a = game_force_spawn(&g, SP_DRIFTER, "cat", 50);
    game_char(&g, 'c');
    ASSERT_EQ(g.target, (int)(a - g.aliens));
    ASSERT_EQ(a->typed, 1);
    game_event_t z = last_of(&g, GE_ZAP, &n);
    ASSERT_EQ(n, 1); ASSERT_EQ(z.arg, 1);

    /* wrong letter: streak broken, target unlocked, event */
    game_char(&g, 'x');
    ASSERT_EQ(g.target, -1);
    last_of(&g, GE_WRONG, &n); ASSERT_EQ(n, 1);
    ASSERT_EQ(g.streak_mult, 1);
    ASSERT_EQ((int)g.chars_wrong, 1);

    /* re-lock and finish: kill + score 10*3*100%*1 = 30 */
    game_char(&g, 'c');           /* word restarts? no - typed stays at 1 */
    ASSERT_EQ(g.target, -1);      /* 'c' is not the next letter ('a' is) */
    last_of(&g, GE_WRONG, &n); ASSERT_EQ(n, 1);
    game_char(&g, 'a');           /* matches aliens[0] at typed=1 */
    ASSERT_EQ(g.target, (int)(a - g.aliens));
    game_char(&g, 't');
    ASSERT_TRUE(!a->active);
    last_of(&g, GE_KILL, &n); ASSERT_EQ(n, 1);
    ASSERT_EQ((int)g.score, 30);
    ASSERT_EQ(g.flawless_words, 0);   /* word had misses */

    /* clean words grow the streak: mult = min(8, 1+flawless) */
    for (int w = 0; w < 9; w++) {
        alien_t *b = game_force_spawn(&g, SP_DRIFTER, "dog", 60);
        ASSERT_TRUE(b != 0);
        type_word(&g, "dog");
        ASSERT_TRUE(!b->active);
    }
    ASSERT_EQ(g.streak_mult, 8);      /* capped */
    ASSERT_EQ(g.flawless_words, 9);

    /* ambiguity: lowest (largest y) matching alien wins the lock */
    game_t g2;
    words_init(2); game_init(&g2, 2); game_start(&g2);
    g2.spawn_left = 0;
    alien_t *hi = game_force_spawn(&g2, SP_DRIFTER, "map", 40);
    alien_t *lo = game_force_spawn(&g2, SP_DRIFTER, "mud", 200);
    hi->y_q8 = 20 << 8; lo->y_q8 = 120 << 8;
    game_char(&g2, 'm');
    ASSERT_EQ(g2.target, (int)(lo - g2.aliens));

    /* shielded: full word twice; shield pop refreshes the word */
    game_t g3;
    words_init(3); game_init(&g3, 3); game_start(&g3);
    g3.spawn_left = 0;
    alien_t *s = game_force_spawn(&g3, SP_SHIELDED, "bat", 80);
    type_word(&g3, "bat");
    ASSERT_TRUE(s->active);           /* survived: shield popped */
    ASSERT_TRUE(!s->shield);
    ASSERT_EQ(s->typed, 0);
    ASSERT_TRUE(strlen(s->word) > 0);
    last_of(&g3, GE_SHIELD_POP, &n); ASSERT_EQ(n, 1);
    type_word(&g3, s->word);
    ASSERT_TRUE(!s->active);
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_game_typing test_game_typing.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_typing PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_typing COMMAND test_game_typing)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: `game_typing` FAILs (stub `game_char` does nothing).

- [ ] **Step 3: Replace the `game_char` stub in `game.c`**

Delete the stub `game_char` and add (final code; `stamp_char`'s ring is consumed by Task 7's WPM):

```c
static void stamp_char(game_t *g)
{
    g->chars_typed++;
    g->char_ticks[g->char_head] = g->ticks;
    g->char_head = (g->char_head + 1) % GAME_CHAR_RING;
}

static void mark_wrong(game_t *g, char expected)
{
    g->chars_wrong++;
    g->word_dirty = true;
    g->flawless_words = 0;
    g->streak_mult = 1;
    g->target = -1;
    if (expected) {
        if (expected == g->fumble_ch) g->fumble_n++;
        else { g->fumble_ch = expected; g->fumble_n = 1; }
    }
    game_push_event(g, GE_WRONG, 0);
}

static void score_kill(game_t *g, alien_t *a)
{
    int len = 0;
    for (const char *p = a->word; *p; p++) if (*p != ' ') len++;
    const alien_def_t *d = alien_def(a->sp);
    g->score += (uint32_t)(10 * len * d->score_pct / 100 * g->streak_mult);
}

static void advance_letter(game_t *g, alien_t *a)
{
    a->typed++;
    stamp_char(g);
    if (a->word[a->typed - 1] == g->fumble_ch) g->fumble_n = 0;  /* mercy clears */
    game_push_event(g, GE_ZAP, g->streak_mult);

    while (a->word[a->typed] == ' ') {            /* boss phrases auto-advance */
        a->typed++;
        int remaining = 0;
        for (const char *p = a->word + a->typed; *p; p++) remaining += (*p == ' ');
        game_push_event(g, GE_BOSS_CHUNK, remaining + 1);
    }

    if (a->word[a->typed] != '\0') return;

    /* word complete */
    if (a->sp == SP_SHIELDED && a->shield) {
        a->shield = false;
        a->typed = 0;
        strncpy(a->word, words_pick(level_bucket(g->level)), GAME_WORD_MAX - 1);
        a->word[GAME_WORD_MAX - 1] = '\0';
        g->word_dirty = false;         /* fresh word, fresh flawless chance */
        game_push_event(g, GE_SHIELD_POP, 0);
        return;
    }
    score_kill(g, a);
    if (a->sp == SP_BOSS) game_push_event(g, GE_BOSS_KILL, 0);
    game_push_event(g, GE_KILL, (int32_t)a->sp);
    int idx = (int)(a - g->aliens);
    alien_deactivate(g, idx);
    if (!g->word_dirty) {
        g->flawless_words++;
        g->streak_mult = 1 + g->flawless_words;
        if (g->streak_mult > 8) g->streak_mult = 8;
    }
    g->word_dirty = false;
}

void game_char(game_t *g, char c)
{
    if (g->game_over) return;
    if (g->target >= 0 && g->aliens[g->target].active) {
        alien_t *a = &g->aliens[g->target];
        if (a->word[a->typed] == c) { advance_letter(g, a); return; }
        mark_wrong(g, a->word[a->typed]);
        return;
    }
    /* no lock: target the lowest (deepest) alien whose next letter matches */
    int best = -1;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        alien_t *a = &g->aliens[i];
        if (!a->active || a->word[a->typed] != c) continue;
        if (best < 0 || a->y_q8 > g->aliens[best].y_q8) best = i;
    }
    if (best < 0) { mark_wrong(g, 0); return; }
    g->target = best;
    /* NOTE: word_dirty is NOT cleared here - a re-lock continues the same
     * word attempt, so an earlier miss still marks it dirty. It clears only
     * on kill and on shield pop. */
    advance_letter(g, &g->aliens[best]);
}
```

Also update `alien_deactivate` usage: `game_char` runs before `advance_letter` may deactivate the target; no change needed — `alien_deactivate` already clears `g->target`.

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (5 tests).

- [ ] **Step 5: Commit**

```bash
git add apps/typing_invaders/game.c tests
git commit -m "feat: typing mechanics - lock-on, scoring, streak multiplier"
```

---

### Task 7: Game completion — boss chunks, WPM, smart bomb, mercy hints

**Files:**
- Modify: `apps/typing_invaders/game.c` (replace remaining stubs)
- Create: `tests/test_game_boss.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 5-6 internals.
- Produces: final `game_shake`, `game_wpm`, `game_avg_wpm`, `game_accuracy_pct`, `game_hint_mercy` used by `render.c`/`main.c`/`hiscore`.

- [ ] **Step 1: Write the failing test `tests/test_game_boss.c`**

```c
#include "test_util.h"
#include "game.h"
#include "words.h"
#include <string.h>

static int count_of(game_t *g, game_event_type t) {
    game_event_t e; int n = 0;
    while (game_next_event(g, &e)) if (e.type == t) n++;
    return n;
}

int main(void) {
    game_t g;
    words_init(5); game_init(&g, 5); game_start(&g);
    g.spawn_left = 0;

    /* boss phrase: spaces auto-advance, chunk events fire, kill at the end */
    alien_t *boss = game_force_spawn(&g, SP_BOSS, "big bad boss", 100);
    for (const char *p = "bigbadboss"; *p; p++) game_char(&g, *p);
    ASSERT_TRUE(!boss->active);
    ASSERT_EQ(count_of(&g, GE_BOSS_CHUNK), 2);   /* after "big", after "bad" */
    /* re-check kill events were emitted */
    /* (ring already drained by count_of; re-run scenario for BOSS_KILL) */
    alien_t *boss2 = game_force_spawn(&g, SP_BOSS, "go now", 100);
    for (const char *p = "gonow"; *p; p++) game_char(&g, *p);
    ASSERT_TRUE(!boss2->active);
    ASSERT_EQ(count_of(&g, GE_BOSS_KILL), 1);

    /* WPM: 25 correct chars in the window = 25/5 words / 15 s => 20 wpm */
    game_t g2;
    words_init(6); game_init(&g2, 6); game_start(&g2);
    g2.spawn_left = 0;
    for (int t = 0; t < GAME_WPM_WINDOW; t++) game_tick(&g2);  /* fill window */
    for (int w = 0; w < 5; w++) {
        alien_t *a = game_force_spawn(&g2, SP_DRIFTER, "cargo", 50);
        for (const char *p = "cargo"; *p; p++) game_char(&g2, *p);
        ASSERT_TRUE(!a->active);
    }
    ASSERT_EQ(game_wpm(&g2), 20);
    ASSERT_EQ(game_accuracy_pct(&g2), 100);
    game_char(&g2, 'q');                     /* one miss */
    ASSERT_EQ(game_accuracy_pct(&g2), 96);   /* 25/26 = 96% */
    ASSERT_TRUE(game_avg_wpm(&g2) > 0);

    /* smart bomb: kills everything for zero points, disarms until level-up */
    game_t g3;
    words_init(7); game_init(&g3, 7); game_start(&g3);
    g3.spawn_left = 0;
    game_force_spawn(&g3, SP_DRIFTER, "cat", 40);
    game_force_spawn(&g3, SP_ZIGZAG, "dog", 200);
    uint32_t score_before = g3.score;
    game_shake(&g3);
    ASSERT_EQ(count_of(&g3, GE_BOMB), 1);
    ASSERT_EQ((int)g3.score, (int)score_before);
    ASSERT_TRUE(!g3.bomb_armed);
    for (int i = 0; i < GAME_MAX_ALIENS; i++) ASSERT_TRUE(!g3.aliens[i].active);
    game_shake(&g3);                          /* disarmed: no event */
    ASSERT_EQ(count_of(&g3, GE_BOMB), 0);

    /* mercy: fumbling the same expected letter twice enables the hint */
    game_t gm;
    words_init(12); game_init(&gm, 12); game_start(&gm);
    gm.spawn_left = 0;
    game_force_spawn(&gm, SP_DRIFTER, "kev", 40);
    game_char(&gm, 'k');            /* lock, expected 'e' */
    game_char(&gm, 'x');            /* fumble 'e' #1 (unlocks) */
    game_char(&gm, 'k');            /* no match -> wrong, expected=0, uncounted */
    game_char(&gm, 'e');            /* re-lock+advance: clears the counter */
    ASSERT_TRUE(!game_hint_mercy(&gm, 'e'));
    game_char(&gm, 'x');            /* expected 'v': fumble 'v' #1 */
    game_char(&gm, 'v');            /* re-lock, typed=2 -> word done, kill */
    /* direct counter check (white box: two consecutive fumbles of one char) */
    gm.fumble_ch = 'q'; gm.fumble_n = 2;
    ASSERT_TRUE(game_hint_mercy(&gm, 'q'));
    ASSERT_TRUE(!game_hint_mercy(&gm, 'r'));
    gm.fumble_n = 1;
    ASSERT_TRUE(!game_hint_mercy(&gm, 'q'));
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_game_boss test_game_boss.c
    ${APPSRC}/game.c ${APPSRC}/words.c ${APPSRC}/aliens.c)
target_include_directories(test_game_boss PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME game_boss COMMAND test_game_boss)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: `game_boss` FAILs (WPM returns 0, `game_shake` does nothing).

- [ ] **Step 3: Replace the remaining stubs in `game.c`**

Delete the stub `game_shake`, `game_wpm`, `game_avg_wpm`, `game_accuracy_pct`, `game_hint_mercy` and add:

```c
void game_shake(game_t *g)
{
    if (g->game_over || !g->bomb_armed) return;
    int n = 0;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        if (!g->aliens[i].active) continue;
        alien_deactivate(g, i);
        n++;
    }
    if (!n) return;                    /* nothing on screen: keep the bomb */
    g->bomb_armed = false;
    game_push_event(g, GE_BOMB, n);
}

int game_wpm(const game_t *g)
{
    uint32_t window = g->ticks < GAME_WPM_WINDOW ? g->ticks : GAME_WPM_WINDOW;
    if (!window) return 0;
    int count = 0;
    uint32_t total = g->chars_typed < GAME_CHAR_RING ? g->chars_typed
                                                     : GAME_CHAR_RING;
    for (uint32_t i = 0; i < total; i++) {
        uint32_t stamp = g->char_ticks[(g->char_head + GAME_CHAR_RING - 1 - i)
                                       % GAME_CHAR_RING];
        if (g->ticks - stamp <= window) count++;
    }
    return (int)((uint32_t)count * GAME_TICKS_PER_SEC * 60u / (window * 5u));
}

int game_avg_wpm(const game_t *g)
{
    if (!g->ticks) return 0;
    return (int)(g->chars_typed * GAME_TICKS_PER_SEC * 60u / (g->ticks * 5u));
}

int game_accuracy_pct(const game_t *g)
{
    uint32_t total = g->chars_typed + g->chars_wrong;
    if (!total) return 100;
    return (int)(g->chars_typed * 100u / total);
}

bool game_hint_mercy(const game_t *g, char c)
{
    return g->fumble_n >= 2 && g->fumble_ch == c;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (6 tests).

- [ ] **Step 5: Commit**

```bash
git add apps/typing_invaders/game.c tests
git commit -m "feat: boss phrases, wpm stats, smart bomb, mercy hints"
```

---

### Task 8: Chiptune synth (pure)

**Files:**
- Create: `apps/typing_invaders/sfx.h`, `apps/typing_invaders/sfx.c`
- Create: `tests/test_sfx.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (pure; no float printf, integers only).
- Produces: `sfx_init(sfx_t*)`, `sfx_trigger(sfx_t*, sfx_id, int arg)`, `sfx_render(sfx_t*, int16_t *out, int n)` — mono 16009 Hz samples. `sfx_ring.c` (Task 13) pulls from `sfx_render`; `main.c` triggers by game event.

- [ ] **Step 1: Write the failing test `tests/test_sfx.c`**

```c
#include "test_util.h"
#include "sfx.h"
#include <string.h>

static int nonzero(const int16_t *b, int n) {
    for (int i = 0; i < n; i++) if (b[i]) return 1;
    return 0;
}

int main(void) {
    sfx_t s;
    int16_t buf[2048];

    /* silence when idle */
    sfx_init(&s);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));

    /* zap: sound now, silence after its ~60 ms run out */
    sfx_trigger(&s, SFX_ZAP, 1);
    sfx_render(&s, buf, 1024);            /* 64 ms */
    ASSERT_TRUE(nonzero(buf, 1024));
    sfx_render(&s, buf, 1024);
    ASSERT_TRUE(!nonzero(buf, 1024));

    /* deterministic: same trigger renders identical samples */
    int16_t buf2[512];
    sfx_init(&s); sfx_trigger(&s, SFX_ZAP, 4);
    sfx_render(&s, buf, 512);
    sfx_init(&s); sfx_trigger(&s, SFX_ZAP, 4);
    sfx_render(&s, buf2, 512);
    ASSERT_TRUE(memcmp(buf, buf2, sizeof buf2) == 0);

    /* all voices at once stay in int16 range (clamped, no wrap) */
    sfx_init(&s);
    sfx_trigger(&s, SFX_ZAP, 8);
    sfx_trigger(&s, SFX_BOSS_BOOM, 0);
    sfx_trigger(&s, SFX_LEVELUP, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(nonzero(buf, 2048));      /* something audible */

    /* level-up jingle spans several steps: still audible at 300 ms */
    sfx_init(&s); sfx_trigger(&s, SFX_LEVELUP, 0);
    sfx_render(&s, buf, 2048);            /* skip 128 ms */
    sfx_render(&s, buf, 2048);            /* 128..256 ms */
    sfx_render(&s, buf, 1024);            /* 256..320 ms */
    ASSERT_TRUE(nonzero(buf, 1024));

    /* title loop keeps playing far beyond one pass; stop silences it */
    sfx_init(&s); sfx_trigger(&s, SFX_TITLE_LOOP, 0);
    for (int i = 0; i < 30; i++) sfx_render(&s, buf, 2048);   /* ~3.8 s */
    ASSERT_TRUE(nonzero(buf, 2048));
    sfx_trigger(&s, SFX_LOOP_STOP, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));

    /* noise voice (kill) is audible and decays out */
    sfx_init(&s); sfx_trigger(&s, SFX_KILL, 0);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(nonzero(buf, 2048));
    sfx_render(&s, buf, 2048);
    sfx_render(&s, buf, 2048);
    ASSERT_TRUE(!nonzero(buf, 2048));
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_sfx test_sfx.c ${APPSRC}/sfx.c)
target_include_directories(test_sfx PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME sfx COMMAND test_sfx)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `sfx.h` missing.

- [ ] **Step 3: Write `apps/typing_invaders/sfx.h` and `sfx.c`**

```c
// sfx.h - pure 2-oscillator chiptune synth at the codec rate. No hardware,
// no floats. Voices: v0 = player squares (zap/wrong), v1 = noise (booms),
// v2 = sequences (jingles + title loop).
#ifndef SFX_H
#define SFX_H
#include <stdint.h>

#define SFX_RATE 16009

typedef enum {
    SFX_ZAP = 0,      /* arg = streak multiplier 1..8 (pitch) */
    SFX_KILL,
    SFX_BOSS_BOOM,
    SFX_WRONG,
    SFX_LEVELUP,
    SFX_GAMEOVER,
    SFX_BOMB,
    SFX_TITLE_LOOP,
    SFX_LOOP_STOP,
} sfx_id;

#define SFX_VOICES 3

typedef struct {
    uint8_t  kind;            /* 0 off, 1 square, 2 noise */
    uint32_t phase, inc;
    int32_t  amp, decay;      /* linear envelope, amp units per sample */
    uint32_t left;            /* samples left in current step */
    const uint16_t *seq;      /* pairs (freq_hz, ms), terminated by 0,0 */
    int      seq_i;
    uint8_t  looping;
    uint16_t lfsr;
} sfx_voice_t;

typedef struct { sfx_voice_t v[SFX_VOICES]; } sfx_t;

void sfx_init(sfx_t *s);
void sfx_trigger(sfx_t *s, sfx_id id, int arg);
void sfx_render(sfx_t *s, int16_t *out, int n);

#endif
```

```c
#include "sfx.h"
#include <string.h>

static uint32_t hz_inc(unsigned hz)
{
    return (uint32_t)(((uint64_t)hz << 32) / SFX_RATE);
}
static uint32_t ms_samples(unsigned ms) { return (uint32_t)ms * SFX_RATE / 1000; }

static void voice_square(sfx_voice_t *v, unsigned hz, unsigned ms, int amp)
{
    v->kind = 1;
    v->inc = hz_inc(hz);
    v->phase = 0;
    v->left = ms_samples(ms);
    v->amp = amp;
    v->decay = v->left ? amp / (int32_t)v->left : amp;
    v->seq = 0; v->looping = 0;
}
static void voice_noise(sfx_voice_t *v, unsigned ms, int amp)
{
    v->kind = 2;
    v->lfsr = 0xACE1;
    v->left = ms_samples(ms);
    v->amp = amp;
    v->decay = v->left ? amp / (int32_t)v->left : amp;
    v->seq = 0; v->looping = 0;
}
static void seq_step(sfx_voice_t *v)
{
    unsigned hz = v->seq[v->seq_i * 2], ms = v->seq[v->seq_i * 2 + 1];
    if (!hz && !ms) {
        if (!v->looping) { v->kind = 0; return; }
        v->seq_i = 0;
        hz = v->seq[0]; ms = v->seq[1];
    }
    v->inc = hz_inc(hz);
    v->left = ms_samples(ms);
    v->amp = 7000;
    v->decay = 0;                       /* sequences hold level per step */
    v->seq_i++;
}
static void voice_seq(sfx_voice_t *v, const uint16_t *seq, int looping)
{
    v->kind = 1;
    v->phase = 0;
    v->seq = seq; v->seq_i = 0; v->looping = (uint8_t)looping;
    seq_step(v);
}

static const uint16_t k_levelup[]  = { 523,80, 659,80, 784,80, 1047,200, 0,0 };
static const uint16_t k_gameover[] = { 392,150, 330,150, 262,320, 0,0 };
static const uint16_t k_title[]    = { 131,180, 0,60, 131,90, 0,30,
                                       196,180, 0,60, 165,90, 0,30, 0,0 };

void sfx_init(sfx_t *s) { memset(s, 0, sizeof *s); }

void sfx_trigger(sfx_t *s, sfx_id id, int arg)
{
    switch (id) {
    case SFX_ZAP:       voice_square(&s->v[0], 500 + 150 * (unsigned)arg, 60, 8000); break;
    case SFX_WRONG:     voice_square(&s->v[0], 130, 120, 8000); break;
    case SFX_KILL:      voice_noise(&s->v[1], 150, 9000); break;
    case SFX_BOSS_BOOM: voice_noise(&s->v[1], 420, 12000); break;
    case SFX_BOMB:      voice_noise(&s->v[1], 500, 12000); break;
    case SFX_LEVELUP:   voice_seq(&s->v[2], k_levelup, 0); break;
    case SFX_GAMEOVER:  voice_seq(&s->v[2], k_gameover, 0); break;
    case SFX_TITLE_LOOP:voice_seq(&s->v[2], k_title, 1); break;
    case SFX_LOOP_STOP: s->v[2].kind = 0; break;
    }
}

void sfx_render(sfx_t *s, int16_t *out, int n)
{
    for (int i = 0; i < n; i++) {
        int32_t mix = 0;
        for (int vi = 0; vi < SFX_VOICES; vi++) {
            sfx_voice_t *v = &s->v[vi];
            if (!v->kind) continue;
            if (v->kind == 1) {                    /* square */
                if (v->inc) {                      /* freq 0 = rest step */
                    mix += (v->phase & 0x80000000u) ? v->amp : -v->amp;
                    v->phase += v->inc;
                }
            } else {                               /* noise */
                unsigned bit = ((v->lfsr >> 0) ^ (v->lfsr >> 2) ^
                                (v->lfsr >> 3) ^ (v->lfsr >> 5)) & 1u;
                v->lfsr = (uint16_t)((v->lfsr >> 1) | (bit << 15));
                mix += (v->lfsr & 1) ? v->amp : -v->amp;
            }
            if (v->decay) { v->amp -= v->decay; if (v->amp < 0) v->amp = 0; }
            if (v->left && --v->left == 0) {
                if (v->seq) seq_step(v);
                else v->kind = 0;
            }
        }
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[i] = (int16_t)mix;
    }
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (7 tests).

- [ ] **Step 5: Add `sfx.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c)
```

```bash
git add apps/typing_invaders/sfx.h apps/typing_invaders/sfx.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: pure chiptune synth - squares, noise, jingle sequencer"
```

---

### Task 9: High-score table codec (pure)

**Files:**
- Create: `apps/typing_invaders/hiscore.h`, `apps/typing_invaders/hiscore.c`
- Create: `tests/test_hiscore.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces: `hs_table_t` (top-10 + `best_wpm`), `hs_clear`, `hs_rank`, `hs_insert`, `hs_note_wpm`, `hs_encode`/`hs_decode` over a fixed `HS_BLOB_SIZE`-byte blob (magic+version+CRC32). `hiscore_flash.c` (Task 13) wraps these; `render.c`/`main.c` read the table.

- [ ] **Step 1: Write the failing test `tests/test_hiscore.c`**

```c
#include "test_util.h"
#include "hiscore.h"
#include <string.h>

int main(void) {
    hs_table_t t;
    hs_clear(&t);
    ASSERT_EQ(t.count, 0); ASSERT_EQ(t.best_wpm, 0);

    /* empty table: every score qualifies at rank 0 */
    ASSERT_EQ(hs_rank(&t, 10), 0);

    hs_insert(&t, "AAA", 100, 12);
    hs_insert(&t, "BBB", 300, 20);
    hs_insert(&t, "CCC", 200, 15);
    ASSERT_EQ(t.count, 3);
    ASSERT_TRUE(strcmp(t.e[0].initials, "BBB") == 0);   /* sorted desc */
    ASSERT_TRUE(strcmp(t.e[1].initials, "CCC") == 0);
    ASSERT_TRUE(strcmp(t.e[2].initials, "AAA") == 0);
    ASSERT_EQ(hs_rank(&t, 250), 1);
    ASSERT_EQ(hs_rank(&t, 50), 3);

    /* fill to 10; an 11th low score no longer qualifies */
    for (int i = 0; i < 7; i++) hs_insert(&t, "DDD", 400 + i, 10);
    ASSERT_EQ(t.count, 10);
    ASSERT_EQ(hs_rank(&t, 1), -1);
    ASSERT_TRUE(hs_rank(&t, 500) >= 0);
    uint32_t lowest_before = t.e[9].score;
    hs_insert(&t, "EEE", 999, 30);
    ASSERT_EQ(t.count, 10);                              /* stayed capped */
    ASSERT_TRUE(t.e[9].score >= lowest_before);          /* lowest dropped */
    ASSERT_TRUE(strcmp(t.e[0].initials, "EEE") == 0);

    hs_note_wpm(&t, 25); ASSERT_EQ(t.best_wpm, 25);
    hs_note_wpm(&t, 19); ASSERT_EQ(t.best_wpm, 25);      /* keeps max */

    /* round-trip */
    uint8_t blob[HS_BLOB_SIZE];
    hs_encode(&t, blob);
    hs_table_t u;
    ASSERT_TRUE(hs_decode(&u, blob));
    ASSERT_EQ(u.count, t.count);
    ASSERT_EQ(u.best_wpm, t.best_wpm);
    ASSERT_TRUE(memcmp(u.e, t.e, sizeof u.e) == 0);

    /* corruption is rejected */
    blob[10] ^= 0xFF;
    ASSERT_TRUE(!hs_decode(&u, blob));
    /* erased-flash blob (all 0xFF) is rejected */
    memset(blob, 0xFF, sizeof blob);
    ASSERT_TRUE(!hs_decode(&u, blob));
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_hiscore test_hiscore.c ${APPSRC}/hiscore.c)
target_include_directories(test_hiscore PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME hiscore COMMAND test_hiscore)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `hiscore.h` missing.

- [ ] **Step 3: Write `apps/typing_invaders/hiscore.h` and `hiscore.c`**

```c
// hiscore.h - top-10 score table + all-time best WPM, with a fixed-size
// checksummed blob codec for flash persistence. Pure, host-testable.
#ifndef HISCORE_H
#define HISCORE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define HS_MAX 10
#define HS_BLOB_SIZE 128

typedef struct {
    char     initials[4];      /* 3 chars + NUL */
    uint32_t score;
    uint16_t wpm;
} hs_entry_t;

typedef struct {
    hs_entry_t e[HS_MAX];
    int        count;
    uint16_t   best_wpm;
} hs_table_t;

void   hs_clear(hs_table_t *t);
int    hs_rank(const hs_table_t *t, uint32_t score);  /* insert idx, -1 = no */
void   hs_insert(hs_table_t *t, const char *initials, uint32_t score, uint16_t wpm);
void   hs_note_wpm(hs_table_t *t, uint16_t wpm);
void   hs_encode(const hs_table_t *t, uint8_t blob[HS_BLOB_SIZE]);
bool   hs_decode(hs_table_t *t, const uint8_t blob[HS_BLOB_SIZE]);

#endif
```

```c
#include "hiscore.h"
#include <string.h>

#define HS_MAGIC 0x54595031u   /* "TYP1" */

void hs_clear(hs_table_t *t) { memset(t, 0, sizeof *t); }

int hs_rank(const hs_table_t *t, uint32_t score)
{
    for (int i = 0; i < t->count; i++)
        if (score > t->e[i].score) return i;
    return t->count < HS_MAX ? t->count : -1;
}

void hs_insert(hs_table_t *t, const char *initials, uint32_t score, uint16_t wpm)
{
    int at = hs_rank(t, score);
    if (at < 0) return;
    int last = t->count < HS_MAX ? t->count : HS_MAX - 1;
    for (int i = last; i > at; i--) t->e[i] = t->e[i - 1];
    memset(&t->e[at], 0, sizeof t->e[at]);
    strncpy(t->e[at].initials, initials, 3);
    t->e[at].score = score;
    t->e[at].wpm = wpm;
    if (t->count < HS_MAX) t->count++;
}

void hs_note_wpm(hs_table_t *t, uint16_t wpm)
{
    if (wpm > t->best_wpm) t->best_wpm = wpm;
}

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int b = 0; b < 8; b++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* layout: magic u32 | count u8 | best_wpm u16 | pad u8 |
 *         10 x (initials 4B | score u32 | wpm u16) = 100B | crc u32
 * total 8 + 100 + 4 = 112, zero-padded to HS_BLOB_SIZE. */
void hs_encode(const hs_table_t *t, uint8_t blob[HS_BLOB_SIZE])
{
    memset(blob, 0, HS_BLOB_SIZE);
    put32(blob, HS_MAGIC);
    blob[4] = (uint8_t)t->count;
    blob[5] = (uint8_t)t->best_wpm;
    blob[6] = (uint8_t)(t->best_wpm >> 8);
    uint8_t *p = blob + 8;
    for (int i = 0; i < HS_MAX; i++, p += 10) {
        memcpy(p, t->e[i].initials, 4);
        put32(p + 4, t->e[i].score);
        p[8] = (uint8_t)t->e[i].wpm;
        p[9] = (uint8_t)(t->e[i].wpm >> 8);
    }
    put32(blob + 108, crc32(blob, 108));
}

bool hs_decode(hs_table_t *t, const uint8_t blob[HS_BLOB_SIZE])
{
    if (get32(blob) != HS_MAGIC) return false;
    if (get32(blob + 108) != crc32(blob, 108)) return false;
    hs_clear(t);
    t->count = blob[4];
    if (t->count > HS_MAX) return false;
    t->best_wpm = (uint16_t)(blob[5] | (blob[6] << 8));
    const uint8_t *p = blob + 8;
    for (int i = 0; i < HS_MAX; i++, p += 10) {
        memcpy(t->e[i].initials, p, 4);
        t->e[i].initials[3] = '\0';
        t->e[i].score = get32(p + 4);
        t->e[i].wpm = (uint16_t)(p[8] | (p[9] << 8));
    }
    return true;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (8 tests).

- [ ] **Step 5: Add `hiscore.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c hiscore.c)
```

```bash
git add apps/typing_invaders/hiscore.h apps/typing_invaders/hiscore.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: high-score table with checksummed flash blob codec"
```

---

### Task 10: Shake detector (pure)

**Files:**
- Create: `apps/typing_invaders/shake.h`, `apps/typing_invaders/shake.c`
- Create: `tests/test_shake.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (accel values in g arrive from `bmi323_read` in `main.c`).
- Produces: `shake_init(shake_t*)`, `bool shake_feed(shake_t*, float ax, float ay, float az, uint32_t now_ms)` — true once per shake (>2.5 g magnitude), 800 ms lockout.

- [ ] **Step 1: Write the failing test `tests/test_shake.c`**

```c
#include "test_util.h"
#include "shake.h"

int main(void) {
    shake_t s;
    shake_init(&s);
    /* at rest (1 g down): never fires */
    for (uint32_t t = 0; t < 2000; t += 20)
        ASSERT_TRUE(!shake_feed(&s, 0.0f, 0.0f, 1.0f, t));
    /* hard shake fires once, then locks out */
    ASSERT_TRUE(shake_feed(&s, 2.0f, 2.0f, 0.5f, 2000));
    ASSERT_TRUE(!shake_feed(&s, 2.0f, 2.0f, 0.5f, 2020));
    ASSERT_TRUE(!shake_feed(&s, 3.0f, 0.0f, 0.0f, 2600));
    /* after the 800 ms lockout it can fire again */
    ASSERT_TRUE(shake_feed(&s, 3.0f, 0.0f, 0.0f, 2801));
    /* moderate motion (1.5 g) is below threshold */
    shake_init(&s);
    ASSERT_TRUE(!shake_feed(&s, 1.5f, 0.0f, 0.0f, 100));
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_shake test_shake.c ${APPSRC}/shake.c)
target_include_directories(test_shake PRIVATE ${APPSRC} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME shake COMMAND test_shake)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `shake.h` missing.

- [ ] **Step 3: Write `apps/typing_invaders/shake.h` and `shake.c`**

```c
// shake.h - debounced shake detector over accel samples (units: g).
// Fires once when |a| > 2.5 g, then locks out for 800 ms.
#ifndef SHAKE_H
#define SHAKE_H
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t lockout_until; } shake_t;

void shake_init(shake_t *s);
bool shake_feed(shake_t *s, float ax, float ay, float az, uint32_t now_ms);

#endif
```

```c
#include "shake.h"

#define SHAKE_G2      6.25f    /* (2.5 g)^2 */
#define SHAKE_LOCK_MS 800u

void shake_init(shake_t *s) { s->lockout_until = 0; }

bool shake_feed(shake_t *s, float ax, float ay, float az, uint32_t now_ms)
{
    float m2 = ax * ax + ay * ay + az * az;
    if (now_ms < s->lockout_until) return false;
    if (m2 < SHAKE_G2) return false;
    s->lockout_until = now_ms + SHAKE_LOCK_MS;
    return true;
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (9 tests).

- [ ] **Step 5: Add `shake.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c hiscore.c shake.c)
```

```bash
git add apps/typing_invaders/shake.h apps/typing_invaders/shake.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: debounced imu shake detector"
```

---

### Task 11: Framebuffer helpers (pure)

**Files:**
- Create: `apps/typing_invaders/fb.h`, `apps/typing_invaders/fb.c`
- Create: `tests/test_fb.c`
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: `display/st7796.h` (only `ST7796_W/H` defines — header is pure), `display/font5x7.h` + `font5x7.c` (pure glyph table).
- Produces: `fb_set(uint16_t*)`, `fb_get()`, `fb_clear`, `fb_fill_rect` (clipped), `fb_draw_text` (opaque bg), `fb_draw_text_t` (transparent bg), `uint16_t fb_rgb(uint8_t r,g,b)` (big-endian RGB565). Used by `render.c` and `main.c` (double buffering).

- [ ] **Step 1: Write the failing test `tests/test_fb.c`**

```c
#include "test_util.h"
#include "fb.h"
#include "display/st7796.h"
#include <stdlib.h>
#include <string.h>

int main(void) {
    uint16_t *buf = calloc(ST7796_W * ST7796_H, 2);
    uint16_t *guard = buf;   /* calloc'd exact size; OOB writes would crash */
    fb_set(buf);
    ASSERT_TRUE(fb_get() == buf);

    uint16_t red = fb_rgb(255, 0, 0);
    fb_clear(red);
    ASSERT_EQ(buf[0], red);
    ASSERT_EQ(buf[ST7796_W * ST7796_H - 1], red);

    /* clipped fill: negative origin and past-edge extent */
    uint16_t blue = fb_rgb(0, 0, 255);
    fb_fill_rect(-10, -10, 20, 20, blue);
    ASSERT_EQ(buf[0], blue);                              /* 10x10 landed */
    ASSERT_EQ(buf[10 * ST7796_W + 10], red);              /* outside kept */
    fb_fill_rect(ST7796_W - 5, ST7796_H - 5, 50, 50, blue);
    ASSERT_EQ(buf[ST7796_H * ST7796_W - 1], blue);

    /* text draws pixels; transparent variant leaves bg alone */
    fb_clear(0);
    uint16_t white = fb_rgb(255, 255, 255);
    fb_draw_text(0, 0, 2, white, red, "A");
    int fg = 0, bg = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 12; x++) {
            if (buf[y * ST7796_W + x] == white) fg++;
            if (buf[y * ST7796_W + x] == red) bg++;
        }
    ASSERT_TRUE(fg > 10); ASSERT_TRUE(bg > 10);
    fb_clear(red);
    fb_draw_text_t(0, 0, 2, white, "A");
    int untouched = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 12; x++)
            if (buf[y * ST7796_W + x] == red) untouched++;
    ASSERT_TRUE(untouched > 10);                          /* bg preserved */

    /* out-of-bounds text: no crash, buffer intact */
    fb_draw_text(ST7796_W - 3, ST7796_H - 3, 4, white, red, "XYZ");
    (void)guard;
    free(buf);
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_fb test_fb.c ${APPSRC}/fb.c ${BSPDIR}/display/font5x7.c)
target_include_directories(test_fb PRIVATE ${APPSRC} ${BSPDIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME fb COMMAND test_fb)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `fb.h` missing.

- [ ] **Step 3: Write `apps/typing_invaders/fb.h` and `fb.c`**

(The draw core is the proven `hello_keyboard` fb code, parameterized on a settable buffer and extended with a transparent-text variant.)

```c
// fb.h - software framebuffer helpers over a caller-owned 480x320 wire-order
// RGB565 buffer (PSRAM on target, malloc in host tests). Pure: no hardware.
#ifndef FB_H
#define FB_H
#include <stdint.h>

void      fb_set(uint16_t *buf);
uint16_t *fb_get(void);
void      fb_clear(uint16_t color_be);
void      fb_fill_rect(int x, int y, int w, int h, uint16_t color_be);
void      fb_draw_text(int x, int y, int scale, uint16_t fg_be, uint16_t bg_be,
                       const char *s);
void      fb_draw_text_t(int x, int y, int scale, uint16_t fg_be, const char *s);
uint16_t  fb_rgb(uint8_t r, uint8_t g, uint8_t b);   /* big-endian RGB565 */

#endif
```

```c
#include "fb.h"
#include "display/st7796.h"
#include "display/font5x7.h"

static uint16_t *s_fb;

void      fb_set(uint16_t *buf) { s_fb = buf; }
uint16_t *fb_get(void)          { return s_fb; }

uint16_t fb_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

void fb_clear(uint16_t color_be)
{
    for (int i = 0; i < ST7796_W * ST7796_H; i++) s_fb[i] = color_be;
}

void fb_fill_rect(int x, int y, int w, int h, uint16_t color_be)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > ST7796_W) w = ST7796_W - x;
    if (y + h > ST7796_H) h = ST7796_H - y;
    for (int yy = y; yy < y + h; yy++) {
        uint16_t *row = s_fb + (long)yy * ST7796_W + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color_be;
    }
}

static void draw_text_impl(int x, int y, int scale, uint16_t fg_be,
                           uint16_t bg_be, int transparent, const char *s)
{
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    const int w = 6 * scale, h = 8 * scale;
    for (; *s; s++, x += w) {
        if (x + w > ST7796_W || y + h > ST7796_H || x < 0 || y < 0) break;
        char c = *s;
        const uint8_t *cols = (c >= FONT5X7_FIRST && c <= FONT5X7_LAST)
                                  ? font5x7[c - FONT5X7_FIRST]
                                  : font5x7[0];
        for (int gy = 0; gy < h; gy++) {
            int grow = gy / scale;
            uint16_t *row = s_fb + (long)(y + gy) * ST7796_W + x;
            for (int gx = 0; gx < w; gx++) {
                int col = gx / scale;
                int on = col < 5 && grow < 7 && ((cols[col] >> grow) & 1);
                if (on) row[gx] = fg_be;
                else if (!transparent) row[gx] = bg_be;
            }
        }
    }
}

void fb_draw_text(int x, int y, int scale, uint16_t fg_be, uint16_t bg_be,
                  const char *s)
{
    draw_text_impl(x, y, scale, fg_be, bg_be, 0, s);
}

void fb_draw_text_t(int x, int y, int scale, uint16_t fg_be, const char *s)
{
    draw_text_impl(x, y, scale, fg_be, 0, 1, s);
}
```

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (10 tests).

- [ ] **Step 5: Add `fb.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c hiscore.c shake.c fb.c)
```

```bash
git add apps/typing_invaders/fb.h apps/typing_invaders/fb.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: framebuffer helpers with transparent text"
```

---

### Task 12: Renderer — playfield, HUD, screens

**Files:**
- Create: `apps/typing_invaders/render.h`, `apps/typing_invaders/render.c`
- Create: `tests/test_render.c` (smoke: renders into a host buffer without crashing)
- Modify: `tests/CMakeLists.txt`, `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: `fb_*` (Task 11), `game_t`/queries (Tasks 5-7), `alien_def` (Task 4), `hints_chord` (Task 2), `hs_table_t` (Task 9).
- Produces (called by `main.c`):
  - `void render_game(const game_t *g, const render_fx_t *fx)` — `render_fx_t { int laser_x, laser_y, laser_ttl, flash_ttl; }`
  - `void render_title(const hs_table_t *hs, uint32_t frame)`
  - `void render_level_intro(int level)`
  - `void render_game_over(const game_t *g)`
  - `void render_initials(const char initials[4], int pos, uint32_t score)`
  - `void render_hiscores(const hs_table_t *hs)`

- [ ] **Step 1: Write the failing smoke test `tests/test_render.c`**

```c
#include "test_util.h"
#include "render.h"
#include "game.h"
#include "words.h"
#include "hiscore.h"
#include "fb.h"
#include "display/st7796.h"
#include <stdlib.h>

static int nonzero_px(const uint16_t *b) {
    int n = 0;
    for (int i = 0; i < ST7796_W * ST7796_H; i++) n += (b[i] != 0);
    return n;
}

int main(void) {
    uint16_t *buf = calloc(ST7796_W * ST7796_H, 2);
    fb_set(buf);

    game_t g;
    words_init(3); game_init(&g, 3); game_start(&g);
    game_force_spawn(&g, SP_DRIFTER, "cat", 50);
    game_force_spawn(&g, SP_ZIGZAG, "dog", 200)->y_q8 = 100 << 8;
    game_force_spawn(&g, SP_BOSS, "big bad boss", 120)->y_q8 = 40 << 8;
    game_char(&g, 'c');                       /* lock target for bracket path */

    render_fx_t fx = { .laser_x = 60, .laser_y = 30, .laser_ttl = 2, .flash_ttl = 0 };
    render_game(&g, &fx);
    ASSERT_TRUE(nonzero_px(buf) > 2000);      /* aliens + HUD drew something */

    /* every screen renders without crashing */
    hs_table_t hs; hs_clear(&hs);
    hs_insert(&hs, "AAA", 1000, 22);
    render_title(&hs, 0);       ASSERT_TRUE(nonzero_px(buf) > 500);
    render_title(&hs, 99);
    render_level_intro(5);      ASSERT_TRUE(nonzero_px(buf) > 500);
    render_game_over(&g);       ASSERT_TRUE(nonzero_px(buf) > 500);
    render_initials("AB", 2, 12345);
    render_hiscores(&hs);       ASSERT_TRUE(nonzero_px(buf) > 500);

    /* flash overlay path */
    fx.flash_ttl = 2;
    render_game(&g, &fx);
    free(buf);
    TEST_RETURN();
}
```

Add to `tests/CMakeLists.txt`:

```cmake
add_executable(test_render test_render.c
    ${APPSRC}/render.c ${APPSRC}/fb.c ${APPSRC}/game.c ${APPSRC}/words.c
    ${APPSRC}/aliens.c ${APPSRC}/hints.c ${APPSRC}/hiscore.c
    ${BSPDIR}/display/font5x7.c)
target_include_directories(test_render PRIVATE ${APPSRC} ${BSPDIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME render COMMAND test_render)
```

- [ ] **Step 2: Run tests to verify failure**

Run: `python tools/fw.py test`
Expected: FAIL — `render.h` missing.

- [ ] **Step 3: Write `apps/typing_invaders/render.h` and `render.c`**

```c
// render.h - draws all screens into the fb module's current buffer.
// Pure w.r.t. hardware (fb is a plain memory buffer); main.c flushes.
#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
#include "game.h"
#include "hiscore.h"

typedef struct {
    int laser_x, laser_y;   /* beam top point (alien center-bottom), px */
    int laser_ttl;          /* ticks left to draw the beam */
    int flash_ttl;          /* smart-bomb white flash */
} render_fx_t;

void render_game(const game_t *g, const render_fx_t *fx);
void render_title(const hs_table_t *hs, uint32_t frame);
void render_level_intro(int level);
void render_game_over(const game_t *g);
void render_initials(const char initials[4], int pos, uint32_t score);
void render_hiscores(const hs_table_t *hs);

#endif
```

```c
#include "render.h"
#include "fb.h"
#include "hints.h"
#include "aliens.h"
#include "display/st7796.h"
#include <string.h>
#include <stdio.h>

#define CELL_W 12
#define CELL_H 16
#define HUD_Y  296

/* fw2kb keycap colors (from the FW2 firmware soft menu) for hint dots */
static const uint8_t k_btn_rgb[5][3] = {
    { 214, 210, 214 },   /* gray   */
    { 255, 227,  49 },   /* yellow */
    {  60, 160,  30 },   /* green (brightened from #104100 for visibility) */
    {  40,  90, 255 },   /* blue (brightened from #001cc5) */
    { 220,  40,  90 },   /* red (brightened from #84003a) */
};

static uint16_t btn_col(int i)
{
    return fb_rgb(k_btn_rgb[i][0], k_btn_rgb[i][1], k_btn_rgb[i][2]);
}

static void draw_num(int x, int y, int scale, uint16_t fg, uint16_t bg,
                     const char *label, uint32_t v)
{
    char s[24];
    snprintf(s, sizeof s, "%s%u", label, (unsigned)v);
    fb_draw_text(x, y, scale, fg, bg, s);
}

static void draw_alien(const game_t *g, const alien_t *a, int idx)
{
    const alien_def_t *d = alien_def(a->sp);
    int x = a->x_q8 >> 8, y = a->y_q8 >> 8;
    uint16_t col = fb_rgb(d->r, d->g, d->b);
    int frame = (int)((g->ticks >> 3) & 1);

    for (int r = 0; r < d->rows; r++)
        fb_draw_text_t(x, y + r * CELL_H, 2, col, d->art[frame][r]);

    /* targeting bracket */
    if (g->target == idx) {
        uint16_t w = fb_rgb(255, 255, 255);
        fb_fill_rect(x - 4, y, 2, d->rows * CELL_H, w);
        fb_fill_rect(x + d->cols * CELL_W + 2, y, 2, d->rows * CELL_H, w);
    }

    /* word banner: typed dim, next bright, rest mid; hint dots per mode */
    int by = y + d->rows * CELL_H + 2;
    int wlen = (int)strlen(a->word);
    int bx = x + (d->cols * CELL_W - wlen * CELL_W) / 2;
    if (bx < 0) bx = 0;
    for (int i = 0; i < wlen; i++) {
        char cs[2] = { a->word[i], 0 };
        uint16_t fg = i < a->typed ? fb_rgb(90, 90, 90)
                    : i == a->typed ? fb_rgb(255, 255, 255)
                    : fb_rgb(190, 190, 190);
        fb_draw_text_t(bx + i * CELL_W, by, 2, fg, cs);
        int show = game_hint_all(g)
                || (game_hint_next(g) && i == a->typed)
                || game_hint_mercy(g, a->word[i]);
        int b1, b2;
        if (show && i >= a->typed && hints_chord(a->word[i], &b1, &b2)) {
            fb_fill_rect(bx + i * CELL_W + 1, by + CELL_H, 4, 4, btn_col(b1));
            fb_fill_rect(bx + i * CELL_W + 7, by + CELL_H, 4, 4, btn_col(b2));
        }
    }

    /* shielded: draw a shield tick above; boss: health bar */
    if (a->sp == SP_SHIELDED && a->shield)
        fb_draw_text_t(x, y - 10, 1, fb_rgb(255, 200, 0), "=SHIELD=");
    if (a->sp == SP_BOSS) {
        int total = wlen ? wlen : 1;
        int left = total - a->typed;
        int bw = d->cols * CELL_W;
        fb_fill_rect(x, y - 8, bw, 5, fb_rgb(60, 0, 0));
        fb_fill_rect(x, y - 8, bw * left / total, 5, fb_rgb(255, 40, 40));
    }
}

void render_game(const game_t *g, const render_fx_t *fx)
{
    if (fx->flash_ttl > 0) {                   /* smart-bomb whiteout frame */
        fb_clear(fb_rgb(255, 255, 255));
        return;
    }
    fb_clear(0);

    /* defense line + cannon */
    fb_fill_rect(0, GAME_DEFENSE_Y, GAME_FIELD_W, 1, fb_rgb(80, 80, 120));
    fb_draw_text_t(240 - 2 * CELL_W + CELL_W / 2, GAME_DEFENSE_Y + 2, 2,
                   fb_rgb(120, 255, 120), "/_A_\\");

    /* laser beam: alien center-bottom straight down to the defense line */
    if (fx->laser_ttl > 0 && fx->laser_y >= 0)
        fb_fill_rect(fx->laser_x - 1, fx->laser_y, 2,
                     GAME_DEFENSE_Y - fx->laser_y, fb_rgb(180, 255, 180));

    for (int i = 0; i < GAME_MAX_ALIENS; i++)
        if (g->aliens[i].active) draw_alien(g, &g->aliens[i], i);

    /* HUD strip */
    uint16_t hfg = fb_rgb(200, 200, 255);
    draw_num(4, HUD_Y, 2, hfg, 0, "", g->score);
    draw_num(150, HUD_Y, 2, fb_rgb(255, 220, 100), 0, "X", (uint32_t)g->streak_mult);
    draw_num(210, HUD_Y, 2, hfg, 0, "WPM ", (uint32_t)game_wpm(g));
    draw_num(330, HUD_Y, 2, hfg, 0, "LV", (uint32_t)g->level);
    for (int i = 0; i < g->lives; i++)   /* font5x7 covers 0x20-0x7E only */
        fb_draw_text_t(400 + i * 14, HUD_Y, 2, fb_rgb(255, 60, 60), "*");
    if (g->bomb_armed)
        fb_draw_text_t(450, HUD_Y, 2, fb_rgb(255, 255, 100), "@");
}

void render_title(const hs_table_t *hs, uint32_t frame)
{
    fb_clear(fb_rgb(0, 0, 16));
    fb_draw_text(96, 60, 4, fb_rgb(120, 255, 120), fb_rgb(0, 0, 16), "TYPING");
    fb_draw_text(72, 100, 4, fb_rgb(255, 80, 80), fb_rgb(0, 0, 16), "INVADERS");
    if ((frame >> 4) & 1)
        fb_draw_text(108, 180, 2, fb_rgb(255, 255, 255), fb_rgb(0, 0, 16),
                     "PRESS ANY CHORD TO START");
    char line[48];
    if (hs->count > 0)
        snprintf(line, sizeof line, "HI %s %u  BEST %u WPM",
                 hs->e[0].initials, (unsigned)hs->e[0].score,
                 (unsigned)hs->best_wpm);
    else
        snprintf(line, sizeof line, "NO SCORES YET - BE FIRST");
    fb_draw_text(60, 240, 2, fb_rgb(160, 160, 200), fb_rgb(0, 0, 16), line);
}

void render_level_intro(int level)
{
    fb_clear(0);
    char s[24];
    snprintf(s, sizeof s, "WAVE %d", level);
    fb_draw_text(160, 120, 4, fb_rgb(255, 255, 255), 0, s);
    const char *sub = game_is_boss_level(level) ? "!! BOSS INCOMING !!"
                    : level == 4 ? "HINTS FADE - NEXT LETTER ONLY"
                    : level == 7 ? "NO MORE HINTS - YOU KNOW THIS"
                    : "TYPE TO DESTROY";
    fb_draw_text(90, 170, 2, fb_rgb(255, 220, 100), 0, sub);
}

void render_game_over(const game_t *g)
{
    fb_clear(fb_rgb(24, 0, 0));
    fb_draw_text(110, 60, 4, fb_rgb(255, 80, 80), fb_rgb(24, 0, 0), "GAME OVER");
    draw_num(120, 130, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "SCORE ", g->score);
    draw_num(120, 156, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "AVG WPM ", (uint32_t)game_avg_wpm(g));
    draw_num(120, 182, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "ACCURACY ", (uint32_t)game_accuracy_pct(g));
    draw_num(120, 208, 2, fb_rgb(255, 255, 255), fb_rgb(24, 0, 0),
             "WAVE ", (uint32_t)g->level);
}

void render_initials(const char initials[4], int pos, uint32_t score)
{
    fb_clear(fb_rgb(0, 0, 24));
    fb_draw_text(84, 60, 2, fb_rgb(255, 255, 255), fb_rgb(0, 0, 24),
                 "NEW HIGH SCORE - ENTER INITIALS");
    draw_num(180, 90, 2, fb_rgb(255, 220, 100), fb_rgb(0, 0, 24), "", score);
    for (int i = 0; i < 3; i++) {
        int x = 180 + i * 48;
        char cs[2] = { initials[i] ? initials[i] : '_', 0 };
        uint16_t fg = i == pos ? fb_rgb(255, 255, 100) : fb_rgb(255, 255, 255);
        fb_draw_text(x, 140, 4, fg, fb_rgb(0, 0, 24), cs);
        fb_fill_rect(x, 176, 24, 2, fg);
    }
    fb_draw_text(96, 230, 1, fb_rgb(140, 140, 180), fb_rgb(0, 0, 24),
                 "chords type - tap top of screen = backspace");
}

void render_hiscores(const hs_table_t *hs)
{
    fb_clear(fb_rgb(0, 0, 16));
    fb_draw_text(140, 20, 3, fb_rgb(120, 255, 120), fb_rgb(0, 0, 16), "HALL OF FAME");
    for (int i = 0; i < hs->count; i++) {
        char line[40];
        snprintf(line, sizeof line, "%2d %s %8u %3u WPM", i + 1,
                 hs->e[i].initials, (unsigned)hs->e[i].score,
                 (unsigned)hs->e[i].wpm);
        fb_draw_text(90, 60 + i * 20, 2, fb_rgb(220, 220, 220), fb_rgb(0, 0, 16), line);
    }
    draw_num(150, 270, 2, fb_rgb(255, 220, 100), fb_rgb(0, 0, 16),
             "BEST WPM ", hs->best_wpm);
}
```

Note: `snprintf` is fine here — render runs on the CPU, not through RTT; the no-float rule applies to `DIAG` and all values printed are integers.

- [ ] **Step 4: Run tests to verify pass**

Run: `python tools/fw.py test`
Expected: `100% tests passed` (11 tests).

- [ ] **Step 5: Add `render.c` to the app source list and commit**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c hiscore.c shake.c fb.c render.c)
```

```bash
git add apps/typing_invaders/render.h apps/typing_invaders/render.c tests apps/typing_invaders/CMakeLists.txt
git commit -m "feat: renderer - playfield, hint dots, hud, all screens"
```

---

### Task 13: Hardware bindings — audio ring, LEDs, haptic, flash

**Files:**
- Create: `apps/typing_invaders/sfx_ring.h`, `apps/typing_invaders/sfx_ring.c`
- Create: `apps/typing_invaders/ledfx.h`, `apps/typing_invaders/ledfx.c`
- Create: `apps/typing_invaders/haptic.h`, `apps/typing_invaders/haptic.c`
- Create: `apps/typing_invaders/hiscore_flash.h`, `apps/typing_invaders/hiscore_flash.c`
- Modify: `apps/typing_invaders/CMakeLists.txt`

**Interfaces:**
- Consumes: BSP `audio_i2s_duplex_*`, `codec_nau88c10_*`, `ws2812_*`, `led_color.h` (`rgb_t`), pico-sdk `hardware/flash.h`, `hardware/sync.h`, `hardware/gpio.h`, `time_us_64()`; `sfx_render` (Task 8); `hs_encode/decode` (Task 9).
- Produces (used by `main.c`):
  - `sfxring_init(sfx_t*)`, `sfxring_pump(void)`, `sfxring_stop(void)`, `sfxring_resume(void)`
  - `ledfx_init(void)`, `ledfx_set_level(int)`, `ledfx_chord_flash(int btn)`, `ledfx_danger(bool)`, `ledfx_rainbow(void)`, `ledfx_white(void)`, `ledfx_task(void)`
  - `haptic_init(void)`, `haptic_pulse(uint32_t ms)`, `haptic_double(uint32_t on_ms, uint32_t gap_ms)`, `haptic_task(void)`
  - `hsflash_load(hs_table_t*)`, `hsflash_save(const hs_table_t*)` (**contract: call save only with audio stopped and no flush in progress**)

These are target-only (no host tests); verification is the target build compiling clean plus the Task 14 on-device checklist.

- [ ] **Step 1: Write `sfx_ring.h` / `sfx_ring.c`**

```c
// sfx_ring.h - couples the pure sfx synth to the I2S DMA stream loop.
// A 8192-frame ring (SRAM - flash writes stall PSRAM) loops forever via
// zero-CPU DMA; pump() renders ~100 ms ahead of a wall-clock playhead.
#ifndef SFX_RING_H
#define SFX_RING_H
#include "sfx.h"

void sfxring_init(sfx_t *s);     /* codec + i2s up, ring armed */
void sfxring_pump(void);         /* call every main-loop pass */
void sfxring_stop(void);         /* silence + DMA stopped (for flash writes) */
void sfxring_resume(void);
#endif
```

```c
#include "sfx_ring.h"
#include "fw2.h"
#include "pico/stdlib.h"
#include <string.h>

#define RING_FRAMES 8192u        /* one stream chunk = 512 ms */

static uint32_t s_ring[RING_FRAMES];
static uint64_t s_t0;
static uint32_t s_written;
static sfx_t   *s_sfx;

void sfxring_init(sfx_t *s)
{
    s_sfx = s;
    codec_nau88c10_init();
    audio_i2s_duplex_init(16000);
    codec_nau88c10_dac_mute(false);
    memset(s_ring, 0, sizeof s_ring);
    audio_i2s_duplex_play_stream_loop(s_ring, RING_FRAMES);
    s_t0 = time_us_64();
    s_written = 0;
}

void sfxring_pump(void)
{
    if (!s_sfx) return;
    uint64_t pos = (time_us_64() - s_t0) * SFX_RATE / 1000000ull;
    uint32_t target = (uint32_t)pos + SFX_RATE / 10;    /* 100 ms lead */
    while (s_written < target) {
        int16_t chunk[128];
        uint32_t want = target - s_written;
        int n = want > 128 ? 128 : (int)want;
        sfx_render(s_sfx, chunk, n);
        for (int i = 0; i < n; i++) {
            uint16_t u = (uint16_t)chunk[i];
            s_ring[(s_written + (uint32_t)i) % RING_FRAMES] =
                ((uint32_t)u << 16) | u;
        }
        s_written += (uint32_t)n;
    }
}

void sfxring_stop(void)
{
    audio_i2s_duplex_play_stop();
}

void sfxring_resume(void)
{
    memset(s_ring, 0, sizeof s_ring);
    audio_i2s_duplex_play_stream_loop(s_ring, RING_FRAMES);
    s_t0 = time_us_64();
    s_written = 0;
}
```

- [ ] **Step 2: Write `ledfx.h` / `ledfx.c`**

```c
// ledfx.h - ambient effect layer for the 16-pixel WS2812 strip.
#ifndef LEDFX_H
#define LEDFX_H
#include <stdbool.h>

void ledfx_init(void);
void ledfx_set_level(int level);     /* idle glow color follows the level */
void ledfx_chord_flash(int btn);     /* 0..4 = keycap color segment flash */
void ledfx_danger(bool on);          /* alien near defense line: red pulse */
void ledfx_rainbow(void);            /* level-up sweep */
void ledfx_white(void);              /* smart-bomb burst */
void ledfx_task(void);               /* call every main-loop pass */
#endif
```

```c
#include "ledfx.h"
#include "fw2.h"
#include "pico/stdlib.h"

static const rgb_t k_btn[5] = {
    { 120, 118, 120 }, { 255, 227, 49 }, { 40, 160, 20 },
    { 20, 60, 255 }, { 220, 20, 70 } };
static const rgb_t k_level[8] = {
    { 0, 40, 0 }, { 0, 30, 30 }, { 0, 0, 50 }, { 40, 30, 0 },
    { 40, 0, 40 }, { 50, 20, 0 }, { 30, 30, 30 }, { 50, 0, 0 } };

static rgb_t    s_base;
static bool     s_danger;
static int      s_flash_btn = -1;
static uint64_t s_flash_until, s_rainbow_until, s_white_until, s_next_show;

void ledfx_init(void)
{
    ws2812_init(pio1, 0, 21);        /* PIN_LED_DATA (pinmap.md) */
    ws2812_set_brightness(40);
    s_base = k_level[0];
}

void ledfx_set_level(int level) { s_base = k_level[(level - 1) & 7]; }
void ledfx_chord_flash(int btn)
{
    if (btn < 0 || btn > 4) return;
    s_flash_btn = btn;
    s_flash_until = time_us_64() + 150000;
}
void ledfx_danger(bool on) { s_danger = on; }
void ledfx_rainbow(void) { s_rainbow_until = time_us_64() + 600000; }
void ledfx_white(void)   { s_white_until = time_us_64() + 300000; }

static rgb_t wheel(unsigned p)       /* 0..255 -> color wheel */
{
    p &= 255;
    if (p < 85)  return (rgb_t){ (uint8_t)(255 - p * 3), (uint8_t)(p * 3), 0 };
    if (p < 170) { p -= 85; return (rgb_t){ 0, (uint8_t)(255 - p * 3), (uint8_t)(p * 3) }; }
    p -= 170;    return (rgb_t){ (uint8_t)(p * 3), 0, (uint8_t)(255 - p * 3) };
}

void ledfx_task(void)
{
    uint64_t now = time_us_64();
    if (now < s_next_show) return;   /* ~60 Hz cap */
    s_next_show = now + 16000;

    for (unsigned i = 0; i < FW2_LED_COUNT; i++) {
        rgb_t c = s_base;
        if (s_danger) {
            unsigned tri = (unsigned)(now / 4000) & 255;      /* ~1 Hz pulse */
            if (tri > 127) tri = 255 - tri;
            c = (rgb_t){ (uint8_t)(60 + tri), 0, 0 };
        }
        if (now < s_flash_until && s_flash_btn >= 0) {
            unsigned seg = i * 5u / FW2_LED_COUNT;            /* 5 segments */
            if ((int)seg == s_flash_btn) c = k_btn[s_flash_btn];
        }
        if (now < s_rainbow_until)
            c = wheel((unsigned)(i * 16 + now / 3000));
        if (now < s_white_until)
            c = (rgb_t){ 255, 255, 255 };
        ws2812_set_pixel(i, c);
    }
    ws2812_show();
}
```

- [ ] **Step 3: Write `haptic.h` / `haptic.c`**

```c
// haptic.h - vibration motor on GPIO 46 (no BSP driver yet; plain on/off).
#ifndef HAPTIC_H
#define HAPTIC_H
#include <stdint.h>

void haptic_init(void);
void haptic_pulse(uint32_t ms);
void haptic_double(uint32_t on_ms, uint32_t gap_ms);   /* buzz-buzz */
void haptic_task(void);                                /* call every pass */
#endif
```

```c
#include "haptic.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define PIN_HAPTIC 46

static uint64_t s_off_at;          /* current pulse end */
static uint64_t s_next_on_at;      /* queued second pulse start */
static uint32_t s_next_on_ms;

void haptic_init(void)
{
    gpio_init(PIN_HAPTIC);
    gpio_set_dir(PIN_HAPTIC, GPIO_OUT);
    gpio_put(PIN_HAPTIC, 0);
}

void haptic_pulse(uint32_t ms)
{
    gpio_put(PIN_HAPTIC, 1);
    s_off_at = time_us_64() + (uint64_t)ms * 1000;
    s_next_on_at = 0;
}

void haptic_double(uint32_t on_ms, uint32_t gap_ms)
{
    haptic_pulse(on_ms);
    s_next_on_at = s_off_at + (uint64_t)gap_ms * 1000;
    s_next_on_ms = on_ms;
}

void haptic_task(void)
{
    uint64_t now = time_us_64();
    if (s_off_at && now >= s_off_at) {
        gpio_put(PIN_HAPTIC, 0);
        s_off_at = 0;
    }
    if (s_next_on_at && now >= s_next_on_at) {
        uint32_t ms = s_next_on_ms;
        s_next_on_at = 0;
        haptic_pulse(ms);
    }
}
```

- [ ] **Step 4: Write `hiscore_flash.h` / `hiscore_flash.c`**

```c
// hiscore_flash.h - persist the hs_table_t in the last 4 KB flash sector.
// CONTRACT: hsflash_save() only while audio is stopped (sfxring_stop) and
// no display flush is running (st7796_flush_busy() == false) - flash ops
// stall the QMI, which also serves PSRAM (framebuffers).
#ifndef HISCORE_FLASH_H
#define HISCORE_FLASH_H
#include "hiscore.h"

void hsflash_load(hs_table_t *t);          /* bad/blank flash -> empty table */
void hsflash_save(const hs_table_t *t);
#endif
```

```c
#include "hiscore_flash.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

#define HS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

void hsflash_load(hs_table_t *t)
{
    const uint8_t *p = (const uint8_t *)(XIP_BASE + HS_OFFSET);
    uint8_t blob[HS_BLOB_SIZE];
    memcpy(blob, p, HS_BLOB_SIZE);
    if (!hs_decode(t, blob)) hs_clear(t);
}

void hsflash_save(const hs_table_t *t)
{
    static uint8_t page[FLASH_PAGE_SIZE];      /* 256 B */
    memset(page, 0xFF, sizeof page);
    hs_encode(t, page);
    uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(HS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(HS_OFFSET, page, FLASH_PAGE_SIZE);
    restore_interrupts(irq);
}
```

- [ ] **Step 5: Add all four to the app source list**

```cmake
add_executable(typing_invaders
    main.c hints.c words.c aliens.c game.c sfx.c hiscore.c shake.c fb.c render.c
    sfx_ring.c ledfx.c haptic.c hiscore_flash.c)
```

- [ ] **Step 6: Build for target**

Run: `python tools/fw.py build`
Expected: clean build, `typing_invaders.elf` produced. Fix any compile errors before committing (host tests can't cover these files).

- [ ] **Step 7: Commit**

```bash
git add apps/typing_invaders
git commit -m "feat: hardware bindings - audio ring, ledfx, haptic, flash scores"
```

---

### Task 14: Main integration — state machine, input, on-target smoke test

**Files:**
- Modify: `apps/typing_invaders/main.c` (replace skeleton with the full game loop)

**Interfaces:**
- Consumes: everything above, plus BSP: `board_init`, `psram_init`, `st7796_*`, `ft6336_poll`, `uartkbd_*`, `fw2kb_*`, `bmi323_*`, `PSRAM_BASE`.
- Produces: the shipping game binary.

- [ ] **Step 1: Replace `apps/typing_invaders/main.c` entirely**

```c
// typing_invaders - main: board bring-up, 30 Hz fixed-step loop, screen state
// machine. Input: fw2kb chords via uartkbd; touch = space/backspace; shake =
// smart bomb. Double-buffered PSRAM framebuffers flushed by DMA.
#include <string.h>
#include "fw2.h"
#include "platform/diag.h"
#include "platform/psram.h"
#include "pico/stdlib.h"
#include "fb.h"
#include "render.h"
#include "game.h"
#include "words.h"
#include "sfx.h"
#include "sfx_ring.h"
#include "ledfx.h"
#include "haptic.h"
#include "hiscore.h"
#include "hiscore_flash.h"
#include "shake.h"

typedef enum { SCR_TITLE, SCR_INTRO, SCR_GAME, SCR_OVER, SCR_INITIALS,
               SCR_SCORES } screen_t;

static fw2kb_t    s_kb;
static game_t     s_game;
static sfx_t      s_sfx;
static hs_table_t s_hs;
static shake_t    s_shake;
static screen_t   s_screen = SCR_TITLE;
static int        s_timer;              /* ticks left on timed screens */
static render_fx_t s_fx;
static uint32_t   s_frame;              /* title animation counter */
static char       s_initials[4];
static int        s_ini_pos;
static uint16_t  *s_fbufs[2];
static int        s_backbuf;

static void start_run(void)
{
    uint32_t seed = (uint32_t)time_us_64() | 1u;
    words_init(seed);
    game_init(&s_game, seed);
    game_start(&s_game);
    fw2kb_set_mode(&s_kb, FW2KB_MODE_LOWER);
    memset(&s_fx, 0, sizeof s_fx);
    ledfx_set_level(1);
    s_screen = SCR_INTRO;
    s_timer = 45;
}

static void save_scores(void)
{
    /* contract: audio stopped + flush idle before touching flash (QMI) */
    sfxring_stop();
    while (st7796_flush_busy()) tight_loop_contents();
    hsflash_save(&s_hs);
    sfxring_resume();
}

static void handle_game_events(void)
{
    game_event_t e;
    while (game_next_event(&s_game, &e)) {
        switch (e.type) {
        case GE_ZAP:
            sfx_trigger(&s_sfx, SFX_ZAP, e.arg);
            if (s_game.target >= 0 && s_game.aliens[s_game.target].active) {
                const alien_t *a = &s_game.aliens[s_game.target];
                s_fx.laser_x = (a->x_q8 >> 8) + a->w_px / 2;
                s_fx.laser_y = (a->y_q8 >> 8) + a->h_px;
                s_fx.laser_ttl = 3;
            }
            break;
        case GE_WRONG:
            sfx_trigger(&s_sfx, SFX_WRONG, 0);
            haptic_pulse(30);
            break;
        case GE_KILL:
            if (e.arg != SP_BOSS) sfx_trigger(&s_sfx, SFX_KILL, 0);
            break;
        case GE_SHIELD_POP:
            sfx_trigger(&s_sfx, SFX_ZAP, 8);
            break;
        case GE_BOSS_CHUNK:
            sfx_trigger(&s_sfx, SFX_ZAP, 6);
            haptic_pulse(40);
            break;
        case GE_BOSS_KILL:
            sfx_trigger(&s_sfx, SFX_BOSS_BOOM, 0);
            haptic_double(120, 100);
            break;
        case GE_LIFE_LOST:
            sfx_trigger(&s_sfx, SFX_WRONG, 0);
            haptic_pulse(120);
            break;
        case GE_LEVEL_UP:
            sfx_trigger(&s_sfx, SFX_LEVELUP, 0);
            ledfx_rainbow();
            ledfx_set_level((int)e.arg);
            s_screen = SCR_INTRO;
            s_timer = 45;
            break;
        case GE_BOMB:
            sfx_trigger(&s_sfx, SFX_BOMB, 0);
            haptic_pulse(300);
            ledfx_white();
            s_fx.flash_ttl = 3;
            break;
        case GE_GAME_OVER: {
            sfx_trigger(&s_sfx, SFX_GAMEOVER, 0);
            uint16_t wpm = (uint16_t)game_avg_wpm(&s_game);
            hs_note_wpm(&s_hs, wpm);
            s_screen = SCR_OVER;
            s_timer = 90;
            ledfx_danger(false);
            break;
        }
        default: break;
        }
    }
}

static void handle_char(char c)
{
    switch (s_screen) {
    case SCR_TITLE:
        sfx_trigger(&s_sfx, SFX_LOOP_STOP, 0);
        start_run();
        break;
    case SCR_GAME:
        game_char(&s_game, c);
        break;
    case SCR_INITIALS:
        if (c >= 'a' && c <= 'y') c = (char)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Y' && s_ini_pos < 3) {
            s_initials[s_ini_pos++] = c;
            sfx_trigger(&s_sfx, SFX_ZAP, s_ini_pos);
            if (s_ini_pos == 3) {
                hs_insert(&s_hs, s_initials,
                          s_game.score, (uint16_t)game_avg_wpm(&s_game));
                save_scores();
                s_screen = SCR_SCORES;
            }
        }
        break;
    case SCR_SCORES:
        s_screen = SCR_TITLE;
        sfx_trigger(&s_sfx, SFX_TITLE_LOOP, 0);
        break;
    default: break;
    }
}

static void handle_key_event(const fw2kb_event *ev)
{
    if (ev->key == FW2KB_KEY_CHAR) { handle_char(ev->ch); return; }
    if (ev->key == FW2KB_KEY_BACKSPACE && s_screen == SCR_INITIALS && s_ini_pos > 0)
        s_initials[--s_ini_pos] = '\0';
}

static void game_tick_and_fx(void)
{
    game_tick(&s_game);
    handle_game_events();
    if (s_fx.laser_ttl > 0) s_fx.laser_ttl--;
    if (s_fx.flash_ttl > 0) s_fx.flash_ttl--;
    bool danger = false;
    for (int i = 0; i < GAME_MAX_ALIENS; i++) {
        const alien_t *a = &s_game.aliens[i];
        if (a->active && (a->y_q8 >> 8) + a->h_px > GAME_DEFENSE_Y * 2 / 3)
            danger = true;
    }
    ledfx_danger(danger);
}

static void render_current(void)
{
    if (st7796_flush_busy()) return;             /* skip frame, logic goes on */
    s_backbuf ^= 1;
    fb_set(s_fbufs[s_backbuf]);
    switch (s_screen) {
    case SCR_TITLE:    render_title(&s_hs, s_frame); break;
    case SCR_INTRO:    render_level_intro(s_game.level); break;
    case SCR_GAME:     render_game(&s_game, &s_fx); break;
    case SCR_OVER:     render_game_over(&s_game); break;
    case SCR_INITIALS: render_initials(s_initials, s_ini_pos, s_game.score); break;
    case SCR_SCORES:   render_hiscores(&s_hs); break;
    }
    st7796_flush_async(0, 0, ST7796_W - 1, ST7796_H - 1,
                       s_fbufs[s_backbuf], NULL);
}

int main(void)
{
    board_init();
    size_t psram = psram_init();
    if (psram < 2u * 1024 * 1024) {
        DIAG("typing_invaders: PSRAM absent/too small (%u) - halting\n",
             (unsigned)psram);
        for (;;) tight_loop_contents();
    }
    s_fbufs[0] = (uint16_t *)PSRAM_BASE;
    s_fbufs[1] = (uint16_t *)(PSRAM_BASE + 0x100000);   /* +1 MB */

    st7796_init();
    board_backlight_set(1);
    ft6336_init();
    uartkbd_init();
    fw2kb_init(&s_kb);
    fw2kb_set_mode(&s_kb, FW2KB_MODE_LOWER);
    fw2kb_set_touch_threshold(&s_kb, ST7796_H / 2);
    sfx_init(&s_sfx);
    sfxring_init(&s_sfx);
    ledfx_init();
    haptic_init();
    shake_init(&s_shake);
    bool have_imu = bmi323_init();
    hsflash_load(&s_hs);
    sfx_trigger(&s_sfx, SFX_TITLE_LOOP, 0);
    DIAG("typing_invaders up: imu=%d scores=%d best_wpm=%u\n",
         (int)have_imu, s_hs.count, (unsigned)s_hs.best_wpm);

    uint64_t next_tick = time_us_64();
    bool was_touch = false;
    uint32_t imu_div = 0;

    for (;;) {
        /* --- input --- */
        uartkbd_task();
        uartkbd_event_t uev;
        while (uartkbd_next_event(&uev)) {
            if (!uev.pressed) continue;
            if (uev.btn <= UARTKBD_BTN_RED) {
                fw2kb_press(&s_kb, (fw2kb_btn)uev.btn);
                ledfx_chord_flash((int)uev.btn);
            } else if (uev.btn == UARTKBD_BTN_PAGE) {
                fw2kb_press(&s_kb, FW2KB_BTN_AI);
            }
        }
        uint16_t tx, ty;
        bool touch = ft6336_poll(&tx, &ty);
        if (touch && !was_touch) fw2kb_touch(&s_kb, (int)tx, (int)ty);
        was_touch = touch;
        fw2kb_event kev;
        while (fw2kb_next_event(&s_kb, &kev)) handle_key_event(&kev);

        /* --- fixed-step logic --- */
        uint64_t now = time_us_64();
        if (now >= next_tick) {
            next_tick += GAME_TICK_MS * 1000ull;
            if (next_tick + 100000 < now) next_tick = now;   /* fell behind */
            s_frame++;
            switch (s_screen) {
            case SCR_INTRO:
                if (--s_timer <= 0) s_screen = SCR_GAME;
                break;
            case SCR_GAME:
                game_tick_and_fx();
                if (have_imu) {          /* 30 Hz sampling catches real shakes */
                    bmi323_reading_t m;
                    if (bmi323_read(&m) &&
                        shake_feed(&s_shake, m.ax, m.ay, m.az,
                                   (uint32_t)(now / 1000)))
                        game_shake(&s_game);
                }
                break;
            case SCR_OVER:
                if (--s_timer <= 0) {
                    if (hs_rank(&s_hs, s_game.score) >= 0) {
                        memset(s_initials, 0, sizeof s_initials);
                        s_ini_pos = 0;
                        fw2kb_set_mode(&s_kb, FW2KB_MODE_UPPER);
                        s_screen = SCR_INITIALS;
                    } else {
                        save_scores();       /* persists best_wpm */
                        s_screen = SCR_SCORES;
                    }
                }
                break;
            default: break;
            }
            render_current();
        }

        /* --- background services --- */
        sfxring_pump();
        ledfx_task();
        haptic_task();
        sleep_us(500);
    }
}
```

- [ ] **Step 2: Build**

Run: `python tools/fw.py build`
Expected: clean build. Also re-run `python tools/fw.py test` — all host tests still pass.

- [ ] **Step 3: Flash and smoke-test on hardware**

Run: `python tools/fw.py flash` then `python tools/fw.py rtt`
Expected RTT: `typing_invaders up: imu=1 scores=0 best_wpm=0`.

Manual checklist (walk it on the device; note failures, fix, re-flash):

1. **Title**: logo, blinking "PRESS ANY CHORD TO START", bass loop audible, LEDs dim glow.
2. **Start**: type any letter (two presses) → WAVE 1 card → aliens fall.
3. **Typing**: correct letter → zap sound (pitch rises with streak), laser flash, chord press flashes the matching LED segment; wrong letter → low blip + haptic tick.
4. **Hints**: level 1-3 dots under every letter; check colors match the physical keycaps.
5. **Kill/score**: word completes → explosion noise, score & multiplier update; WPM readout moves.
6. **Life loss**: let one land → haptic pulse, life icon gone, streak reset; LEDs pulse red when aliens near the line.
7. **Level-up**: clear the wave → jingle + rainbow sweep + WAVE 2 card.
8. **Boss (wave 5)**: big red alien with phrase + health bar; chunks knock the bar down; kill = boom + double buzz.
9. **Smart bomb**: shake hard → white flash, 300 ms buzz, LEDs white, aliens gone, no score gain; second shake does nothing until next level.
10. **Game over**: stats card (score/avg WPM/accuracy/wave) → initials entry in UPPER mode (chords type, top-half tap backspaces) → hall of fame shows entry.
11. **Persistence**: power-cycle; title shows the saved high score + best WPM.
12. **Audio underrun check**: leave the title screen running 10 minutes; loop must not glitch (wall-clock playhead drift check).

- [ ] **Step 4: Commit**

```bash
git add apps/typing_invaders/main.c
git commit -m "feat: main loop - state machine, input, av wiring"
```

---

## Post-plan notes for the executor

- **Tuning knobs** all live in one place each: fall speed / budgets (`level_*` functions in `game.c`), SFX presets (`sfx_trigger`), LED brightness (`ledfx_init`), shake threshold (`shake.c` defines). Expect a tuning pass after first real play; keep changes in those functions.
- **Deviation from spec**: boss phrases auto-advance across spaces (the chordboard has no space chord — space is a touch gesture). The spec's "space typed to confirm" was replaced by GE_BOSS_CHUNK auto-advance; the design doc has been updated.
- If `fw test` can't find MinGW GCC on Windows, install it (e.g. `winget install BrewSoftware.MinGW` or MSYS2) — the wilibsp `_host_toolchain_args()` picks it up automatically.
