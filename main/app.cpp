/*
 * app.cpp - screens, input and main loop of Blips for Gamebuino AKA.
 *
 * A port of "Blips" by Willems Davy (joyrider3774), MIT License. The rules and the level format live
 * in engine/ (hardware independent, derived from the original sources); everything here sits on the
 * Gamebuino-AKA library, so that the very same code runs on the console and in the SDL2 desktop build.
 *
 * SPDX-License-Identifier: MIT
 */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "gamebuino.h"
#include "gb_ll_lcd.h"
#include "gb_ll_system.h"

#include "assets/tiles.h"
#include "audio.h"
#include "engine/blips_game.h"
#include "gfx.h"
#include "i18n.h"
#include "packs.h"
#include "platform.h"
#include "save.h"

extern gb_core g_core;
extern gb_graphics g_gfx;

using i18n::tr;
using namespace blips;

static_assert((int)audio::SFX_COUNT == (int)blips::SND_COUNT, "audio and engine sound lists must match");

namespace app {
namespace {

// ================================================================================ colours
struct Palette {
    uint16_t white, black, yellow, orange, green, red, gray, dim, navy, navy2, border, shadow, cell, grid;
    uint16_t bg_top[6], bg_bot[6];
};
Palette P;

void init_palette()
{
    using gfx::rgb;
    P.white = rgb(255, 255, 255);  P.black = rgb(0, 0, 0);
    P.yellow = rgb(255, 222, 90);  P.orange = rgb(240, 150, 50);
    P.green = rgb(110, 220, 120);  P.red = rgb(240, 90, 80);
    P.gray = rgb(190, 196, 210);   P.dim = rgb(110, 118, 140);
    P.navy = rgb(18, 26, 48);      P.navy2 = rgb(34, 46, 80);
    P.border = rgb(120, 150, 220); P.shadow = rgb(6, 8, 16);
    P.cell = rgb(20, 24, 38);      P.grid = rgb(34, 40, 60);
    const int tops[6][3] = { {40, 60, 110}, {70, 40, 100}, {30, 90, 90}, {100, 60, 40}, {50, 80, 50}, {90, 50, 70} };
    for (int i = 0; i < 6; ++i) {
        P.bg_top[i] = rgb(tops[i][0], tops[i][1], tops[i][2]);
        P.bg_bot[i] = rgb(tops[i][0] / 3, tops[i][1] / 3, tops[i][2] / 3);
    }
}

// ================================================================================ input
const uint16_t KEYS[] = {
    gb_buttons::KEY_LEFT, gb_buttons::KEY_RIGHT, gb_buttons::KEY_UP, gb_buttons::KEY_DOWN,
    gb_buttons::KEY_A, gb_buttons::KEY_B, gb_buttons::KEY_C, gb_buttons::KEY_D,
    gb_buttons::KEY_L1, gb_buttons::KEY_R1, gb_buttons::KEY_MENU, gb_buttons::KEY_RUN };
constexpr int NKEYS = (int)(sizeof KEYS / sizeof KEYS[0]);
enum { K_LEFT, K_RIGHT, K_UP, K_DOWN, K_A, K_B, K_C, K_D, K_L1, K_R1, K_MENU, K_RUN };

constexpr uint32_t REPEAT_DELAY_MS = 380, REPEAT_RATE_MS = 90;

struct Input {
    bool now[NKEYS] = {}, prev[NKEYS] = {}, rep[NKEYS] = {};
    uint32_t next_rep[NKEYS] = {};

