/*
 * blips_world.h - the rules of Blips: parts, movement, explosions, view port, level files.
 *
 * Hardware independent (no SDL, no gamebuino header): the same file is used by the console build,
 * the PC build and the tests.
 *
 * Derived from "Blips" by Willems Davy (joyrider3774), MIT License,
 * https://github.com/joyrider3774/blips (files CWorldPart.*, CPlayer.*, CWorldParts.*, CViewPort.*).
 * The game logic keeps the structure and the behaviour of the original on purpose, so that levels
 * play exactly the same; the changes are: no SDL, 16 pixel tiles on a 320x240 screen, a world that
 * is stepped at 30 Hz (the original ran at 60 Hz with 32 pixel tiles - same speed on screen) and
 * sound requests going through a callback.
 *
 * Original work: Copyright (c) 2024 Willems Davy. Port changes: Copyright (c) 2026 Jicehel.
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace blips {

// ---------------------------------------------------------------------------- constants
constexpr int TILE = 16;                          // the original uses 32 px tiles on 640x360
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
constexpr int N_COLS = 50;                        // a level is at most 50 x 50 tiles
constexpr int N_ROWS = 50;
constexpr int COLS_VISIBLE = SCREEN_W / TILE;     // 20
constexpr int ROWS_VISIBLE = SCREEN_H / TILE;     // 15
constexpr int MAX_PARTS = N_ROWS * N_COLS * 3;

// Part ids: they are also the numbers stored in the .lev files.
enum PartId {
    ID_EMPTY = 1, ID_PLAYER = 2, ID_BOX = 3, ID_FLOOR = 4, ID_BOMB = 5, ID_WALL = 6, ID_DIAMOND = 7,
    ID_PLAYER2 = 8, ID_BOX1 = 9, ID_BOX2 = 10, ID_BOXBOMB = 11, ID_BOXWALL = 12, ID_WALLBREAKABLE = 13,
    ID_EXPLOSION = 14,
    ID_EDITOR_FIRST = 1, ID_EDITOR_LAST = 13      // what the editor can place
};

// Draw order (higher on top).
enum { Z_FLOOR = 1, Z_DIAMOND = 3, Z_BOMB = 3, Z_WALL = 4, Z_BOX = 5, Z_PLAYER = 10, Z_EXPLOSION = 15 };

enum Sound { SND_MENU, SND_SELECT, SND_ERROR, SND_STAGEEND, SND_EXPLODE, SND_COLLECT, SND_BACK, SND_MOVE, SND_COUNT };

// Timing: the world is stepped 30 times per second (the original 60 times with 32 px tiles).
constexpr int PLAYER_ANIM_DELAY = 6;              // frames per walking phase     (original 12 @60Hz)
constexpr int EXPLOSION_ANIM_DELAY = 1;           // frames per explosion phase   (original 2  @60Hz)
constexpr int EXPLOSION_PHASES = 8;
constexpr int MOVE_SPEED = 2;                     // pixels per step: 8 steps per tile (original 2 px of 32)
constexpr int FLICKER_FRAMES = 9;                 // player switch blink          (original 18 @60Hz)

class World;
class Part;

// ---------------------------------------------------------------------------- view port
class ViewPort {
public:
    int VPMinX, VPMinY, VPMaxX, VPMaxY;
    int MinScreenX, MinScreenY, MaxScreenX, MaxScreenY;
    int VPLimitMinX, VPLimitMinY, VPLimitMaxX, VPLimitMaxY;
    int Width, Height;

    ViewPort(int MinX, int MinY, int MaxX, int MaxY, int MinX2, int MinY2, int MaxX2, int MaxY2);
    void SetVPLimit(int MinX, int MinY, int MaxX, int MaxY);
    void Move(int Xi, int Yi);
    void SetViewPort(int MinX, int MinY, int MaxX, int MaxY);
};

// ---------------------------------------------------------------------------- drawing hook
// Called for every visible part: (type, animation phase, screen x, screen y).
typedef void (*BlitFn)(void* user, int type, int phase, int sx, int sy);
struct Painter { BlitFn blit; void* user; };

// ---------------------------------------------------------------------------- parts
class Part {
public:
    Part(int PlayFieldX, int PlayFieldY);
    virtual ~Part();

    World* ParentList;
    int PlayFieldX, PlayFieldY;
    int X, Y, Xi, Yi;
    int Type, Z, Group;
    int MoveDelay, MoveDelayCounter, MoveSpeed;
    int AnimPhase;
    bool IsMoving, PNeedToKill, BHide, IsDeath;

    void Hide() { BHide = true; }
    bool NeedHide() const { return BHide; }
    void Kill() { PNeedToKill = true; }
    bool NeedToKill() const { return PNeedToKill; }
    int GetType() const { return Type; }
    int GetPlayFieldX() const { return PlayFieldX; }
    int GetPlayFieldY() const { return PlayFieldY; }
    int GetZ() const { return Z; }

    void SetPosition(int PlayFieldXin, int PlayFieldYin);
    virtual void MoveTo(int PlayFieldXin, int PlayFieldYin, bool BackWards);
    virtual bool CanMoveTo(int PlayFieldXin, int PlayFieldYin);
    void Move();
    // Runs the "before draw" animation step and, when `p` is given, draws the part.
    void Draw(const Painter* p);

    virtual void Event_ArrivedOnNewSpot() {}
    virtual void Event_BeforeDraw() {}
    virtual void Event_LeaveCurrentSpot() {}
    virtual void Event_Moving(int, int, int, int) {}
};

class Empty : public Part { public: Empty(int x, int y); };
class Wall : public Part { public: Wall(int x, int y); };
class WallBreakable : public Part { public: WallBreakable(int x, int y); };
class Floor : public Part { public: Floor(int x, int y); };
class Bomb : public Part { public: Bomb(int x, int y); };
class Diamond : public Part { public: Diamond(int x, int y); };

class Explosion : public Part {
public:
    Explosion(int x, int y);
    void Event_BeforeDraw() override;
    int AnimPhases, AnimDelay, AnimDelayCounter;
};

// Box (id 3), Box1 (only player 1), Box2 (only player 2), BoxWall (two of them make a wall) and
// BoxBomb (destroys what it touches) share the same shape, only the rules differ.
class Box : public Part {
public:
    Box(int x, int y, int type, int phase);
    bool CanMoveTo(int x, int y) override;
    void Event_ArrivedOnNewSpot() override;
};

class Player : public Part {
public:
    Player(int x, int y);
    bool CanMoveTo(int x, int y) override;
    void MoveTo(int x, int y, bool BackWards) override;
    void Event_ArrivedOnNewSpot() override;
    void Event_BeforeDraw() override;
    void Event_Moving(int sx, int sy, int xi, int yi) override;
    int AnimBase, AnimPhases, AnimCounter, AnimDelay, AnimDelayCounter;
};

class Player2 : public Player { public: Player2(int x, int y); };

Part* make_part(int type, int x, int y);          // nullptr for ids that are not placeable

// ---------------------------------------------------------------------------- the world
class World {
public:
    World();
    ~World();
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    Part** Items;
    int ItemCount;
    Part** MoveAbleItems;
    int MoveAbleItemCount;
    bool DisableSorting;
    ViewPort* VP;
    Part *Player, *Player1, *Player2;
    int ActivePlayer;
    int ActivePlayerFlicker;

    void (*sound_cb)(void* user, int snd);
    void* sound_user;
    void play(int snd) { if (sound_cb) sound_cb(sound_user, snd); }

    void RemoveAll();
    void Remove(int x, int y);                    // everything on that tile
    void Remove(int x, int y, int type);          // every part of that type on that tile
    void Add(Part* p);
    void Sort();
    void forget(Part* gone);                      // drops dangling pointers to a deleted part

    // Level files: records of (type, x, y), one byte each.
    bool LoadBuffer(const uint8_t* data, size_t n);
    bool LoadFile(const char* path);
    size_t SaveBuffer(uint8_t* out, size_t cap) const;
    bool SaveFile(const char* path) const;

    void Move();
    void Draw(const Painter* p);                  // draws (if p) and removes the killed parts
    void SwitchPlayers();
    void CenterVPOnPlayer();
    void LimitVPLevel();

    int Count(int type) const;
    bool StageDone() const { return Count(ID_DIAMOND) == 0; }
    bool AnyPlayerDead() const;
    bool ExplosionsActive() const { return Count(ID_EXPLOSION) > 0; }

    // ---- level editor helpers (same rules as the original editor)
    // Puts `type` on tile (x, y), removing what cannot share the tile. Returns true when the level
    // changed (the original's "LevelHasChanged").
    bool EditPlace(int type, int x, int y);
    // Moves the whole level to the middle of the 50x50 area. Returns true when something moved.
    bool CenterLevel();
    // Why a level cannot be saved: 0 = fine, 1 = no player, 2 = no coin.
    int LevelError() const;
};

}  // namespace blips
