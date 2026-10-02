/*
 * blips_world.cpp - see blips_world.h.
 *
 * Derived from "Blips" by Willems Davy (joyrider3774), MIT License.
 * Original work: Copyright (c) 2024 Willems Davy. Port changes: Copyright (c) 2026 Jicehel.
 * SPDX-License-Identifier: MIT
 */
#include "blips_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace blips {

// ============================================================================ ViewPort
ViewPort::ViewPort(int MinX, int MinY, int MaxX, int MaxY, int MinX2, int MinY2, int MaxX2, int MaxY2)
{
    if ((MinX < N_COLS) && (MinX >= 0) && (MaxX + 1 < N_COLS) && (MaxX >= 0) &&
        (MinY < N_ROWS) && (MinY >= 0) && (MaxY + 1 < N_ROWS) && (MaxY >= 0)) {
        VPMinX = MinX; VPMinY = MinY; VPMaxX = MaxX; VPMaxY = MaxY;
        MinScreenX = VPMinX * TILE;
        MinScreenY = VPMinY * TILE;
        MaxScreenX = VPMaxX * TILE;
        MaxScreenY = VPMaxY * TILE;
    } else {
        VPMinX = 0; VPMinY = 0;
        VPMaxX = COLS_VISIBLE; VPMaxY = ROWS_VISIBLE;
        MinScreenX = 0; MinScreenY = 0;
        MaxScreenX = SCREEN_W; MaxScreenY = SCREEN_H;
    }
    if ((MinX2 < N_COLS) && (MinX2 >= 0) && (MaxX2 + 1 < N_COLS) && (MaxX2 >= 0) &&
        (MinY2 < N_ROWS) && (MinY2 >= 0) && (MaxY2 + 1 < N_ROWS) && (MaxY2 >= 0)) {
        VPLimitMinX = MinX2; VPLimitMinY = MinY2;
        VPLimitMaxX = MaxX2 + 1;       // one more: the end of the last tile
        VPLimitMaxY = MaxY2 + 1;
    } else {
        VPLimitMinX = 0; VPLimitMinY = 0;
        VPLimitMaxX = N_COLS; VPLimitMaxY = N_ROWS;
    }
    Width = VPMaxX - VPMinX;
    Height = VPMaxY - VPMinY;
}

void ViewPort::SetVPLimit(int MinX, int MinY, int MaxX, int MaxY)
{
    if ((MinX < N_COLS) && (MinX >= 0) && (MaxX + 1 < N_COLS) && (MaxX + 1 >= 0) &&
        (MinY < N_ROWS) && (MinY >= 0) && (MaxY + 1 < N_ROWS) && (MaxY + 1 >= 0)) {
        VPLimitMinX = MinX; VPLimitMinY = MinY;
        VPLimitMaxX = MaxX + 1;
        VPLimitMaxY = MaxY + 1;
    } else {
        VPLimitMinX = 0; VPLimitMinY = 0;
        VPLimitMaxX = N_COLS; VPLimitMaxY = N_ROWS;
    }
    if (VPLimitMaxX - VPLimitMinX < Width) {
        if (VPLimitMaxX - Width >= 0) VPLimitMinX = VPLimitMaxX - Width;
        else { VPLimitMinX = 0; VPLimitMaxX = VPLimitMinX + Width; }
    }
    if (VPLimitMaxY - VPLimitMinY < Height) {
        if (VPLimitMaxY - Height >= 0) VPLimitMinY = VPLimitMaxY - Height;
        else { VPLimitMinY = 0; VPLimitMaxY = VPLimitMinY + Height; }
    }
}

void ViewPort::Move(int Xi, int Yi)
{
    if ((MinScreenX + Xi <= TILE * VPLimitMaxX) && (MinScreenX + Xi >= VPLimitMinX * TILE) &&
        (MaxScreenX + Xi <= TILE * VPLimitMaxX) && (MaxScreenX + Xi >= VPLimitMinX * TILE) &&
        (MinScreenY + Yi <= TILE * VPLimitMaxY) && (MinScreenY + Yi >= VPLimitMinY * TILE) &&
        (MaxScreenY + Yi <= TILE * VPLimitMaxY) && (MaxScreenY + Yi >= VPLimitMinY * TILE)) {
        MinScreenX += Xi; MaxScreenX += Xi;
        MinScreenY += Yi; MaxScreenY += Yi;
        VPMinX = MinScreenX / TILE;
        VPMinY = MinScreenY / TILE;
        VPMaxX = MaxScreenX / TILE;
        VPMaxY = MaxScreenY / TILE;
    }
}