    void poll(uint32_t t)
    {
        const uint16_t st = g_core.buttons.state() | g_core.joystick.state();
        for (int i = 0; i < NKEYS; ++i) {
            prev[i] = now[i];
            now[i] = (st & KEYS[i]) != 0;
            rep[i] = false;
            if (now[i] && !prev[i]) next_rep[i] = t + REPEAT_DELAY_MS;
            else if (now[i] && (int32_t)(t - next_rep[i]) >= 0) { rep[i] = true; next_rep[i] = t + REPEAT_RATE_MS; }
        }
    }
    bool held(int k) const { return now[k]; }
    bool pressed(int k) const { return now[k] && !prev[k]; }
    bool released(int k) const { return !now[k] && prev[k]; }
    bool repeat(int k) const { return pressed(k) || rep[k]; }
};
Input in;

// ================================================================================ state
enum Screen { SC_TITLE, SC_PACKS, SC_LEVELS, SC_PLAY, SC_PAUSE, SC_OPTIONS, SC_CREDITS, SC_HOWTO,
              SC_ED_PACKS, SC_ED_NAME, SC_ED_LEVELS, SC_ED_EDIT, SC_ED_MENU };

enum PlayState { PS_RUN, PS_DONE_WAIT, PS_DONE, PS_DEAD, PS_CONF_LEVELS, PS_CONF_RELOAD };

save::Config cfg;
Screen screen = SC_TITLE;
Screen options_from = SC_TITLE;
uint32_t t_now = 0;
char toast[48] = "";
uint32_t toast_until = 0;

void show_toast(const char* s)
{
    snprintf(toast, sizeof toast, "%s", s);
    toast_until = t_now + 2200;
}

int pack_sel = 0, pack_top = 0;
int cur_pack = -1;

constexpr int LV_COLS = 7, LV_ROWS = 5, LV_CW = 32, LV_CH = 24, LV_X = 8, LV_Y = 30;
int lv_sel = 0, lv_top = 0;
int unl = 1;                    // unlocked levels of the current pack

Game game;                      // the level being played, and also the one being edited
int cur_level = 0;
PlayState ps = PS_RUN;
int done_wait = 0;
bool test_mode = false;         // playing a level from the editor (nothing is saved)
Controls ctl;

int menu_sel = 0;
int opt_sel = 0;
int credits_page = 0;

// ================================================================================ helpers
void sfx(audio::Sfx s) { audio::play(s); }

void apply_audio_settings() { audio::set_volumes(cfg.music, cfg.sfx); }

void on_sound(void*, int s) { if (s >= 0 && s < audio::SFX_COUNT) audio::play((audio::Sfx)s); }

void blit(int id, int x, int y)
{
    g_gfx.drawImage((int16_t)x, (int16_t)y, TILES_16, TS, (uint16_t)(T_COUNT * TS), 0, (uint16_t)(id * TS), TS, TS, TILE_KEY);
}

void blit_scaled(int id, int x, int y, int dst)
{
    g_gfx.drawImageScaled((int16_t)x, (int16_t)y, (uint16_t)dst, (uint16_t)dst, TILES_16, TS, (uint16_t)(T_COUNT * TS),
                          0, (uint16_t)(id * TS), TS, TS, TILE_KEY);
}

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int tile_for(int type, int phase)
{
    switch (type) {
        case ID_FLOOR: return T_FLOOR;
        case ID_WALL: return T_WALL;
        case ID_WALLBREAKABLE: return T_WALLB;
        case ID_DIAMOND: return T_COIN;
        case ID_BOMB: return T_BOMB;
        case ID_BOX: return T_BOX;
        case ID_BOX1: return T_BOX1;
        case ID_BOX2: return T_BOX2;
        case ID_BOXBOMB: return T_BOXBOMB;
        case ID_BOXWALL: return T_BOXWALL;
        case ID_EMPTY: return T_ERASE;
        case ID_EXPLOSION: return T_EXPL + clampi(phase, 0, 7);
        case ID_PLAYER: return T_PLAYER + clampi(phase, 0, 15);
        case ID_PLAYER2: return T_PLAYER2 + clampi(phase, 0, 15);
        default: return -1;
    }
}

// The engine calls this for every part it draws: (type, animation phase, screen position).
void paint(void*, int type, int phase, int sx, int sy)
{
    const int t = tile_for(type, phase);
    if (t >= 0) blit(t, sx, sy);
}
const Painter painter = { paint, nullptr };

// Draws a world without advancing it (pause, dialogs, editor). y0: top of the map area on screen.
void draw_world(const World& w, int camx, int camy, int y0)
{
    for (int i = 0; i < w.ItemCount; ++i) {
        const Part* p = w.Items[i];
        if (p->NeedHide()) continue;
        const int sx = p->X - camx, sy = p->Y - camy + y0;
        if (sx <= -TS || sx >= gfx::W || sy <= y0 - TS || sy >= gfx::H) continue;
        paint(nullptr, p->Type, p->AnimPhase, sx, sy);
    }
}

// Copies at most `maxc` UTF-8 characters.
void fit(const char* src, int maxc, char* dst, size_t n)
{
    size_t o = 0;
    int c = 0;
    while (*src && c < maxc) {
        size_t len = 1;
        const uint8_t b = (uint8_t)*src;
        if (b >= 0xF0) len = 4; else if (b >= 0xE0) len = 3; else if (b >= 0xC0) len = 2;
        for (size_t i = 0; i < len && src[i]; ++i) if (o + 1 < n) dst[o++] = src[i];
        for (size_t i = 0; i < len && *src; ++i) ++src;
        ++c;
    }
    dst[o] = 0;
}

int hash_str(const char* s)
{
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return (int)(h % 6);
}

void background()
{
    const bool pack_bg = cur_pack >= 0 && cur_pack < packs::count && screen != SC_TITLE && screen < SC_ED_PACKS && !test_mode;
    const int k = pack_bg ? hash_str(packs::list[cur_pack].dir) : 0;
    gfx::gradient(P.bg_top[k], P.bg_bot[k]);
}

void header(const char* title)
{
    gfx::rect(0, 0, gfx::W, 22, P.navy);
    gfx::rect(0, 22, gfx::W, 1, P.border);
    gfx::text_center(7, title, P.yellow);
}

void footer(const char* hint)
{
    gfx::rect(0, gfx::H - 14, gfx::W, 14, P.navy);
    gfx::rect(0, gfx::H - 14, gfx::W, 1, P.border);
    gfx::text_center(gfx::H - 11, hint, P.gray);
}

// Moves a menu cursor with up/down (wrapping). Returns true when it moved.
bool nav(int& sel, int n)
{
    if (n <= 0) return false;
    if (in.repeat(K_DOWN)) { sel = (sel + 1) % n; sfx(audio::SFX_MENU); return true; }
    if (in.repeat(K_UP)) { sel = (sel + n - 1) % n; sfx(audio::SFX_MENU); return true; }
    return false;
}

void menu_box(int x, int y, int w, const char* title, const i18n::Str* items, int n, int sel)
{
    gfx::panel(x, y, w, 28 + n * 16, P.navy, P.yellow);
    gfx::text_center(y + 8, title, P.yellow);
    for (int i = 0; i < n; ++i) {
        const int yy = y + 28 + i * 16;
        if (i == sel) { gfx::rect(x + 6, yy - 3, w - 12, 14, P.navy2); gfx::text(x + 10, yy, ">", P.yellow); }
        gfx::text_center(yy, tr(items[i]), i == sel ? P.yellow : P.white);
    }
}

// ================================================================================ level preview
// Reads the raw level file (type, x, y bytes) and keeps the top part of each tile.
struct Preview {
    uint8_t cell[N_ROWS][N_COLS];
    int minx, miny, maxx, maxy, coins;
    bool ok;
};
Preview pv;
int pv_pack = -1, pv_level = -1;

int part_rank(int t)
{
    static const int8_t R[16] = { 0, 0, 10, 5, 1, 3, 4, 3, 10, 5, 5, 5, 5, 4, 0, 0 };
    return (t >= 0 && t < 16) ? R[t] : 0;
}

void load_preview_file(const char* path)
{
    memset(&pv, 0, sizeof pv);
    pv.minx = pv.miny = N_COLS;
    pv.maxx = pv.maxy = -1;
    FILE* f = fopen(path, "rb");
    if (!f) return;
    uint8_t r[3];
    while (fread(r, 1, 3, f) == 3) {
        const int t = (int8_t)r[0], x = (int8_t)r[1], y = (int8_t)r[2];
        if (x < 0 || x >= N_COLS || y < 0 || y >= N_ROWS || t < 1 || t > 13) continue;
        if (part_rank(t) >= part_rank(pv.cell[y][x])) pv.cell[y][x] = (uint8_t)t;
        if (t == ID_DIAMOND) ++pv.coins;
        if (x < pv.minx) pv.minx = x;
        if (x > pv.maxx) pv.maxx = x;
        if (y < pv.miny) pv.miny = y;
        if (y > pv.maxy) pv.maxy = y;
    }
    fclose(f);
    pv.ok = pv.maxx >= 0;
}

void ensure_preview(const packs::Pack& p, int pack_idx, int level)
{
    if (pv_pack == pack_idx && pv_level == level) return;
    pv_pack = pack_idx;
    pv_level = level;
    char path[700];
    if (packs::level_path(p, level, path, sizeof path)) load_preview_file(path);
    else { memset(&pv, 0, sizeof pv); }
}

uint16_t part_color(int t)
{
    using gfx::rgb;
    switch (t) {
        case ID_FLOOR: return rgb(70, 84, 104);
        case ID_WALL: return rgb(120, 126, 140);
        case ID_WALLBREAKABLE: return rgb(160, 150, 150);
        case ID_DIAMOND: return rgb(255, 230, 90);
        case ID_BOMB: return rgb(230, 60, 50);
        case ID_BOX: case ID_BOXWALL: return rgb(210, 140, 60);
        case ID_BOXBOMB: return rgb(240, 100, 60);
        case ID_BOX1: return rgb(90, 200, 90);
        case ID_BOX2: return rgb(80, 150, 240);
        case ID_PLAYER: return rgb(120, 255, 140);
        case ID_PLAYER2: return rgb(130, 200, 255);
        default: return 0;
    }
}

void draw_preview(int x, int y, int w, int h)
{
    if (!pv.ok) return;
    const int cw = pv.maxx - pv.minx + 1, ch = pv.maxy - pv.miny + 1;
    int s = w / cw;
    const int s2 = h / ch;
    if (s2 < s) s = s2;
    s = clampi(s, 1, 6);
    const int x0 = x + (w - cw * s) / 2, y0 = y + (h - ch * s) / 2;
    for (int j = 0; j < ch; ++j)
        for (int i = 0; i < cw; ++i) {
            const int t = pv.cell[pv.miny + j][pv.minx + i];
            if (t) gfx::rect(x0 + i * s, y0 + j * s, s, s, part_color(t));
        }
}

// ================================================================================ packs / levels
void refresh_unlocked()
{
    const packs::Pack& p = packs::list[cur_pack];
    unl = cfg.unlock_all ? p.count : save::unlocked(p.dir, p.count);
}

void enter_levels()
{
    refresh_unlocked();
    lv_sel = unl - 1;
    const packs::Pack& p = packs::list[cur_pack];
    if (strcmp(cfg.last_pack, p.dir) == 0 && cfg.last_level < unl) lv_sel = cfg.last_level;
    lv_top = 0;
    pv_pack = pv_level = -1;
    screen = SC_LEVELS;
}

void setup_play()
{
    game.world.sound_cb = on_sound;
    game.world.sound_user = nullptr;
    game.look_reset = false;
    ps = PS_RUN;
    done_wait = 0;
    ctl = Controls();
}

bool begin_level(int idx)
{
    const packs::Pack& p = packs::list[cur_pack];
    if (!packs::load_level(game.world, p, idx) || game.world.LevelError() != 0) return false;
    setup_play();
    cur_level = idx;
    test_mode = false;
    cfg.last_level = (uint16_t)idx;
    snprintf(cfg.last_pack, sizeof cfg.last_pack, "%s", p.dir);
    return true;
}

void finish_level()
{
    sfx(audio::SFX_STAGEEND);
    if (test_mode) return;
    const packs::Pack& p = packs::list[cur_pack];
    if (cur_level + 2 > unl && !cfg.unlock_all) save::set_unlocked(p.dir, cur_level + 2);
    refresh_unlocked();
}

// ---- editor <-> test play (declared here, defined with the editor)
void ed_start_test();
void ed_back_from_test();
void ed_restart_test();

void restart_play()
{
    if (test_mode) ed_restart_test();
    else if (!begin_level(cur_level)) { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_LOAD_ERROR)); }
}

