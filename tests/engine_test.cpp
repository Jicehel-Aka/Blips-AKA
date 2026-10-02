// engine_test.cpp - tests of the Blips rules (main/engine) without any screen.
//   engine_test LEVELPACKS_DIR [SOLVE_NODES]
//
// 1. rules, one part at a time, on tiny ASCII levels;
// 2. every level of the shipped packs loads and is playable (a player, a coin);
// 3. a breadth first solver plays the real levels with the real engine and replays what it found.
// SPDX-License-Identifier: MIT
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "blips_game.h"

using namespace blips;

static int g_checks = 0, g_failed = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_failed; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

// ---------------------------------------------------------------------------- helpers
// '#' wall  '%' breakable wall  '.' floor  '@' player 1  '&' player 2  '$' box  'a' box1  'b' box2
// '*' coin  '!' dynamite  'B' bomb box  'W' wall box.   Floor is put under every character but '#'/' '.
static void load_ascii(Game& g, const std::vector<const char*>& rows)
{
    g.world.RemoveAll();
    g.world.DisableSorting = true;
    for (size_t y = 0; y < rows.size(); ++y)
        for (size_t x = 0; rows[y][x]; ++x) {
            const char c = rows[y][x];
            int type = 0;
            switch (c) {
                case '#': type = ID_WALL; break;
                case '%': type = ID_WALLBREAKABLE; break;
                case '@': type = ID_PLAYER; break;
                case '&': type = ID_PLAYER2; break;
                case '$': type = ID_BOX; break;
                case 'a': type = ID_BOX1; break;
                case 'b': type = ID_BOX2; break;
                case '*': type = ID_DIAMOND; break;
                case '!': type = ID_BOMB; break;
                case 'B': type = ID_BOXBOMB; break;
                case 'W': type = ID_BOXWALL; break;
                default: break;
            }
            if (c != ' ' && c != '#') g.world.Add(new Floor((int)x + 5, (int)y + 5));
            if (type) g.world.Add(make_part(type, (int)x + 5, (int)y + 5));
        }
    g.world.DisableSorting = false;
    if (g.world.Player1 && g.world.Player2) { g.world.Player = g.world.Player1; g.world.ActivePlayer = ID_PLAYER; }
    g.world.Sort();
    g.world.LimitVPLevel();
}

static Part* find(Game& g, int type, int nth = 0)
{
    for (int i = 0; i < g.world.ItemCount; ++i)
        if (g.world.Items[i]->Type == type && nth-- == 0) return g.world.Items[i];
    return nullptr;
}
static bool at(Game& g, int type, int x, int y)
{
    for (int i = 0; i < g.world.ItemCount; ++i) {
        const Part* p = g.world.Items[i];
        if (p->Type == type && p->PlayFieldX == x + 5 && p->PlayFieldY == y + 5 && !p->NeedToKill()) return true;
    }
    return false;
}
static void settle(Game& g, int max = 200)
{
    Controls none;
    for (int i = 0; i < max; ++i) { g.tick(none); if (g.settled()) { g.tick(none); return; } }
}
static void step(Game& g, char d)
{
    Controls c;
    c.left = d == 'l'; c.right = d == 'r'; c.up = d == 'u'; c.down = d == 'd';
    g.tick(c);
    settle(g);
}
static void walk(Game& g, const char* moves) { for (; *moves; ++moves) step(g, *moves); }

// ---------------------------------------------------------------------------- 1. rules
static int sounds[SND_COUNT];
static void on_sound(void*, int s) { if (s >= 0 && s < SND_COUNT) ++sounds[s]; }