void ViewPort::SetViewPort(int MinX, int MinY, int MaxX, int MaxY)
{
    if ((MinX <= VPLimitMaxX) && (MinX >= VPLimitMinX) && (MaxX + 1 <= VPLimitMaxX) && (MaxX + 1 >= VPLimitMinX) &&
        (MinY <= VPLimitMaxY) && (MinY >= VPLimitMinY) && (MaxY + 1 <= VPLimitMaxY) && (MaxY + 1 >= VPLimitMinY)) {
        VPMinX = MinX; VPMinY = MinY;
        VPMaxX = MaxX + 1; VPMaxY = MaxY + 1;
        MinScreenX = VPMinX * TILE; MinScreenY = VPMinY * TILE;
        MaxScreenX = VPMaxX * TILE; MaxScreenY = VPMaxY * TILE;
    } else {
        VPMinX = MinX; VPMinY = MinY;
        VPMaxX = MaxX + 1; VPMaxY = MaxY + 1;
        if (VPMinX < VPLimitMinX) { VPMinX = VPLimitMinX; VPMaxX = VPLimitMinX + COLS_VISIBLE; }
        if (VPMaxX > VPLimitMaxX) { VPMinX = VPLimitMaxX - COLS_VISIBLE; VPMaxX = VPLimitMaxX; }
        if (VPMinY < VPLimitMinY) { VPMaxY = VPLimitMinY + ROWS_VISIBLE; VPMinY = VPLimitMinY; }
        if (VPMaxY > VPLimitMaxY) { VPMinY = VPLimitMaxY - ROWS_VISIBLE; VPMaxY = VPLimitMaxY; }
        MinScreenX = VPMinX * TILE; MinScreenY = VPMinY * TILE;
        MaxScreenX = VPMaxX * TILE; MaxScreenY = VPMaxY * TILE;
    }
    Width = VPMaxX - VPMinX;
    Height = VPMaxY - VPMinY;
}

// ============================================================================ Part
Part::Part(int px, int py)
{
    ParentList = nullptr;
    PlayFieldX = px; PlayFieldY = py;
    X = px * TILE; Y = py * TILE;
    Xi = Yi = 0;
    Type = 0; Z = 0; Group = 0;
    MoveDelay = MoveDelayCounter = MoveSpeed = 0;
    AnimPhase = 0;
    IsMoving = PNeedToKill = BHide = IsDeath = false;
}

Part::~Part() {}

void Part::SetPosition(int px, int py)
{
    if ((px >= 0) && (px < N_COLS) && (py >= 0) && (py < N_ROWS)) {
        PlayFieldX = px; PlayFieldY = py;
        X = px * TILE; Y = py * TILE;
    }
}

void Part::MoveTo(int px, int py, bool BackWards)
{
    if (!IsMoving && !NeedToKill() && !NeedHide()) {
        if ((px != PlayFieldX) || (py != PlayFieldY))
            if (CanMoveTo(px, py) || BackWards) {
                PlayFieldX = px;
                PlayFieldY = py;
                if (X < PlayFieldX * TILE) Xi = MoveSpeed;
                if (X > PlayFieldX * TILE) Xi = -MoveSpeed;
                if (Y > PlayFieldY * TILE) Yi = -MoveSpeed;
                if (Y < PlayFieldY * TILE) Yi = MoveSpeed;
                IsMoving = true;
                Event_LeaveCurrentSpot();
            }
    }
}

bool Part::CanMoveTo(int, int) { return false; }

void Part::Move()
{
    if (IsMoving && !NeedToKill() && !NeedHide()) {
        if (MoveDelayCounter == MoveDelay) {
            X += Xi;
            Y += Yi;
            Event_Moving(X, Y, Xi, Yi);
            if ((X == PlayFieldX * TILE) && (Y == PlayFieldY * TILE)) {
                IsMoving = false;
                Xi = 0;
                Yi = 0;
                Event_ArrivedOnNewSpot();
            }
            MoveDelayCounter = -1;
        }
        MoveDelayCounter++;
    }
}

void Part::Draw(const Painter* p)
{
    if (!BHide) {
        Event_BeforeDraw();
        if (p && p->blit) {
            if (ParentList)
                p->blit(p->user, Type, AnimPhase, X - ParentList->VP->MinScreenX, Y - ParentList->VP->MinScreenY);
            else
                p->blit(p->user, Type, AnimPhase, X, Y);
        }
    }
}

