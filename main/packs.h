/*
 * packs.h - level packs on the SD card (or next to the PC program).
 *
 * A pack is a folder holding level1.lev, level2.lev ... (no gaps) and an optional credits.dat:
 *   BLIPS/levelpacks/<pack>/   shipped packs (never modified)
 *   BLIPS/mylevels/<pack>/     packs made with the editor; a folder here with the name of a shipped pack
 *                              replaces its levels (same idea as ~/.blips_levelpacks upstream)
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stddef.h>

#include "engine/blips_world.h"

namespace packs {

constexpr int MAX_PACKS = 48;
constexpr int MAX_LEVELS = 100;

struct Pack {
    char dir[40];          // folder name
    char name[40];         // shown name (underscores become spaces)
    char creator[48];      // from credits.dat, may be empty
    bool user;             // only in mylevels/: editable
    int count;             // number of levels
};

extern Pack list[MAX_PACKS];
extern int count;

void scan();                                           // fills list[], shipped packs first, then user packs
int  count_levels(const Pack& p);
// Path of a level to read (user folder first, then the shipped one). false when it does not exist.
bool level_path(const Pack& p, int level, char* out, size_t n);
bool load_level(blips::World& w, const Pack& p, int level);

// ---- editor side (user packs only)
bool valid_name(const char* name);                     // A-Z, 0-9, '_' , 1..16 chars
bool create_pack(const char* name, const char* creator);
bool user_level_path(const char* dir, int level, char* out, size_t n);
bool save_level(const char* dir, int level, const blips::World& w);
bool delete_level(const char* dir, int level, int total);   // later levels move down by one
void display_name(const char* dir, char* out, size_t n);

}  // namespace packs