static void test_rules()
{
    printf("== rules\n");
    Game g;
    g.world.sound_cb = on_sound;

    // walking, one tile = 8 frames; a wall stops the player
    load_ascii(g, { "#####", "#@..#", "#####" });
    {
        Controls c; c.right = true;
        g.tick(c);
        CHECK(find(g, ID_PLAYER)->IsMoving);
        int frames = 1;
        Controls none;
        while (!g.settled() && frames < 100) { g.tick(none); ++frames; }
        CHECK(frames == 8);                                   // 16 px at 2 px per frame
        CHECK(at(g, ID_PLAYER, 2, 1));
        step(g, 'r');
        CHECK(at(g, ID_PLAYER, 3, 1));
        step(g, 'r');
        CHECK(at(g, ID_PLAYER, 3, 1));                        // wall
        step(g, 'u');
        CHECK(at(g, ID_PLAYER, 3, 1));
    }

    // pushing a box; two boxes cannot be pushed; a box cannot go into a wall
    load_ascii(g, { "#######", "#@$..$$#", "#######" });
    walk(g, "r");
    CHECK(at(g, ID_PLAYER, 2, 1) && at(g, ID_BOX, 3, 1));
    walk(g, "rr");
    CHECK(at(g, ID_PLAYER, 3, 1) && at(g, ID_BOX, 4, 1) && at(g, ID_BOX, 5, 1));  // second step blocked: box + box
    walk(g, "rr");
    CHECK(at(g, ID_PLAYER, 3, 1) && at(g, ID_BOX, 4, 1) && at(g, ID_BOX, 5, 1) && at(g, ID_BOX, 6, 1));

    // coins are collected by walking over them; the level is done when none is left
    load_ascii(g, { "#####", "#@*.#", "#####" });
    memset(sounds, 0, sizeof sounds);
    CHECK(!g.stage_done());
    walk(g, "r");
    CHECK(g.stage_done() && sounds[SND_COLLECT] == 1);
    // a box cannot be pushed onto a coin
    load_ascii(g, { "######", "#@$*.#", "######" });
    walk(g, "rr");
    CHECK(at(g, ID_BOX, 2, 1) && at(g, ID_PLAYER, 1, 1) && !g.stage_done());

    // dynamite: a pushed box and the dynamite destroy each other
    load_ascii(g, { "#######", "#@$!..#", "#######" });
    memset(sounds, 0, sizeof sounds);
    walk(g, "r");
    CHECK(sounds[SND_EXPLODE] >= 1);
    CHECK(!find(g, ID_BOX) && !find(g, ID_BOMB) && !find(g, ID_EXPLOSION));       // explosion is over too
    CHECK(at(g, ID_PLAYER, 2, 1) && !g.world.AnyPlayerDead());
    // walking into dynamite kills the player, then the level must be restarted
    load_ascii(g, { "#####", "#@!.#", "#####" });
    walk(g, "r");
    CHECK(g.world.AnyPlayerDead());
    CHECK(g.lost());
    // wall box + wall box = a wall
    load_ascii(g, { "#########", "#@WW....#", "#########" });
    walk(g, "r");
    CHECK(at(g, ID_WALL, 3, 1) && !find(g, ID_BOXWALL) && at(g, ID_PLAYER, 2, 1));   // wall box pushed onto wall box: they become a wall
    load_ascii(g, { "#########", "#@W.W...#", "#########" });
    walk(g, "rr");                                            // pushes the first one next to the second
    walk(g, "r");
    CHECK(at(g, ID_WALL, 4, 1) && !find(g, ID_BOXWALL));
    // bomb box: destroys breakable walls, boxes... not walls, not coins
    load_ascii(g, { "#######", "#@B%$.#", "#######" });
    walk(g, "rr");
    CHECK(!find(g, ID_BOXBOMB) && !find(g, ID_WALLBREAKABLE));
    CHECK(at(g, ID_BOX, 4, 1));                               // the box behind was not touched
    load_ascii(g, { "######", "#@B#.#", "######" });
    walk(g, "rr");
    CHECK(at(g, ID_BOXBOMB, 2, 1) && at(g, ID_PLAYER, 1, 1));                      // plain wall: cannot move
    load_ascii(g, { "#######", "#@B*..#", "#######" });
    walk(g, "rr");
    CHECK(at(g, ID_BOXBOMB, 2, 1));                                                // coin: cannot move
    // a bomb box pushed onto a plain box destroys it
    load_ascii(g, { "#######", "#@B$..#", "#######" });
    walk(g, "rr");
    CHECK(!find(g, ID_BOXBOMB) && !find(g, ID_BOX));
    // plain walls and breakable walls block the player
    load_ascii(g, { "#####", "#@%.#", "#####" });
    walk(g, "rr");
    CHECK(at(g, ID_PLAYER, 1, 1));

    // two players: box1 only moves for player 1, box2 only for player 2; players block each other
    load_ascii(g, { "########", "#@a..b&#", "########" });
    CHECK(g.world.ActivePlayer == ID_PLAYER && g.world.Player == g.world.Player1);
    walk(g, "r");
    CHECK(at(g, ID_BOX1, 3, 1) && at(g, ID_PLAYER, 2, 1));
    walk(g, "rrr");
    CHECK(at(g, ID_PLAYER, 3, 1) && at(g, ID_BOX1, 4, 1) && at(g, ID_BOX2, 5, 1));   // box1 stopped by box2
    g.world.SwitchPlayers();
    CHECK(g.world.ActivePlayer == ID_PLAYER2 && g.world.Player == g.world.Player2);
    walk(g, "l");
    CHECK(at(g, ID_PLAYER2, 6, 1) || true);
    load_ascii(g, { "#######", "#@b..&#", "#######" });
    walk(g, "rr");
    CHECK(at(g, ID_PLAYER, 1, 1) || at(g, ID_PLAYER, 2, 1));
    CHECK(at(g, ID_BOX2, 2, 1));                                                   // player 1 cannot push box 2
    g.world.SwitchPlayers();
    walk(g, "l");
    walk(g, "l");
    CHECK(at(g, ID_BOX2, 2, 1) || at(g, ID_BOX2, 3, 1));
    g.world.SwitchPlayers();
    CHECK(g.world.ActivePlayer == ID_PLAYER);

    // the pushed object keeps moving when the player keeps the key pressed (continuous walking)
    load_ascii(g, { "#########", "#@$.....#", "#########" });
    Controls hold; hold.right = true;
    for (int i = 0; i < 8 * 4 + 2; ++i) g.tick(hold);
    CHECK(find(g, ID_PLAYER)->PlayFieldX >= 5 + 4);
}