// ============================================================================ simple parts
Empty::Empty(int x, int y) : Part(x, y) { Type = ID_EMPTY; }
Wall::Wall(int x, int y) : Part(x, y) { Type = ID_WALL; Z = Z_WALL; }
WallBreakable::WallBreakable(int x, int y) : Part(x, y) { AnimPhase = 1; Type = ID_WALLBREAKABLE; Z = Z_WALL; }
Floor::Floor(int x, int y) : Part(x, y) { Type = ID_FLOOR; Z = Z_FLOOR; }
Bomb::Bomb(int x, int y) : Part(x, y) { Type = ID_BOMB; Z = Z_BOMB; }
Diamond::Diamond(int x, int y) : Part(x, y) { Type = ID_DIAMOND; Z = Z_DIAMOND; }

Explosion::Explosion(int x, int y) : Part(x, y)
{
    Type = ID_EXPLOSION;
    Z = Z_EXPLOSION;
    AnimPhase = 0;
    AnimPhases = EXPLOSION_PHASES;
    AnimDelay = EXPLOSION_ANIM_DELAY;
    AnimDelayCounter = 0;
    MoveDelay = 0;
    MoveSpeed = 4;
}

void Explosion::Event_BeforeDraw()
{
    AnimDelayCounter++;
    if (AnimDelayCounter == AnimDelay) {
        AnimDelayCounter = 0;
        AnimPhase++;
        if (AnimPhase == AnimPhases) Kill();
    }
}

// ============================================================================ boxes
static bool is_type(const Part* p, int t) { return p->Type == t; }

// True when a part of one of `types` (alive) sits on tile (x, y).
static bool blocked_by(World* w, int x, int y, const int* types, int n)
{
    for (int i = 0; i < w->ItemCount; i++) {
        const Part* it = w->Items[i];
        if (it->NeedToKill() || it->NeedHide()) continue;
        for (int k = 0; k < n; k++)
            if (it->Type == types[k] && it->PlayFieldX == x && it->PlayFieldY == y) return true;
    }
    return false;
}

Box::Box(int x, int y, int type, int phase) : Part(x, y)
{
    Type = type;
    AnimPhase = phase;
    MoveDelay = 0;
    MoveSpeed = MOVE_SPEED;
    Z = Z_BOX;
}

bool Box::CanMoveTo(int x, int y)
{
    if (!((x >= 0) && (x < N_COLS) && (y >= 0) && (y < N_ROWS))) return false;
    if (!ParentList) return true;
    switch (Type) {
        case ID_BOXBOMB: {
            static const int t[] = { ID_WALL, ID_DIAMOND };
            return !blocked_by(ParentList, x, y, t, 2);
        }
        case ID_BOXWALL: {
            static const int t[] = { ID_WALL, ID_WALLBREAKABLE, ID_BOX, ID_DIAMOND, ID_PLAYER, ID_PLAYER2, ID_BOX1, ID_BOX2 };
            return !blocked_by(ParentList, x, y, t, 8);
        }
        default: {                                   // Box, Box1, Box2
            static const int t[] = { ID_WALL, ID_WALLBREAKABLE, ID_BOX, ID_DIAMOND, ID_PLAYER, ID_PLAYER2,
                                     ID_BOX1, ID_BOX2, ID_BOXWALL };
            return !blocked_by(ParentList, x, y, t, 9);
        }
    }
}

void Box::Event_ArrivedOnNewSpot()
{
    switch (Type) {
        case ID_BOX1: AnimPhase = 2; break;
        case ID_BOX2: AnimPhase = 3; break;
        case ID_BOXWALL: AnimPhase = 1; break;
        case ID_BOXBOMB: AnimPhase = 4; break;
        default: AnimPhase = 0; break;
    }
    World* w = ParentList;
    if (!w) return;
    for (int i = 0; i < w->ItemCount; i++) {
        Part* it = w->Items[i];
        if (it->NeedToKill() || it->NeedHide()) continue;
        if (it->PlayFieldX != PlayFieldX || it->PlayFieldY != PlayFieldY) continue;

        if (Type == ID_BOXBOMB) {
            if (is_type(it, ID_BOMB)) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
            }
            if (it->Type != ID_WALL && it->Type != ID_DIAMOND && it->Type != ID_FLOOR && it != this) {
                // kill needs to come first: Add sorts the list
                if (it->Type == ID_PLAYER || it->Type == ID_PLAYER2) {
                    it->IsDeath = true;
                    it->Hide();
                } else {
                    it->Kill();
                }
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
                break;
            }
            if (is_type(it, ID_BOXBOMB) && this != it) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
                break;
            }
        } else if (Type == ID_BOXWALL) {
            if (is_type(it, ID_BOMB)) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
            }
            if (is_type(it, ID_BOXWALL) && it != this) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
                w->Add(new Wall(PlayFieldX, PlayFieldY));
                break;
            }
            if (is_type(it, ID_BOXBOMB) && this != it) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
                break;
            }
        } else {                                     // Box, Box1, Box2
            if (is_type(it, ID_BOMB)) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
            }
            if (is_type(it, ID_BOXBOMB)) {
                it->Kill();
                Kill();
                w->Add(new Explosion(PlayFieldX, PlayFieldY));
                break;
            }
        }
    }
}