void leave_play()          // "level select" for a normal game, back to the drawing board in test mode
{
    if (test_mode) ed_back_from_test();
    else enter_levels();
}

void update_play()
{
    ctl = Controls();
    switch (ps) {
        case PS_RUN:
            ctl.left = in.held(K_LEFT);  ctl.right = in.held(K_RIGHT);
            ctl.up = in.held(K_UP);      ctl.down = in.held(K_DOWN);
            ctl.look = in.held(K_D);
            if (in.pressed(K_A) && !ctl.look && game.world.Player2 && game.world.Player && !game.world.Player->IsMoving &&
                !game.world.AnyPlayerDead())
                game.world.SwitchPlayers();
            if (in.pressed(K_B)) {
                sfx(audio::SFX_BACK);
                if (test_mode) { ed_back_from_test(); return; }
                ps = PS_CONF_LEVELS;
                return;
            }
            if (in.pressed(K_C)) {
                sfx(audio::SFX_BACK);
                if (test_mode) { restart_play(); return; }
                ps = PS_CONF_RELOAD;
                return;
            }
            if (in.pressed(K_L1)) { audio::music_prev(); }
            if (in.pressed(K_R1)) { audio::music_next(); }
            if (game.stage_done()) { ps = PS_DONE_WAIT; done_wait = 8; }          // 250 ms, like upstream
            else if (game.lost()) ps = PS_DEAD;
            break;
        case PS_DONE_WAIT:
            if (--done_wait <= 0) { finish_level(); ps = PS_DONE; }
            break;
        case PS_DONE:
            if (in.pressed(K_A)) {
                sfx(audio::SFX_SELECT);
                if (test_mode) ed_back_from_test();
                else if (cur_level + 1 < packs::list[cur_pack].count) begin_level(cur_level + 1);
                else enter_levels();
            } else if (in.pressed(K_B)) { sfx(audio::SFX_BACK); leave_play(); }
            else if (in.pressed(K_C)) { sfx(audio::SFX_SELECT); restart_play(); }
            break;
        case PS_DEAD:
            if (in.pressed(K_A)) { sfx(audio::SFX_SELECT); restart_play(); }
            else if (in.pressed(K_B)) { sfx(audio::SFX_BACK); leave_play(); }
            break;
        case PS_CONF_LEVELS:
            if (in.pressed(K_A)) { sfx(audio::SFX_SELECT); enter_levels(); }
            else if (in.pressed(K_B)) { sfx(audio::SFX_BACK); ps = PS_RUN; }
            break;
        case PS_CONF_RELOAD:
            if (in.pressed(K_A)) { sfx(audio::SFX_SELECT); restart_play(); }
            else if (in.pressed(K_B)) { sfx(audio::SFX_BACK); ps = PS_RUN; }
            break;
    }
}

void draw_hud()
{
    char buf[64];
    if (test_mode) snprintf(buf, sizeof buf, "TEST");
    else {
        char nm[32];
        fit(packs::list[cur_pack].name, 18, nm, sizeof nm);
        snprintf(buf, sizeof buf, "%s %d/%d", nm, cur_level + 1, packs::list[cur_pack].count);
    }
    gfx::text_shadow(3, 3, buf, P.yellow, P.shadow);
    snprintf(buf, sizeof buf, "%s %d", tr(i18n::S_COINS), game.world.Count(ID_DIAMOND));
    gfx::text_shadow(gfx::W - gfx::text_width(buf) - 3, 3, buf, P.white, P.shadow);
}

void dialog(const char* l1, uint16_t c1, const char* l2, const char* l3, const char* help)
{
    int lines = 1 + (l2 ? 1 : 0) + (l3 ? 1 : 0);
    const int h = 20 + lines * 16 + 18;
    const int y = (gfx::H - h) / 2;
    gfx::panel(30, y, 260, h, P.navy, P.yellow);
    int yy = y + 10;
    gfx::text_center(yy, l1, c1); yy += 16;
    if (l2) { gfx::text_center(yy, l2, P.white); yy += 16; }
    if (l3) { gfx::text_center(yy, l3, P.green); yy += 16; }
    gfx::text_center(yy + 4, help, P.gray);
}

// advance: step the game (30 Hz) while drawing; otherwise draw it as it stands.
void draw_play(bool advance)
{
    background();
    if (advance) game.tick(ctl, &painter);
    else draw_world(game.world, game.world.VP->MinScreenX, game.world.VP->MinScreenY, 0);
    draw_hud();
    switch (ps) {
        case PS_DONE: {
            const bool last = !test_mode && cur_level + 1 >= packs::list[cur_pack].count;
            dialog(tr(test_mode ? i18n::S_ED_TEST_OK : i18n::S_SOLVED), test_mode ? P.green : P.yellow,
                   last ? tr(i18n::S_PACK_DONE) : nullptr, nullptr,
                   (last || test_mode) ? tr(i18n::S_BACK) : tr(i18n::S_SOLVED_NEXT));
            break;
        }
        case PS_DEAD:
            dialog(tr(i18n::S_DIED), P.red, nullptr, nullptr, tr(i18n::S_DIED_HELP));
            break;
        case PS_CONF_LEVELS:
            dialog(tr(i18n::S_CONFIRM_LEVELS), P.yellow, nullptr, nullptr, tr(i18n::S_CONFIRM_HELP));
            break;
        case PS_CONF_RELOAD:
            dialog(tr(i18n::S_CONFIRM_RELOAD), P.yellow, nullptr, nullptr, tr(i18n::S_CONFIRM_HELP));
            break;
        default: break;
    }
}

// ================================================================================ title
void ed_enter();

const i18n::Str TITLE_ITEMS[6] = { i18n::S_PLAY, i18n::S_EDITOR, i18n::S_HOWTO, i18n::S_OPTIONS, i18n::S_CREDITS, i18n::S_QUIT };

void update_title()
{
    nav(menu_sel, 6);
    if (in.pressed(K_A)) {
        sfx(audio::SFX_SELECT);
        switch (menu_sel) {
            case 0: pack_sel = clampi(pack_sel, 0, packs::count > 0 ? packs::count - 1 : 0); screen = SC_PACKS; break;
            case 1: ed_enter(); break;
            case 2: screen = SC_HOWTO; break;
            case 3: options_from = SC_TITLE; opt_sel = 0; screen = SC_OPTIONS; break;
            case 4: credits_page = 0; screen = SC_CREDITS; break;
            default: save::save_config(cfg); plat::return_to_loader();
        }
    }
}

void draw_title()
{
    background();
    gfx::text_scaled_center(18, "BLIPS", P.shadow, 7);
    gfx::text_scaled_center(16, "BLIPS", P.yellow, 7);
    gfx::text_center(76, "Gamebuino AKA", P.white);
    const int ids[5] = { T_PLAYER + 4, T_BOX, T_BOMB, T_EXPL + 2, T_COIN };
    for (int i = 0; i < 5; ++i) blit_scaled(ids[i], 24 + i * 56, 94, 48);
    for (int i = 0; i < 6; ++i) {
        const int y = 146 + i * 11;
        const bool sel = i == menu_sel;
        if (sel) { gfx::rect(100, y - 2, 120, 11, P.navy2); gfx::text(106, y, ">", P.yellow); }
        gfx::text_center(y, tr(TITLE_ITEMS[i]), sel ? P.yellow : P.white);
    }
    gfx::text_center(gfx::H - 20, "Original game (c) Willems Davy - MIT", P.dim);
    gfx::text_center(gfx::H - 10, "AKA port: Jicehel", P.dim);
}