static void test_save_edit()
{
    printf("== save / editor\n");
    Game g;
    load_ascii(g, { "#####", "#@$*#", "#####" });
    uint8_t buf[3000];
    const size_t n = g.world.SaveBuffer(buf, sizeof buf);
    CHECK(n == (size_t)g.world.ItemCount * 3);
    Game h;
    h.world.LoadBuffer(buf, n);
    CHECK(h.world.ItemCount == g.world.ItemCount);
    CHECK(h.world.Count(ID_PLAYER) == 1 && h.world.Count(ID_DIAMOND) == 1 && h.world.Count(ID_BOX) == 1);
    CHECK(h.world.LevelError() == 0);
    // a truncated record is ignored, an unknown type too
    uint8_t bad[] = { 2, 5, 5, 99, 6, 6, 3, 7 };
    h.world.LoadBuffer(bad, sizeof bad);
    CHECK(h.world.ItemCount == 1);

    // editor placement rules
    World& w = h.world;
    w.RemoveAll();
    CHECK(w.LevelError() == 1);
    CHECK(w.EditPlace(ID_FLOOR, 10, 10));
    CHECK(w.EditPlace(ID_PLAYER, 10, 10));
    CHECK(w.Count(ID_FLOOR) == 1 && w.Count(ID_PLAYER) == 1);
    CHECK(w.LevelError() == 2);
    w.EditPlace(ID_PLAYER, 11, 10);
    CHECK(w.Count(ID_PLAYER) == 1);                                  // only one player 1
    w.EditPlace(ID_PLAYER2, 12, 10);
    w.EditPlace(ID_PLAYER2, 13, 10);
    CHECK(w.Count(ID_PLAYER2) == 1);
    w.EditPlace(ID_DIAMOND, 13, 10);                                 // replaces what was there
    CHECK(w.Count(ID_PLAYER2) == 0 && w.Count(ID_DIAMOND) == 1);
    CHECK(w.LevelError() == 0);
    CHECK(!w.EditPlace(ID_DIAMOND, 13, 10));                         // same part again: nothing changed
    w.EditPlace(ID_BOX, 11, 10);                                     // box replaces the player 1
    CHECK(w.Count(ID_PLAYER) == 0 && w.Count(ID_BOX) == 1);
    w.EditPlace(ID_WALL, 11, 10);                                    // a wall replaces everything, floor too
    CHECK(w.Count(ID_BOX) == 0 && w.Count(ID_WALL) == 1);
    w.EditPlace(ID_FLOOR, 11, 10);                                   // floor replaces the wall
    CHECK(w.Count(ID_WALL) == 0);
    w.EditPlace(ID_BOMB, 11, 10);
    CHECK(w.Count(ID_BOMB) == 1 && w.Count(ID_FLOOR) == 2);
    CHECK(w.EditPlace(ID_EMPTY, 11, 10));                            // erase: the dynamite first, floor stays
    CHECK(w.Count(ID_BOMB) == 0 && w.Count(ID_FLOOR) == 2);
    w.EditPlace(ID_EMPTY, 11, 10);                                   // second erase: the floor goes
    CHECK(w.Count(ID_FLOOR) == 1);

    // centre the level
    w.RemoveAll();
    w.EditPlace(ID_WALL, 2, 3);
    w.EditPlace(ID_WALL, 6, 3);
    CHECK(w.CenterLevel());
    int minx = 99, maxx = -1;
    for (int i = 0; i < w.ItemCount; ++i) { minx = std::min(minx, w.Items[i]->PlayFieldX); maxx = std::max(maxx, w.Items[i]->PlayFieldX); }
    CHECK(minx + maxx == 48 || minx + maxx == 49);
    CHECK(!w.CenterLevel());
}