// ============================================================================ players
Player::Player(int x, int y) : Part(x, y)
{
    AnimBase = 4;
    AnimPhase = 4;
    AnimPhases = 4;
    AnimCounter = 0;
    AnimDelay = PLAYER_ANIM_DELAY;
    MoveDelay = 0;
    MoveSpeed = MOVE_SPEED;
    AnimDelayCounter = 0;
    Type = ID_PLAYER;
    Z = Z_PLAYER;
}

Player2::Player2(int x, int y) : Player(x, y) { Type = ID_PLAYER2; }

bool Player::CanMoveTo(int x, int y)
{
    bool Result = true;
    if ((x >= 0) && (x < N_COLS) && (y >= 0) && (y < N_ROWS)) {
        if (ParentList) {
            for (int i = 0; i < ParentList->ItemCount; i++) {
                Part* it = ParentList->Items[i];
                if (it->NeedToKill() || it->NeedHide()) continue;
                if ((it->PlayFieldX == x) && (it->PlayFieldY == y)) {
                    const int t = it->Type;
                    if ((t == ID_WALL) || (t == ID_WALLBREAKABLE) || (t == ID_PLAYER) || (t == ID_PLAYER2) ||
                        ((GetType() == ID_PLAYER2) && (t == ID_BOX1)) ||
                        ((GetType() == ID_PLAYER) && (t == ID_BOX2))) {
                        Result = false;
                        break;
                    }
                    if ((t == ID_BOX) ||
                        ((GetType() == ID_PLAYER) && (t == ID_BOX1)) ||
                        ((GetType() == ID_PLAYER2) && (t == ID_BOX2)) ||
                        (t == ID_BOXWALL) || (t == ID_BOXBOMB)) {
                        if (PlayFieldX > x) Result = it->CanMoveTo(x - 1, y);
                        if (PlayFieldX < x) Result = it->CanMoveTo(x + 1, y);
                        if (PlayFieldY > y) Result = it->CanMoveTo(x, y - 1);
                        if (PlayFieldY < y) Result = it->CanMoveTo(x, y + 1);
                        break;
                    }
                }
            }
        }
    } else {
        Result = false;
    }
    return Result;
}

void Player::Event_ArrivedOnNewSpot()
{
    if (!ParentList) return;
    for (int i = 0; i < ParentList->ItemCount; i++) {
        Part* it = ParentList->Items[i];
        if (it->NeedToKill() || it->NeedHide()) continue;
        if ((it->PlayFieldX == PlayFieldX) && (it->PlayFieldY == PlayFieldY)) {
            if (it->Type == ID_BOMB) {
                it->Kill();
                IsDeath = true;
                Hide();
                ParentList->Add(new Explosion(PlayFieldX, PlayFieldY));
                break;
            }
            if (it->Type == ID_DIAMOND) {
                it->Kill();
                ParentList->play(SND_COLLECT);
            }
        }
    }
}

void Player::Event_BeforeDraw()
{
    if (IsMoving) {
        AnimPhase = AnimBase + AnimCounter;
        AnimDelayCounter++;
        if (AnimDelayCounter == AnimDelay) {
            AnimDelayCounter = 0;
            AnimCounter++;
            if (AnimCounter == AnimPhases) AnimCounter = 0;
        }
    }
}