// ================================================================================ packs list
constexpr int PACK_ROWS = 11;

void update_packs()
{
    nav(pack_sel, packs::count);
    if (pack_sel < pack_top) pack_top = pack_sel;
    if (pack_sel >= pack_top + PACK_ROWS) pack_top = pack_sel - PACK_ROWS + 1;
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_TITLE; return; }
    if (in.pressed(K_A) && packs::count > 0) {
        if (packs::list[pack_sel].count > 0) { cur_pack = pack_sel; sfx(audio::SFX_SELECT); enter_levels(); }
        else { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_LOAD_ERROR)); }
    }
}

void draw_packs()
{
    background();
    header(tr(i18n::S_CHOOSE_PACK));
    if (packs::count == 0) gfx::text_center(110, tr(i18n::S_NO_PACKS), P.white);
    for (int r = 0; r < PACK_ROWS; ++r) {
        const int i = pack_top + r;
        if (i >= packs::count) break;
        const packs::Pack& p = packs::list[i];
        const int y = 30 + r * 17;
        const bool sel = i == pack_sel;
        if (sel) gfx::panel(6, y - 3, gfx::W - 12, 16, P.navy2, P.yellow);
        char nm[40];
        fit(p.name, 26, nm, sizeof nm);
        gfx::text(14, y + 1, nm, sel ? P.yellow : (p.user ? P.green : P.white));
        char right[24] = "";
        if (p.count > 0) {
            const int u = cfg.unlock_all ? p.count : save::unlocked(p.dir, p.count);
            snprintf(right, sizeof right, "%d/%d", u, p.count);
        }
        gfx::text(gfx::W - 14 - gfx::text_width(right), y + 1, right, sel ? P.white : P.gray);
    }
    if (pack_top > 0) gfx::text(gfx::W - 12, 26, "^", P.yellow);
    if (pack_top + PACK_ROWS < packs::count) gfx::text(gfx::W - 12, 30 + PACK_ROWS * 17 - 8, "v", P.yellow);
    footer(tr(i18n::S_HELP_MENU));
}

// ================================================================================ level select
void update_levels()
{
    const int maxsel = unl - 1;
    if (in.repeat(K_LEFT) && lv_sel > 0) { --lv_sel; sfx(audio::SFX_MENU); }
    if (in.repeat(K_RIGHT) && lv_sel < maxsel) { ++lv_sel; sfx(audio::SFX_MENU); }
    if (in.repeat(K_UP) && lv_sel - LV_COLS >= 0) { lv_sel -= LV_COLS; sfx(audio::SFX_MENU); }
    if (in.repeat(K_DOWN)) { lv_sel = lv_sel + LV_COLS <= maxsel ? lv_sel + LV_COLS : maxsel; sfx(audio::SFX_MENU); }
    if (in.repeat(K_L1)) { lv_sel = lv_sel >= 5 ? lv_sel - 5 : 0; sfx(audio::SFX_MENU); }
    if (in.repeat(K_R1)) { lv_sel = lv_sel + 5 <= maxsel ? lv_sel + 5 : maxsel; sfx(audio::SFX_MENU); }
    const int row = lv_sel / LV_COLS;
    if (row < lv_top) lv_top = row;
    if (row >= lv_top + LV_ROWS) lv_top = row - LV_ROWS + 1;
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_PACKS; return; }
    if (in.pressed(K_A)) {
        if (begin_level(lv_sel)) { sfx(audio::SFX_SELECT); screen = SC_PLAY; audio::music_game(); }
        else { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_LOAD_ERROR)); }
    }
}

void draw_level_grid(int count, int sel, int top, int unlocked, bool plus_last, bool highlight_next)
{
    for (int r = 0; r < LV_ROWS; ++r)
        for (int c = 0; c < LV_COLS; ++c) {
            const int i = (top + r) * LV_COLS + c;
            if (i >= count) continue;
            const int x = LV_X + c * LV_CW, y = LV_Y + r * LV_CH;
            const bool is_sel = i == sel, locked = i >= unlocked, plus = plus_last && i == count - 1;
            uint16_t fill = locked ? P.navy : (highlight_next && i == unlocked - 1 ? P.navy2 : gfx::rgb(30, 80, 56));
            if (plus) fill = P.navy;
            gfx::panel(x, y, LV_CW - 3, LV_CH - 3, fill, is_sel ? P.yellow : (locked ? P.dim : P.border));
            char n[16];
            snprintf(n, sizeof n, plus ? "+" : "%d", i + 1);
            gfx::text(x + (LV_CW - 3 - gfx::text_width(n)) / 2, y + (LV_CH - 3 - 8) / 2, n,
                      plus ? P.green : (locked ? P.dim : (is_sel ? P.yellow : P.white)));
        }
}

void draw_levels()
{
    background();
    const packs::Pack& p = packs::list[cur_pack];
    char hd[64];
    fit(p.name, 26, hd, sizeof hd);
    header(hd);
    draw_level_grid(p.count, lv_sel, lv_top, unl, false, !cfg.unlock_all);
    gfx::panel(238, 30, 76, 117, P.navy, P.border);
    ensure_preview(p, cur_pack, lv_sel);
    draw_preview(240, 32, 72, 113);
    char buf[64], shown[48];
    snprintf(buf, sizeof buf, tr(i18n::S_LEVEL_FMT), lv_sel + 1);
    gfx::text(8, 158, buf, P.yellow);
    if (p.creator[0]) {
        char who[48];
        fit(p.creator, 30, who, sizeof who);
        snprintf(shown, sizeof shown, tr(i18n::S_BY_FMT), who);
        gfx::text(8, 172, shown, P.gray);
    }
    snprintf(buf, sizeof buf, tr(i18n::S_LEVELS_FMT), p.count);
    gfx::text(8, 186, buf, P.dim);
    if (pv.ok) {
        snprintf(buf, sizeof buf, "%s %d", tr(i18n::S_COINS), pv.coins);
        gfx::text(gfx::W - 8 - gfx::text_width(buf), 158, buf, P.dim);
    }
    footer(tr(i18n::S_HELP_LEVELS));
}

// ================================================================================ pause / options / how to / credits
const i18n::Str PAUSE_ITEMS[5] = { i18n::S_RESUME, i18n::S_RESTART, i18n::S_LEVEL_SELECT, i18n::S_OPTIONS, i18n::S_TO_TITLE };

void update_pause()
{
    nav(menu_sel, 5);
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_PLAY; return; }
    if (in.pressed(K_A)) {
        sfx(audio::SFX_SELECT);
        switch (menu_sel) {
            case 0: screen = SC_PLAY; break;
            case 1: screen = SC_PLAY; restart_play(); break;
            case 2: screen = SC_PLAY; leave_play(); break;
            case 3: options_from = SC_PAUSE; opt_sel = 0; screen = SC_OPTIONS; break;
            default: test_mode = false; audio::music_title(); menu_sel = 0; screen = SC_TITLE;
        }
    }
}

void draw_pause()
{
    draw_play(false);
    menu_box(70, 56, 180, tr(i18n::S_PAUSE), PAUSE_ITEMS, 5, menu_sel);
}

constexpr int OPT_N = 5;

void update_options()
{
    nav(opt_sel, OPT_N);
    const int lr = (in.repeat(K_RIGHT) ? 1 : 0) - (in.repeat(K_LEFT) ? 1 : 0);
    const bool ok = in.pressed(K_A);
    switch (opt_sel) {
        case 0:
            if (lr || ok) {
                cfg.lang = (uint8_t)((cfg.lang + (lr ? lr : 1) + i18n::LANG_COUNT) % i18n::LANG_COUNT);
                i18n::set_lang(cfg.lang);
                sfx(audio::SFX_MENU);
            }
            break;
        case 1:
            if (lr) { cfg.music = (uint8_t)clampi(cfg.music + lr, 0, 10); apply_audio_settings(); sfx(audio::SFX_MENU); }
            break;
        case 2:
            if (lr) { cfg.sfx = (uint8_t)clampi(cfg.sfx + lr, 0, 10); apply_audio_settings(); sfx(audio::SFX_MENU); }
            break;
        case 3: if (lr || ok) { cfg.unlock_all ^= 1; sfx(audio::SFX_MENU); } break;
        default: if (ok) { save::save_config(cfg); sfx(audio::SFX_BACK); screen = options_from; return; }
    }
    if (in.pressed(K_B)) { save::save_config(cfg); sfx(audio::SFX_BACK); screen = options_from; }
}