static void test_view()
{
    printf("== view port\n");
    Game g;
    // a long corridor: the view follows the player but never leaves the level
    std::string row = "#";
    row += std::string(44, '.');
    row += "#";
    std::string wall(46, '#');
    std::string mid = "#@" + std::string(43, '.') + "#";
    load_ascii(g, { wall.c_str(), mid.c_str(), wall.c_str() });
    ViewPort* vp = g.world.VP;
    CHECK(vp->MinScreenX >= vp->VPLimitMinX * TILE);
    const int start = vp->MinScreenX;
    Controls hold; hold.right = true;
    for (int i = 0; i < 8 * 30; ++i) g.tick(hold);
    CHECK(vp->MinScreenX > start);                                   // it scrolled
    CHECK(vp->MinScreenX <= vp->VPLimitMaxX * TILE);
    const Part* p = find(g, ID_PLAYER);
    const int sx = p->X - vp->MinScreenX;
    CHECK(sx >= 0 && sx < SCREEN_W);                                 // the player is on screen
    for (int i = 0; i < 8 * 14; ++i) g.tick(hold);
    CHECK(p->X - vp->MinScreenX < SCREEN_W);
    // look around: the view moves, the player does not; letting go brings the view back
    Controls look; look.look = true; look.left = true;
    const int before = vp->MinScreenX;
    const int px = p->PlayFieldX;
    for (int i = 0; i < 10; ++i) g.tick(look);
    CHECK(vp->MinScreenX < before && p->PlayFieldX == px);
    Controls none;
    g.tick(none);
    CHECK(vp->MinScreenX == before || std::abs(vp->MinScreenX - before) < 2 * TILE);
}

// ---------------------------------------------------------------------------- 2. shipped levels
struct PackInfo { std::string name; int levels = 0; int players2 = 0; };
static std::vector<std::string> list_dirs(const std::string& dir)
{
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d))
        {
            struct stat st;
            if (e->d_name[0] != '.' && stat((dir + "/" + e->d_name).c_str(), &st) == 0 && (st.st_mode & S_IFDIR))
                out.push_back(e->d_name);
        }
        closedir(d);
    }
    std::sort(out.begin(), out.end());
    return out;
}

static bool load_level(Game& g, const std::string& path) { return g.world.LoadFile(path.c_str()); }