void Player::Event_Moving(int sx, int sy, int, int)
{
    ViewPort* vp = ParentList->VP;
    if ((sx > vp->MaxScreenX - SCREEN_W / 2) && (Xi > 0)) vp->Move(Xi, Yi);
    if ((sx < vp->MaxScreenX - SCREEN_W / 2) && (Xi < 0)) vp->Move(Xi, Yi);
    if ((sy > vp->MaxScreenY - SCREEN_H / 2) && (Yi > 0)) vp->Move(Xi, Yi);
    if ((sy < vp->MaxScreenY - SCREEN_H / 2) && (Yi < 0)) vp->Move(Xi, Yi);
}

void Player::MoveTo(int x, int y, bool BackWards)
{
    if (!IsMoving && !NeedToKill() && !NeedHide()) {
        if (CanMoveTo(x, y) || BackWards) {
            PlayFieldX = x;
            PlayFieldY = y;
            // the box (if any) that is on the tile we walk to is pushed one tile further
            int dx = 0, dy = 0;
            if (X < PlayFieldX * TILE) { Xi = MoveSpeed; dx = 1; AnimBase = 4; }
            if (X > PlayFieldX * TILE) { Xi = -MoveSpeed; dx = -1; AnimBase = 0; }
            if (Y > PlayFieldY * TILE) { Yi = -MoveSpeed; dy = -1; AnimBase = 8; }
            if (Y < PlayFieldY * TILE) { Yi = MoveSpeed; dy = 1; AnimBase = 12; }
            if ((dx || dy) && ParentList) {
                for (int i = 0; i < ParentList->ItemCount; i++) {
                    Part* it = ParentList->Items[i];
                    const int t = it->Type;
                    if (((t == ID_BOX) || (t == ID_BOXWALL) ||
                         ((GetType() == ID_PLAYER) && (t == ID_BOX1)) ||
                         ((GetType() == ID_PLAYER2) && (t == ID_BOX2)) ||
                         (t == ID_BOXBOMB)) &&
                        ((it->PlayFieldX == PlayFieldX) && (it->PlayFieldY == PlayFieldY))) {
                        it->MoveTo(PlayFieldX + dx, PlayFieldY + dy, false);
                        break;
                    }
                }
            }
            IsMoving = true;
            if (ParentList) ParentList->play(SND_MOVE);
        } else {
            // blocked: only turn on the spot (and keep walking in place)
            if (x > PlayFieldX) AnimBase = 4;
            if (x < PlayFieldX) AnimBase = 0;
            if (y > PlayFieldY) AnimBase = 12;
            if (y < PlayFieldY) AnimBase = 8;
            AnimPhase = AnimBase + AnimCounter;
            AnimDelayCounter++;
            if (AnimDelayCounter == AnimDelay) {
                AnimDelayCounter = 0;
                AnimCounter++;
                if (AnimCounter == AnimPhases) AnimCounter = 0;
            }
        }
    }
}

Part* make_part(int type, int x, int y)
{
    switch (type) {
        case ID_EMPTY: return new Empty(x, y);
        case ID_BOX: return new Box(x, y, ID_BOX, 0);
        case ID_PLAYER: return new Player(x, y);
        case ID_FLOOR: return new Floor(x, y);
        case ID_WALL: return new Wall(x, y);
        case ID_BOMB: return new Bomb(x, y);
        case ID_DIAMOND: return new Diamond(x, y);
        case ID_PLAYER2: return new Player2(x, y);
        case ID_BOX1: return new Box(x, y, ID_BOX1, 2);
        case ID_BOX2: return new Box(x, y, ID_BOX2, 3);
        case ID_BOXBOMB: return new Box(x, y, ID_BOXBOMB, 4);
        case ID_BOXWALL: return new Box(x, y, ID_BOXWALL, 1);
        case ID_WALLBREAKABLE: return new WallBreakable(x, y);
        default: return nullptr;
    }
}

// ============================================================================ World
World::World()
{
    Items = (Part**)calloc(MAX_PARTS, sizeof(Part*));
    MoveAbleItems = (Part**)calloc(MAX_PARTS, sizeof(Part*));
    ItemCount = 0;
    MoveAbleItemCount = 0;
    DisableSorting = false;
    Player = Player1 = Player2 = nullptr;
    ActivePlayer = -1;
    ActivePlayerFlicker = 0;
    sound_cb = nullptr;
    sound_user = nullptr;
    // 20 x 15 tiles on screen (the original: 19 x 11 of 32 px)
    VP = new ViewPort(0, 0, COLS_VISIBLE - 1, ROWS_VISIBLE - 1, 0, 0, N_COLS - 1, N_ROWS - 1);
}