void draw_slider(int x, int y, int v)
{
    for (int i = 0; i < 10; ++i) gfx::rect(x + i * 7, y + (i < v ? 0 : 3), 5, i < v ? 8 : 5, i < v ? P.yellow : P.dim);
}

void draw_options()
{
    if (options_from == SC_PAUSE) draw_play(false); else background();
    gfx::panel(20, 30, 280, OPT_N * 22 + 36, P.navy, P.border);
    gfx::text_center(38, tr(i18n::S_OPTIONS), P.yellow);
    const i18n::Str labels[OPT_N] = { i18n::S_OPT_LANGUAGE, i18n::S_OPT_MUSIC, i18n::S_OPT_SFX,
                                      i18n::S_OPT_UNLOCK, i18n::S_BACK };
    for (int i = 0; i < OPT_N; ++i) {
        const int y = 62 + i * 22;
        const bool sel = i == opt_sel;
        if (sel) gfx::rect(26, y - 4, 268, 16, P.navy2);
        gfx::text(32, y, tr(labels[i]), sel ? P.yellow : P.white);
        const int rx = 200;
        switch (i) {
            case 0: gfx::text(rx - 8, y, i18n::lang_name(cfg.lang), sel ? P.yellow : P.gray); break;
            case 1: draw_slider(rx, y, cfg.music); break;
            case 2: draw_slider(rx, y, cfg.sfx); break;
            case 3: gfx::text(rx, y, tr(cfg.unlock_all ? i18n::S_ON : i18n::S_OFF), sel ? P.yellow : P.gray); break;
            default: break;
        }
    }
    footer("< >  A  B");
}

void update_simple_back(Screen to)
{
    if (in.pressed(K_B) || in.pressed(K_A)) { sfx(audio::SFX_BACK); screen = to; }
}

void draw_howto()
{
    background();
    header(tr(i18n::S_HOWTO));
    const i18n::Str lines[9] = { i18n::S_HOW1, i18n::S_HOW2, i18n::S_HOW3, i18n::S_HOW4, i18n::S_HOW5, i18n::S_HOW6,
                                 i18n::S_HOW7, i18n::S_HOW8, i18n::S_HOW9 };
    for (int i = 0; i < 9; ++i) gfx::text(8, 32 + i * 16, tr(lines[i]), i < 2 ? P.yellow : P.white);
    // the parts, in a row
    const int ids[6] = { T_COIN, T_BOX, T_BOMB, T_BOXBOMB, T_BOXWALL, T_WALLB };
    for (int i = 0; i < 6; ++i) blit(ids[i], 100 + i * 20, 184);
    footer("A / B");
}

struct CreditPage { i18n::Str title; const char* lines[9]; };
const CreditPage CREDITS[] = {
    { i18n::S_CR_TITLE_GAME, { "Blips", "(c) 2024 Willems Davy", "joyrider3774", "MIT License", "",
                               "github.com/joyrider3774/blips", "", "Remake of Bips, Bips Gold and", "Bips Platinum by Bryant Brownell" } },
    { i18n::S_CR_TITLE_ART, { "Wall: 1001.com  CC BY-SA 3.0", "Floor, coin, player: Kenney  CC0", "Boxes: SpriteAttack  CC0",
                              "via opengameart.org / kenney.nl", "", "Dynamite, explosion, eraser:", "new art of the AKA port (MIT)",
                              "Sprites resized to 16 px", nullptr } },
    { i18n::S_CR_TITLE_SOUND, { "Music: donskeeto (title.mod)", "Move sound: Willems Davy", "Other sounds: new, made for",
                                "the AKA port (MIT)", "", "The paid sounds and the dynamite", "of the original are not used", nullptr, nullptr } },
    { i18n::S_CR_TITLE_LEVELS, { "Bryant Brownell", "Landon Brownell", "Caryn Brownell", "The PocoMan Team", "",
                                 "Level packs of the original game", "(Bips, Gold, Platinum, 2 players)", nullptr, nullptr } },
    { i18n::S_CR_TITLE_PORT, { "Gamebuino AKA port: Jicehel", "Gamebuino-AKA library (LGPL)", "  Jean-Marie Papillon",
                               "font8x8: Daniel Hepper (PD)", "", "Same code on PC: SDL2", "Source and licences in", "the project CREDITS.md",
                               nullptr } },
};
constexpr int N_CREDITS = (int)(sizeof CREDITS / sizeof CREDITS[0]);

void update_credits()
{
    if (in.repeat(K_RIGHT) || in.pressed(K_A)) { credits_page = (credits_page + 1) % N_CREDITS; sfx(audio::SFX_MENU); }
    if (in.repeat(K_LEFT)) { credits_page = (credits_page + N_CREDITS - 1) % N_CREDITS; sfx(audio::SFX_MENU); }
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_TITLE; }
}

void draw_credits()
{
    background();
    const CreditPage& p = CREDITS[credits_page];
    header(tr(p.title));
    for (int i = 0; i < 9 && p.lines[i]; ++i) gfx::text(16, 36 + i * 16, p.lines[i], p.lines[i][0] == ' ' ? P.gray : P.white);
    char pg[16];
    snprintf(pg, sizeof pg, tr(i18n::S_PAGE_FMT), credits_page + 1, N_CREDITS);
    gfx::text(gfx::W - 12 - gfx::text_width(pg), 8, pg, P.dim);
    footer("< >  B");
}

// ================================================================================ level editor
// Packs made here live in BLIPS/mylevels/<NAME>/levelN.lev (the shipped packs are never touched) and show
// up in the normal pack list. The drawing board scrolls over the 50 x 50 tile area of a level.
constexpr int ED_VIEW_COLS = 20, ED_VIEW_ROWS = 12, ED_Y0 = 16;

const int ED_PARTS[13] = { ID_EMPTY, ID_PLAYER, ID_BOX, ID_FLOOR, ID_BOMB, ID_WALL, ID_DIAMOND, ID_PLAYER2,
                           ID_BOX1, ID_BOX2, ID_BOXBOMB, ID_BOXWALL, ID_WALLBREAKABLE };
const i18n::Str ED_NAMES[13] = { i18n::S_P_ERASE, i18n::S_P_PLAYER, i18n::S_P_BOX, i18n::S_P_FLOOR, i18n::S_P_DYNAMITE,
                                 i18n::S_P_WALL, i18n::S_P_COIN, i18n::S_P_PLAYER2, i18n::S_P_BOX1, i18n::S_P_BOX2,
                                 i18n::S_P_BOXBOMB, i18n::S_P_BOXWALL, i18n::S_P_BREAKABLE };

char ed_dir[40] = "";             // folder of the pack being edited
char ed_title[48] = "";
int  ed_count = 0;                // levels in that pack
int  ed_sel = 0, ed_top = 0;      // level list
int  ed_idx = -1;                 // level being edited
bool ed_new_level = false;
int  ed_cx = 25, ed_cy = 25;      // cursor, in tiles
int  ed_camx = 15, ed_camy = 19;  // top-left tile of the view
int  ed_part = 3;                 // index in ED_PARTS
int  ed_pack_sel = 0, ed_pack_top = 0;
int  ed_menu_sel = 0;
uint32_t ed_delete_until = 0;
int  ed_packs[packs::MAX_PACKS];  // indexes in packs::list of the editable packs
int  ed_npacks = 0;
uint8_t ed_buf[MAX_PARTS * 3];    // the level while it is being tested
size_t  ed_len = 0;