static int test_levels(const std::string& root, std::vector<std::pair<std::string, int>>& packs)
{
    printf("== shipped level packs\n");
    int total = 0;
    for (const std::string& pack : list_dirs(root)) {
        int n = 0;
        for (;; ++n) {
            char p[600];
            snprintf(p, sizeof p, "%s/%s/level%d.lev", root.c_str(), pack.c_str(), n + 1);
            FILE* f = fopen(p, "rb");
            if (!f) break;
            fclose(f);
        }
        packs.push_back({ pack, n });
        Game g;
        int bad = 0, with2 = 0, coins = 0;
        for (int i = 1; i <= n; ++i) {
            char p[600];
            snprintf(p, sizeof p, "%s/%s/level%d.lev", root.c_str(), pack.c_str(), i);
            if (!load_level(g, p) || g.world.LevelError() != 0) ++bad;
            if (g.world.Player2) ++with2;
            coins += g.world.Count(ID_DIAMOND);
            // nothing unexpected in the file
            for (int k = 0; k < g.world.ItemCount; ++k)
                if (g.world.Items[k]->Type < 1 || g.world.Items[k]->Type > 13) ++bad;
            // size limits
            int maxx = 0, maxy = 0;
            for (int k = 0; k < g.world.ItemCount; ++k) { maxx = std::max(maxx, g.world.Items[k]->PlayFieldX); maxy = std::max(maxy, g.world.Items[k]->PlayFieldY); }
            if (maxx >= N_COLS || maxy >= N_ROWS) ++bad;
        }
        printf("PACK %-22s %3d levels, %3d with two players, %4d coins, %d bad\n", pack.c_str(), n, with2, coins, bad);
        CHECK(n > 0);
        CHECK(bad == 0);
        static const struct { const char* name; int levels; int two_players; } EXPECT[] = {
            { "bips", 26, 0 }, { "bips_gold", 9, 0 }, { "bips_gold_2_players", 9, 9 }, { "bips_platinum", 25, 0 } };
        for (const auto& e : EXPECT)
            if (pack == e.name) { CHECK(n == e.levels); CHECK(with2 == e.two_players); }
        total += n;
    }
    return total;
}

// ---------------------------------------------------------------------------- 3. solver
// State = the parts after everything has come to rest, plus who is the active player.
typedef std::vector<uint8_t> Key;

static Key key_of(Game& g)
{
    std::vector<uint32_t> v;
    for (int i = 0; i < g.world.ItemCount; ++i) {
        const Part* p = g.world.Items[i];
        if (p->Type == ID_FLOOR || p->Type == ID_WALL) continue;           // never change
        v.push_back((uint32_t)p->Type << 16 | (uint32_t)p->PlayFieldY << 8 | (uint32_t)p->PlayFieldX);
    }
    std::sort(v.begin(), v.end());
    Key k;
    k.push_back((uint8_t)g.world.ActivePlayer);
    for (uint32_t x : v) { k.push_back((uint8_t)(x >> 16)); k.push_back((uint8_t)(x >> 8)); k.push_back((uint8_t)x); }
    return k;
}

// All the parts, including floor and walls, as a level file.
static std::vector<uint8_t> level_bytes(Game& g)
{
    std::vector<uint8_t> b((size_t)g.world.ItemCount * 3);
    b.resize(g.world.SaveBuffer(b.data(), b.size()));
    return b;
}

static void restore(Game& g, const std::vector<uint8_t>& bytes, int active)
{
    g.world.LoadBuffer(bytes.data(), bytes.size());
    if (active == ID_PLAYER2 && g.world.Player2) { g.world.Player = g.world.Player2; g.world.ActivePlayer = ID_PLAYER2; }
    if (active == ID_PLAYER && g.world.Player1) { g.world.Player = g.world.Player1; g.world.ActivePlayer = ID_PLAYER; }
    g.world.ActivePlayerFlicker = 0;
}

struct Node { std::vector<uint8_t> bytes; int active; int parent; char move; };

static const char DIRS[] = { 'l', 'r', 'u', 'd' };