World::~World()
{
    for (int i = 0; i < ItemCount; i++) delete Items[i];
    delete VP;
    free(Items);
    free(MoveAbleItems);
}

void World::CenterVPOnPlayer()
{
    int PlayerX = -1, PlayerY = -1;
    for (int i = 0; i < ItemCount; i++)
        if (Items[i]->GetType() == ActivePlayer) {
            PlayerX = Items[i]->GetPlayFieldX();
            PlayerY = Items[i]->GetPlayFieldY();
            break;
        }
    VP->SetViewPort(PlayerX - COLS_VISIBLE / 2, PlayerY - ROWS_VISIBLE / 2,
                    PlayerX + COLS_VISIBLE / 2, PlayerY + ROWS_VISIBLE / 2);
}

void World::SwitchPlayers()
{
    if (!Player2) return;
    if (ActivePlayer == ID_PLAYER) {
        ActivePlayer = ID_PLAYER2;
        Player = Player2;
    } else {
        ActivePlayer = ID_PLAYER;
        Player = Player1;
    }
    ActivePlayerFlicker = FLICKER_FRAMES;
    CenterVPOnPlayer();
}

void World::LimitVPLevel()
{
    int MinX = N_COLS, MinY = N_ROWS, MaxX = -1, MaxY = -1;
    for (int i = 0; i < ItemCount; i++) {
        if (Items[i]->GetPlayFieldX() < MinX) MinX = Items[i]->GetPlayFieldX();
        if (Items[i]->GetPlayFieldY() < MinY) MinY = Items[i]->GetPlayFieldY();
        if (Items[i]->GetPlayFieldX() > MaxX) MaxX = Items[i]->GetPlayFieldX();
        if (Items[i]->GetPlayFieldY() > MaxY) MaxY = Items[i]->GetPlayFieldY();
    }
    VP->SetVPLimit(MinX, MinY, MaxX, MaxY);
    CenterVPOnPlayer();
}

void World::RemoveAll()
{
    for (int i = 0; i < ItemCount; i++) {
        delete Items[i];
        Items[i] = nullptr;
    }
    ItemCount = 0;
    MoveAbleItemCount = 0;
    ActivePlayer = -1;
    Player1 = nullptr;
    Player2 = nullptr;
    Player = nullptr;
}

void World::Remove(int x, int y)
{
    for (int a = 0; a < ItemCount; a++) {
        if ((Items[a]->GetPlayFieldX() == x) && (Items[a]->GetPlayFieldY() == y)) {
            Part* gone = Items[a];
            for (int b = a; b < ItemCount - 1; b++) Items[b] = Items[b + 1];
            ItemCount--;
            Items[ItemCount] = nullptr;
            forget(gone);
            delete gone;
            a--;
        }
    }
}

void World::Remove(int x, int y, int type)
{
    for (int a = 0; a < ItemCount; a++) {
        if ((Items[a]->GetPlayFieldX() == x) && (Items[a]->GetPlayFieldY() == y) && (Items[a]->GetType() == type)) {
            Part* gone = Items[a];
            for (int b = a; b < ItemCount - 1; b++) Items[b] = Items[b + 1];
            ItemCount--;
            Items[ItemCount] = nullptr;
            forget(gone);
            delete gone;
            a--;
        }
    }
}

// A deleted part must not stay referenced by the "moving items" list or the player pointers.
void World::forget(Part* gone)
{
    for (int i = 0; i < MoveAbleItemCount; i++)
        if (MoveAbleItems[i] == gone) {
            for (int j = i; j < MoveAbleItemCount - 1; j++) MoveAbleItems[j] = MoveAbleItems[j + 1];
            MoveAbleItemCount--;
            i--;
        }
    if (Player == gone) Player = nullptr;
    if (Player1 == gone) Player1 = nullptr;
    if (Player2 == gone) Player2 = nullptr;
}

void World::Add(Part* p)
{
    if (ItemCount < MAX_PARTS) {
        p->ParentList = this;
        Items[ItemCount] = p;
        ItemCount++;
        Sort();
    } else {
        delete p;
        return;
    }
    if (p->GetType() == ID_PLAYER) {
        Player1 = p;
        Player = Player1;
        ActivePlayer = ID_PLAYER;
    }
    if (p->GetType() == ID_PLAYER2) {
        Player2 = p;
        Player = Player2;
        ActivePlayer = ID_PLAYER2;
    }
    if (p->GetType() == ID_EXPLOSION) play(SND_EXPLODE);
}

