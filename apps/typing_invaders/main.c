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
static uint32_t   s_frame;              /* fixed-tick animation counter */
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
            s_fx.laser_x = s_game.zap_x_px;
            s_fx.laser_y = s_game.zap_y_px;
            s_fx.laser_ttl = 3;
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
        if (c == ' ') break;               /* touch space: not a game letter */
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
                if (fw2kb_in_chord(&s_kb))          /* cancel half-entered chord */
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