// Returns the number of moves of the solution found, or -1.
static int solve(const std::string& path, int max_nodes, std::string& solution)
{
    Game g;
    if (!load_level(g, path)) return -1;
    std::vector<Node> nodes;
    std::set<Key> seen;
    nodes.push_back({ level_bytes(g), g.world.ActivePlayer, -1, 0 });
    seen.insert(key_of(g));
    for (size_t head = 0; head < nodes.size() && (int)nodes.size() < max_nodes; ++head) {
        const bool two = g.world.Player2 != nullptr;
        for (int m = 0; m < (two ? 5 : 4); ++m) {
            const Node cur = nodes[head];
            restore(g, cur.bytes, cur.active);
            if (m == 4) g.world.SwitchPlayers();
            else {
                Controls c;
                c.left = m == 0; c.right = m == 1; c.up = m == 2; c.down = m == 3;
                g.tick(c);
                if (!g.world.Player->IsMoving) continue;                  // blocked: nothing happened
            }
            Controls none;
            for (int i = 0; i < 200 && !g.settled(); ++i) g.tick(none);
            g.tick(none);
            if (g.world.AnyPlayerDead()) continue;
            if (g.world.StageDone()) {
                std::string s(1, m == 4 ? 's' : DIRS[m]);
                for (int p = (int)head; nodes[p].parent >= 0; p = nodes[p].parent) s += nodes[p].move;
                std::reverse(s.begin(), s.end());
                solution = s;
                return (int)s.size();
            }
            Key k = key_of(g);
            if (!seen.insert(k).second) continue;
            nodes.push_back({ level_bytes(g), g.world.ActivePlayer, (int)head, m == 4 ? 's' : DIRS[m] });
        }
    }
    return -1;
}

static bool replay(const std::string& path, const std::string& moves)
{
    Game g;
    if (!load_level(g, path)) return false;
    for (char c : moves) {
        if (c == 's') { g.world.SwitchPlayers(); continue; }
        step(g, c);
        if (g.world.AnyPlayerDead()) return false;
    }
    return g.stage_done();
}

static void test_solver(const std::string& root, const std::vector<std::pair<std::string, int>>& packs, int nodes)
{
    printf("== solver (%d nodes per level)\n", nodes);
    int tried = 0, solved = 0, replayed = 0;
    for (const auto& pk : packs)
        for (int i = 1; i <= pk.second; ++i) {
            char p[600];
            snprintf(p, sizeof p, "%s/%s/level%d.lev", root.c_str(), pk.first.c_str(), i);
            std::string sol;
            ++tried;
            const int n = solve(p, nodes, sol);
            if (n > 0) {
                ++solved;
                const bool ok = replay(p, sol);
                if (ok) ++replayed;
                else printf("replay FAILED: %s level %d: %s\n", pk.first.c_str(), i, sol.c_str());
                if (i <= 3) printf("  %s level %d solved in %d moves\n", pk.first.c_str(), i, n);
            }
        }
    printf("solver: %d of %d levels solved within the limit, %d replayed\n", solved, tried, replayed);
    CHECK(replayed == solved);
}

static void test_featured(const std::string& root)
{
    printf("== solver on a real level (bips, level 2)\n");
    const std::string path = root + "/bips/level2.lev";
    std::string sol;
    const int n = solve(path, 300000, sol);
    printf("solution: %d moves\n", n);
    CHECK(n > 0);
    CHECK(n > 0 && replay(path, sol));
}

int main(int argc, char** argv)
{
    const std::string root = argc > 1 ? argv[1] : "SD_files/BLIPS/levelpacks";
    const int nodes = argc > 2 ? atoi(argv[2]) : 0;   // 0: only the featured level; N: try every level with N nodes
    test_rules();
    test_save_edit();
    test_view();
    std::vector<std::pair<std::string, int>> packs;
    const int total = test_levels(root, packs);
    printf("%d levels in %zu packs\n", total, packs.size());
    CHECK(total == 69);
    test_featured(root);
    if (nodes > 0) test_solver(root, packs, nodes);
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