void World::Sort()
{
    if (DisableSorting) return;
    for (int a = 1; a < ItemCount; a++) {
        const int Index = Items[a]->GetZ();
        Part* p = Items[a];
        int b = a;
        while ((b > 0) && (Items[b - 1]->GetZ() > Index)) {
            Items[b] = Items[b - 1];
            b--;
        }
        Items[b] = p;
    }
}

bool World::LoadBuffer(const uint8_t* data, size_t n)
{
    RemoveAll();
    DisableSorting = true;
    for (size_t pos = 0; pos + 3 <= n; pos += 3) {
        const int type = (int)(int8_t)data[pos];
        const int x = (int)(int8_t)data[pos + 1];
        const int y = (int)(int8_t)data[pos + 2];
        if (x < 0 || x >= N_COLS || y < 0 || y >= N_ROWS) continue;
        Part* p = make_part(type, x, y);
        if (p) Add(p);
    }
    DisableSorting = false;
    if (Player1 && Player2) {
        Player = Player1;
        ActivePlayer = ID_PLAYER;
    }
    Sort();
    LimitVPLevel();
    return true;
}

bool World::LoadFile(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    rewind(f);
    if (size < 0 || size > 3L * MAX_PARTS) { fclose(f); return false; }
    uint8_t* buf = (uint8_t*)malloc(size ? (size_t)size : 1);
    if (!buf) { fclose(f); return false; }
    const bool ok = fread(buf, 1, (size_t)size, f) == (size_t)size;
    fclose(f);
    if (ok) LoadBuffer(buf, (size_t)size);
    free(buf);
    return ok;
}

size_t World::SaveBuffer(uint8_t* out, size_t cap) const
{
    size_t pos = 0;
    for (int i = 0; i < ItemCount && pos + 3 <= cap; i++) {
        out[pos] = (uint8_t)Items[i]->GetType();
        out[pos + 1] = (uint8_t)Items[i]->GetPlayFieldX();
        out[pos + 2] = (uint8_t)Items[i]->GetPlayFieldY();
        pos += 3;
    }
    return pos;
}

bool World::SaveFile(const char* path) const
{
    uint8_t* buf = (uint8_t*)malloc(3 * (size_t)ItemCount + 3);
    if (!buf) return false;
    const size_t n = SaveBuffer(buf, 3 * (size_t)ItemCount);
    FILE* f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        ok = fwrite(buf, 1, n, f) == n;
        fclose(f);
    }
    free(buf);
    return ok;
}

void World::Move()
{
    MoveAbleItemCount = 0;
    for (int i = 0; i < ItemCount; i++) {
        if (Items[i]->IsMoving) {
            // other items are not moveable and have no effect
            if (!Items[i]->NeedToKill() && !Items[i]->NeedHide()) {
                Items[i]->Move();
                if (!Items[i]->NeedToKill() && !Items[i]->NeedHide()) MoveAbleItems[MoveAbleItemCount++] = Items[i];
            }
        }
    }
}

void World::Draw(const Painter* p)
{
    for (int i = 0; i < ItemCount; i++) {
        if (Items[i]->NeedToKill()) {
            Remove(Items[i]->GetPlayFieldX(), Items[i]->GetPlayFieldY(), Items[i]->GetType());
            i--;                                     // go back one item to prevent skips
        } else {
            if ((Items[i]->GetPlayFieldX() >= VP->VPMinX) && (Items[i]->GetPlayFieldX() - 1 <= VP->VPMaxX) &&
                (Items[i]->GetPlayFieldY() >= VP->VPMinY) && (Items[i]->GetPlayFieldY() - 1 <= VP->VPMaxY)) {
                if (ActivePlayerFlicker > 0) {
                    if (Items[i]->GetType() == ActivePlayer) {
                        if (ActivePlayerFlicker % 2 == 0) Items[i]->Draw(p);
                    } else {
                        Items[i]->Draw(p);
                    }
                } else {
                    Items[i]->Draw(p);
                }
            }
        }
    }
    // redraw the moving items so they are always on top
    for (int i = 0; i < MoveAbleItemCount; i++) MoveAbleItems[i]->Draw(p);
    if (ActivePlayerFlicker > 0) ActivePlayerFlicker--;
}

int World::Count(int type) const
{
    int n = 0;
    for (int i = 0; i < ItemCount; i++)
        if (Items[i]->GetType() == type) n++;
    return n;
}