char ed_nm[20] = "MYPACK1";
int  ed_nm_len = 7, ed_nm_pos = 0;
const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

void ed_build_pack_list()
{
    ed_npacks = 0;
    for (int i = 0; i < packs::count; ++i) if (packs::list[i].user) ed_packs[ed_npacks++] = i;
}

void ed_enter()
{
    packs::scan();
    ed_build_pack_list();
    ed_pack_sel = ed_pack_top = 0;
    screen = SC_ED_PACKS;
}

void ed_open_pack(const char* dir)
{
    snprintf(ed_dir, sizeof ed_dir, "%s", dir);
    packs::display_name(dir, ed_title, sizeof ed_title);
    packs::Pack tmp;
    memset(&tmp, 0, sizeof tmp);
    snprintf(tmp.dir, sizeof tmp.dir, "%s", dir);
    tmp.user = true;
    ed_count = packs::count_levels(tmp);
    ed_sel = ed_top = 0;
    screen = SC_ED_LEVELS;
}

void ed_follow_cursor()
{
    if (ed_cx < ed_camx + 1) ed_camx = ed_cx - 1;
    if (ed_cx > ed_camx + ED_VIEW_COLS - 2) ed_camx = ed_cx - ED_VIEW_COLS + 2;
    if (ed_cy < ed_camy + 1) ed_camy = ed_cy - 1;
    if (ed_cy > ed_camy + ED_VIEW_ROWS - 2) ed_camy = ed_cy - ED_VIEW_ROWS + 2;
    ed_camx = clampi(ed_camx, 0, N_COLS - ED_VIEW_COLS);
    ed_camy = clampi(ed_camy, 0, N_ROWS - ED_VIEW_ROWS);
}

void update_ed_packs()
{
    const int n = ed_npacks + 1;                      // first entry: new pack
    nav(ed_pack_sel, n);
    if (ed_pack_sel < ed_pack_top) ed_pack_top = ed_pack_sel;
    if (ed_pack_sel >= ed_pack_top + PACK_ROWS) ed_pack_top = ed_pack_sel - PACK_ROWS + 1;
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_TITLE; return; }
    if (in.pressed(K_A)) {
        sfx(audio::SFX_SELECT);
        if (ed_pack_sel == 0) {
            int n_try = 1;                            // first MYPACKn not used yet
            for (; n_try < 100; ++n_try) {
                snprintf(ed_nm, sizeof ed_nm, "MYPACK%d", n_try);
                bool used = false;
                for (int i = 0; i < packs::count; ++i) if (strcasecmp(packs::list[i].dir, ed_nm) == 0) used = true;
                if (!used) break;
            }
            ed_nm_len = (int)strlen(ed_nm);
            ed_nm_pos = ed_nm_len - 1;
            screen = SC_ED_NAME;
        } else {
            ed_open_pack(packs::list[ed_packs[ed_pack_sel - 1]].dir);
        }
    }
}

void draw_ed_packs()
{
    background();
    header(tr(i18n::S_ED_PACKS_TITLE));
    for (int r = 0; r < PACK_ROWS; ++r) {
        const int i = ed_pack_top + r;
        if (i > ed_npacks) break;
        const int y = 30 + r * 17;
        const bool sel = i == ed_pack_sel;
        if (sel) gfx::panel(6, y - 3, gfx::W - 12, 16, P.navy2, P.yellow);
        if (i == 0) {
            gfx::text(14, y + 1, "+", P.green);
            gfx::text(30, y + 1, tr(i18n::S_ED_NEW_PACK), sel ? P.yellow : P.green);
        } else {
            char nm[40];
            fit(packs::list[ed_packs[i - 1]].name, 30, nm, sizeof nm);
            gfx::text(14, y + 1, nm, sel ? P.yellow : P.white);
        }
    }
    footer(tr(i18n::S_HELP_MENU));
}

// ---- name of a new pack
void update_ed_name()
{
    if (in.repeat(K_UP) || in.repeat(K_DOWN)) {
        const int n = (int)strlen(NAME_CHARS);
        int k = 0;
        while (k < n && NAME_CHARS[k] != ed_nm[ed_nm_pos]) ++k;
        k = (k + (in.repeat(K_UP) ? 1 : n - 1)) % n;
        ed_nm[ed_nm_pos] = NAME_CHARS[k];
        sfx(audio::SFX_MENU);
    }
    if (in.repeat(K_RIGHT)) {
        if (ed_nm_pos < ed_nm_len - 1) ++ed_nm_pos;
        else if (ed_nm_len < 16) { ed_nm[ed_nm_len++] = 'A'; ed_nm[ed_nm_len] = 0; ed_nm_pos = ed_nm_len - 1; }
        sfx(audio::SFX_MENU);
    }
    if (in.repeat(K_LEFT) && ed_nm_pos > 0) { --ed_nm_pos; sfx(audio::SFX_MENU); }
    if (in.pressed(K_D) && ed_nm_len > 1) {
        ed_nm[--ed_nm_len] = 0;
        if (ed_nm_pos >= ed_nm_len) ed_nm_pos = ed_nm_len - 1;
        sfx(audio::SFX_BACK);
    }
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_ED_PACKS; return; }
    if (in.pressed(K_A)) {
        for (int i = 0; i < packs::count; ++i)
            if (strcasecmp(packs::list[i].dir, ed_nm) == 0) { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_ED_EXISTS)); return; }
        if (!packs::create_pack(ed_nm, "")) { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_ED_SAVE_FAILED)); return; }
        sfx(audio::SFX_SELECT);
        ed_open_pack(ed_nm);
    }
}

void draw_ed_name()
{
    background();
    header(tr(i18n::S_ED_NAME_TITLE));
    const int w = ed_nm_len * 16;
    const int x0 = (gfx::W - w) / 2;
    for (int i = 0; i < ed_nm_len; ++i) {
        const int x = x0 + i * 16;
        char ch[2] = { ed_nm[i], 0 };
        gfx::panel(x, 90, 14, 22, i == ed_nm_pos ? P.navy2 : P.navy, i == ed_nm_pos ? P.yellow : P.border);
        gfx::text(x + 3, 97, ch, i == ed_nm_pos ? P.yellow : P.white);
    }
    if ((t_now / 400) % 2 == 0) gfx::rect(x0 + ed_nm_pos * 16 + 1, 114, 12, 2, P.yellow);
    gfx::text_center(150, tr(i18n::S_ED_NAME_HELP), P.gray);
    gfx::text_center(166, tr(i18n::S_ED_NAME_HELP2), P.gray);
}