bool World::AnyPlayerDead() const
{
    return (Player1 && Player1->IsDeath) || (Player2 && Player2->IsDeath);
}

int World::LevelError() const
{
    if (Count(ID_PLAYER) + Count(ID_PLAYER2) == 0) return 1;
    if (Count(ID_DIAMOND) == 0) return 2;
    return 0;
}

// ---------------------------------------------------------------------------- editor
bool World::EditPlace(int type, int x, int y)
{
    bool changed = false;
    bool same = false;
    for (int i = 0; i < ItemCount; i++)
        if ((Items[i]->GetPlayFieldX() == x) && (Items[i]->GetPlayFieldY() == y)) {
            if (Items[i]->GetType() == type) same = true;
            if (type == ID_EMPTY) {
                changed = true;
                break;
            }
        }
    if (type != ID_EMPTY && !changed) changed = !same;

    static const int ALL11[] = { ID_WALL, ID_WALLBREAKABLE, ID_BOX, ID_BOX1, ID_BOX2, ID_BOXBOMB, ID_BOXWALL,
                                 ID_PLAYER, ID_PLAYER2, ID_BOMB, ID_DIAMOND };
    auto clear_all11 = [&]() { for (int t : ALL11) Remove(x, y, t); };

    switch (type) {
        case ID_EMPTY:
            for (int i = 0; i < ItemCount; i++) {
                if ((Items[i]->GetPlayFieldX() == x) && (Items[i]->GetPlayFieldY() == y)) {
                    if (Items[i]->GetType() == ID_FLOOR) {
                        bool another = false;
                        for (int j = i + 1; j < ItemCount; j++)
                            if ((Items[j]->GetPlayFieldX() == x) && (Items[j]->GetPlayFieldY() == y)) {
                                Remove(Items[j]->GetPlayFieldX(), Items[j]->GetPlayFieldY(), Items[j]->GetType());
                                another = true;
                                break;
                            }
                        if (!another) {
                            Remove(Items[i]->GetPlayFieldX(), Items[i]->GetPlayFieldY(), ID_FLOOR);
                            break;
                        }
                    } else {
                        Remove(x, y);
                        break;
                    }
                }
            }
            break;
        case ID_BOX: case ID_BOX1: case ID_BOX2: case ID_BOXBOMB: case ID_BOXWALL:
        case ID_BOMB: case ID_DIAMOND: case ID_WALLBREAKABLE:
            clear_all11();
            Add(make_part(type, x, y));
            break;
        case ID_PLAYER:
        case ID_PLAYER2: {
            const int other = type == ID_PLAYER ? ID_PLAYER2 : ID_PLAYER;
            for (int t : ALL11) if (t != type) Remove(x, y, t);
            (void)other;
            for (int i = 0; i < ItemCount; i++)       // only one of each player in a level
                if (Items[i]->GetType() == type) Remove(Items[i]->GetPlayFieldX(), Items[i]->GetPlayFieldY(), type);
            Add(make_part(type, x, y));
            break;
        }
        case ID_WALL:
            Remove(x, y);
            Add(make_part(type, x, y));
            break;
        case ID_FLOOR:
            Remove(x, y, ID_FLOOR);
            Remove(x, y, ID_WALL);
            Add(make_part(type, x, y));
            break;
        default: break;
    }
    return changed;
}

bool World::CenterLevel()
{
    int MinX = N_COLS - 1, MinY = N_ROWS - 1, MaxX = 0, MaxY = 0;
    for (int i = 0; i < ItemCount; i++) {
        if (Items[i]->GetPlayFieldX() < MinX) MinX = Items[i]->GetPlayFieldX();
        if (Items[i]->GetPlayFieldY() < MinY) MinY = Items[i]->GetPlayFieldY();
        if (Items[i]->GetPlayFieldX() > MaxX) MaxX = Items[i]->GetPlayFieldX();
        if (Items[i]->GetPlayFieldY() > MaxY) MaxY = Items[i]->GetPlayFieldY();
    }
    const int Xi = ((N_COLS - 1) / 2) - (MaxX + MinX) / 2;
    const int Yi = ((N_ROWS - 1) / 2) - (MaxY + MinY) / 2;
    for (int i = 0; i < ItemCount; i++)
        Items[i]->SetPosition(Items[i]->GetPlayFieldX() + Xi, Items[i]->GetPlayFieldY() + Yi);
    return Xi != 0 || Yi != 0;
}

}  // namespace blips