// ---- level list of the pack being edited
void update_ed_levels()
{
    const int cells = ed_count + (ed_count < packs::MAX_LEVELS ? 1 : 0);
    const int maxsel = cells - 1;
    if (in.repeat(K_LEFT) && ed_sel > 0) { --ed_sel; sfx(audio::SFX_MENU); }
    if (in.repeat(K_RIGHT) && ed_sel < maxsel) { ++ed_sel; sfx(audio::SFX_MENU); }
    if (in.repeat(K_UP) && ed_sel - LV_COLS >= 0) { ed_sel -= LV_COLS; sfx(audio::SFX_MENU); }
    if (in.repeat(K_DOWN)) { ed_sel = ed_sel + LV_COLS <= maxsel ? ed_sel + LV_COLS : maxsel; sfx(audio::SFX_MENU); }
    const int row = ed_sel / LV_COLS;
    if (row < ed_top) ed_top = row;
    if (row >= ed_top + LV_ROWS) ed_top = row - LV_ROWS + 1;
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); ed_enter(); return; }
    if (in.pressed(K_D) && ed_sel < ed_count) {        // delete: press twice
        if (t_now < ed_delete_until) {
            if (!packs::delete_level(ed_dir, ed_sel, ed_count)) show_toast(tr(i18n::S_ED_SAVE_FAILED));
            --ed_count;
            ed_delete_until = 0;
            if (ed_sel >= ed_count && ed_sel > 0) --ed_sel;
            pv_pack = pv_level = -1;
            sfx(audio::SFX_BACK);
        } else {
            ed_delete_until = t_now + 2500;
            show_toast(tr(i18n::S_ED_DELETE_CONFIRM));
            sfx(audio::SFX_ERROR);
        }
    }
    if (in.pressed(K_A)) {
        ed_idx = ed_sel;
        ed_new_level = ed_sel >= ed_count;
        if (ed_new_level) {
            ed_idx = ed_count;
            game.world.RemoveAll();
            ed_cx = 25; ed_cy = 25;
        } else {
            packs::Pack tmp;
            memset(&tmp, 0, sizeof tmp);
            snprintf(tmp.dir, sizeof tmp.dir, "%s", ed_dir);
            tmp.user = true;
            if (!packs::load_level(game.world, tmp, ed_idx)) { sfx(audio::SFX_ERROR); show_toast(tr(i18n::S_LOAD_ERROR)); return; }
            int minx = N_COLS, maxx = -1, miny = N_ROWS, maxy = -1;
            for (int i = 0; i < game.world.ItemCount; ++i) {
                const Part* p = game.world.Items[i];
                if (p->PlayFieldX < minx) minx = p->PlayFieldX;
                if (p->PlayFieldX > maxx) maxx = p->PlayFieldX;
                if (p->PlayFieldY < miny) miny = p->PlayFieldY;
                if (p->PlayFieldY > maxy) maxy = p->PlayFieldY;
            }
            ed_cx = maxx >= 0 ? (minx + maxx) / 2 : 25;
            ed_cy = maxy >= 0 ? (miny + maxy) / 2 : 25;
        }
        ed_camx = ed_cx - ED_VIEW_COLS / 2;
        ed_camy = ed_cy - ED_VIEW_ROWS / 2;
        ed_follow_cursor();
        sfx(audio::SFX_SELECT);
        screen = SC_ED_EDIT;
    }
}

void draw_ed_levels()
{
    background();
    char hd[64];
    fit(ed_title, 26, hd, sizeof hd);
    header(hd);
    const int cells = ed_count + (ed_count < packs::MAX_LEVELS ? 1 : 0);
    draw_level_grid(cells, ed_sel, ed_top, cells, ed_count < packs::MAX_LEVELS, false);
    gfx::panel(238, 30, 76, 117, P.navy, P.border);
    if (ed_sel < ed_count) {
        static char cached_dir[40];
        static int cached_level = -1;
        if (pv_pack != -2 || pv_level != ed_sel || strcmp(cached_dir, ed_dir) != 0 || cached_level != ed_sel) {
            char path[700];
            packs::user_level_path(ed_dir, ed_sel, path, sizeof path);
            load_preview_file(path);
            pv_pack = -2;
            pv_level = ed_sel;
            cached_level = ed_sel;
            snprintf(cached_dir, sizeof cached_dir, "%s", ed_dir);
        }
        draw_preview(240, 32, 72, 113);
    }
    char buf[64];
    snprintf(buf, sizeof buf, tr(i18n::S_LEVELS_FMT), ed_count);
    gfx::text(8, 158, buf, P.gray);
    if (ed_sel >= ed_count) gfx::text(8, 172, tr(i18n::S_ED_NEW_LEVEL), P.green);
    gfx::text(8, 186, tr(i18n::S_ED_LIST_HELP), P.dim);
    footer(tr(i18n::S_HELP_MENU));
}

// ---- test play
void ed_start_test()
{
    const int err = game.world.LevelError();
    if (err) { sfx(audio::SFX_ERROR); show_toast(tr(err == 1 ? i18n::S_ED_NO_PLAYER : i18n::S_ED_NO_COIN)); return; }
    ed_len = game.world.SaveBuffer(ed_buf, sizeof ed_buf);
    game.world.LoadBuffer(ed_buf, ed_len);
    setup_play();
    test_mode = true;
    sfx(audio::SFX_SELECT);
    screen = SC_PLAY;
}

void ed_restart_test()
{
    game.world.LoadBuffer(ed_buf, ed_len);
    setup_play();
    test_mode = true;
}

void ed_back_from_test()
{
    game.world.LoadBuffer(ed_buf, ed_len);
    test_mode = false;
    ps = PS_RUN;
    screen = SC_ED_EDIT;
}

// ---- the drawing board
void update_ed_edit()
{
    bool moved = false;
    if (in.repeat(K_LEFT) && ed_cx > 0) { --ed_cx; moved = true; }
    if (in.repeat(K_RIGHT) && ed_cx < N_COLS - 1) { ++ed_cx; moved = true; }
    if (in.repeat(K_UP) && ed_cy > 0) { --ed_cy; moved = true; }
    if (in.repeat(K_DOWN) && ed_cy < N_ROWS - 1) { ++ed_cy; moved = true; }
    if (moved) ed_follow_cursor();
    if (in.pressed(K_L1)) { ed_part = (ed_part + 12) % 13; sfx(audio::SFX_MENU); }
    if (in.pressed(K_R1)) { ed_part = (ed_part + 1) % 13; sfx(audio::SFX_MENU); }
    if (in.pressed(K_A) || (moved && in.held(K_A))) game.world.EditPlace(ED_PARTS[ed_part], ed_cx, ed_cy);   // hold A and move to paint
    if (in.pressed(K_C) || (moved && in.held(K_C))) game.world.EditPlace(ID_EMPTY, ed_cx, ed_cy);
    if (in.pressed(K_D)) ed_start_test();
    if (in.pressed(K_B)) { ed_menu_sel = 0; sfx(audio::SFX_MENU); screen = SC_ED_MENU; }
}

void draw_ed_edit()
{
    background();
    gfx::rect(0, 0, gfx::W, ED_Y0, P.navy);
    gfx::rect(0, ED_Y0 - 1, gfx::W, 1, P.border);
    gfx::text_center(4, tr(i18n::S_ED_HELP), P.gray);

    const int mw = ED_VIEW_COLS * TS, mh = ED_VIEW_ROWS * TS;
    gfx::rect(0, ED_Y0, mw, mh, P.cell);
    for (int i = 1; i < ED_VIEW_COLS; ++i) gfx::rect(i * TS, ED_Y0, 1, mh, P.grid);
    for (int j = 1; j < ED_VIEW_ROWS; ++j) gfx::rect(0, ED_Y0 + j * TS, mw, 1, P.grid);
    draw_world(game.world, ed_camx * TS, ed_camy * TS, ED_Y0);
    // the edges of the 50 x 50 area
    if (ed_camx + ED_VIEW_COLS >= N_COLS) gfx::rect((N_COLS - ed_camx) * TS - 1, ED_Y0, 1, mh, P.red);
    if (ed_camy + ED_VIEW_ROWS >= N_ROWS) gfx::rect(0, ED_Y0 + (N_ROWS - ed_camy) * TS - 1, mw, 1, P.red);
    // cursor (blinks) with the selected part ghosted
    const int cx = (ed_cx - ed_camx) * TS, cy = ED_Y0 + (ed_cy - ed_camy) * TS;
    if ((t_now / 250) % 2 == 0) gfx::frame(cx - 1, cy - 1, TS + 2, TS + 2, P.yellow);
    else gfx::frame(cx, cy, TS, TS, P.white);

    // part palette
    const int py = ED_Y0 + mh + 2;
    for (int i = 0; i < 13; ++i) {
        const int x = 4 + i * 24;
        gfx::panel(x, py, 22, 20, i == ed_part ? P.navy2 : P.navy, i == ed_part ? P.yellow : P.border);
        int t = tile_for(ED_PARTS[i], 0);
        if (ED_PARTS[i] == ID_PLAYER) t = T_PLAYER + 12;
        if (ED_PARTS[i] == ID_PLAYER2) t = T_PLAYER2 + 12;
        if (t >= 0) blit(t, x + 3, py + 2);
    }
    char buf[64];
    snprintf(buf, sizeof buf, "%s  %d,%d  #%d", tr(ED_NAMES[ed_part]), ed_cx + 1, ed_cy + 1, ed_idx + 1);
    gfx::text(6, gfx::H - 10, buf, P.yellow);
    snprintf(buf, sizeof buf, "%s %d", tr(i18n::S_COINS), game.world.Count(ID_DIAMOND));
    gfx::text(gfx::W - gfx::text_width(buf) - 6, gfx::H - 10, buf, P.gray);
}

// ---- menu of the drawing board
const i18n::Str ED_MENU_ITEMS[6] = { i18n::S_RESUME, i18n::S_ED_TEST, i18n::S_ED_CENTER, i18n::S_ED_SAVE,
                                     i18n::S_ED_DISCARD, i18n::S_ED_CLEAR };

void update_ed_menu()
{
    nav(ed_menu_sel, 6);
    if (in.pressed(K_B)) { sfx(audio::SFX_BACK); screen = SC_ED_EDIT; return; }
    if (!in.pressed(K_A)) return;
    switch (ed_menu_sel) {
        case 0: screen = SC_ED_EDIT; break;
        case 1: screen = SC_ED_EDIT; ed_start_test(); break;
        case 2:
            if (game.world.CenterLevel()) sfx(audio::SFX_SELECT);
            else sfx(audio::SFX_MENU);
            screen = SC_ED_EDIT;
            break;
        case 3: {
            const int err = game.world.LevelError();
            if (err) { sfx(audio::SFX_ERROR); show_toast(tr(err == 1 ? i18n::S_ED_NO_PLAYER : i18n::S_ED_NO_COIN)); break; }
            if (packs::save_level(ed_dir, ed_idx, game.world)) {
                if (ed_new_level) ++ed_count;
                sfx(audio::SFX_STAGEEND);
                show_toast(tr(i18n::S_ED_SAVED));
                ed_sel = ed_idx;
                pv_pack = pv_level = -1;
                screen = SC_ED_LEVELS;
            } else {
                sfx(audio::SFX_ERROR);
                show_toast(tr(i18n::S_ED_SAVE_FAILED));
            }
            break;
        }
        case 4:
            ed_sel = ed_idx;
            sfx(audio::SFX_BACK);
            screen = SC_ED_LEVELS;
            break;
        default:
            game.world.RemoveAll();
            sfx(audio::SFX_SELECT);
            screen = SC_ED_EDIT;
    }
}

void draw_ed_menu()
{
    draw_ed_edit();
    menu_box(50, 44, 220, tr(i18n::S_EDITOR), ED_MENU_ITEMS, 6, ed_menu_sel);
}

// ================================================================================ system
uint32_t menu_down_at = 0, combo_at = 0;
bool menu_tainted = false, combo_armed = false;

void system_keys()
{
    // RUN + MENU held for 500 ms returns to the launcher (on release, so the launcher does not see the keys).
    if (in.held(K_RUN) && in.held(K_MENU)) {
        if (!combo_at) combo_at = t_now;
        if (t_now - combo_at >= 500) combo_armed = true;
        menu_tainted = true;
    } else {
        combo_at = 0;
        if (combo_armed && !in.held(K_RUN) && !in.held(K_MENU)) {
            save::save_config(cfg);
            plat::return_to_loader();
        }
        if (!in.held(K_RUN) && !in.held(K_MENU)) combo_armed = false;
    }
    if (in.pressed(K_MENU)) { menu_down_at = t_now; menu_tainted = in.held(K_RUN); }
    if (in.released(K_MENU) && !menu_tainted) {
        const uint32_t held = t_now - menu_down_at;
        if (held >= 1000) {                       // long press: screenshot (taken from the last frame drawn)
            char name[24];
            if (gfx::screenshot(name, sizeof name)) {
                char msg[48];
                snprintf(msg, sizeof msg, "%s %s", tr(i18n::S_SHOT_SAVED), name);
                show_toast(msg);
            } else {
                show_toast(tr(i18n::S_SHOT_FAILED));
            }
        } else if (held < 600) {
            if (screen == SC_PLAY && test_mode) { sfx(audio::SFX_BACK); ed_back_from_test(); }
            else if (screen == SC_PLAY && ps == PS_RUN) { menu_sel = 0; screen = SC_PAUSE; sfx(audio::SFX_MENU); }
            else if (screen == SC_PAUSE) { screen = SC_PLAY; sfx(audio::SFX_BACK); }
        }
    }
    if (!in.held(K_MENU)) menu_tainted = false;
}

void draw_toast()
{
    if (t_now >= toast_until || !toast[0]) return;
    const int w = gfx::text_width(toast) + 12;
    gfx::panel((gfx::W - w) / 2, gfx::H - 40, w, 16, P.navy, P.yellow);
    gfx::text_center(gfx::H - 36, toast, P.white);
}

}  // namespace

// ================================================================================ main loop
void run()
{
    g_core.init();
#ifndef AKA_PC
    g_core.joystick.calibrate_center();            // stick assumed at rest
#else
    lcd_set_fps(30);                               // the world is stepped once per frame: 30 Hz like the console
#endif
    init_palette();
    if (save::load_config(cfg)) i18n::set_lang(cfg.lang);
    else i18n::set_lang(cfg.lang = i18n::FR);
    audio::init();
    apply_audio_settings();
    packs::scan();
    game.world.sound_cb = on_sound;
    audio::music_title();
    printf("[Blips] data: %s  packs: %d\n", plat::data_dir(), packs::count);

    uint32_t next_frame = g_core.get_millis();
    for (;;) {
        g_core.pool();
        t_now = g_core.get_millis();
        in.poll(t_now);
        system_keys();

        switch (screen) {
            case SC_TITLE:   update_title(); break;
            case SC_PACKS:   update_packs(); break;
            case SC_LEVELS:  update_levels(); break;
            case SC_PLAY:    update_play(); break;
            case SC_PAUSE:   update_pause(); break;
            case SC_OPTIONS: update_options(); break;
            case SC_CREDITS: update_credits(); break;
            case SC_HOWTO:   update_simple_back(SC_TITLE); break;
            case SC_ED_PACKS:  update_ed_packs(); break;
            case SC_ED_NAME:   update_ed_name(); break;
            case SC_ED_LEVELS: update_ed_levels(); break;
            case SC_ED_EDIT:   update_ed_edit(); break;
            case SC_ED_MENU:   update_ed_menu(); break;
        }
        switch (screen) {
            case SC_TITLE:   draw_title(); break;
            case SC_PACKS:   draw_packs(); break;
            case SC_LEVELS:  draw_levels(); break;
            case SC_PLAY:    draw_play(ps == PS_RUN || ps == PS_DONE_WAIT); break;
            case SC_PAUSE:   draw_pause(); break;
            case SC_OPTIONS: draw_options(); break;
            case SC_CREDITS: draw_credits(); break;
            case SC_HOWTO:   draw_howto(); break;
            case SC_ED_PACKS:  draw_ed_packs(); break;
            case SC_ED_NAME:   draw_ed_name(); break;
            case SC_ED_LEVELS: draw_ed_levels(); break;
            case SC_ED_EDIT:   draw_ed_edit(); break;
            case SC_ED_MENU:   draw_ed_menu(); break;
        }
        draw_toast();
        gfx::present();
        audio::tick();

#ifndef AKA_PC
        next_frame += 33;                          // 30 fps on the console; the PC backend paces the LCD itself
        const uint32_t n = g_core.get_millis();
        if ((int32_t)(next_frame - n) > 0) gb_delay_ms(next_frame - n); else next_frame = n;
#else
        (void)next_frame;
#endif
    }
}

}  // namespace app
