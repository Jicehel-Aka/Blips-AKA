/*
 * packs.cpp - see packs.h.
 * SPDX-License-Identifier: MIT
 */
#include "packs.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "platform.h"

namespace packs {

Pack list[MAX_PACKS];
int count = 0;

static void sub_path(char* out, size_t n, const char* folder, const char* dir, const char* file)
{
    char rel[200];
    if (file) snprintf(rel, sizeof rel, "%s/%s/%s", folder, dir, file);
    else snprintf(rel, sizeof rel, "%s/%s", folder, dir);
    plat::data_path(out, n, rel);
}

void display_name(const char* dir, char* out, size_t n)
{
    size_t o = 0;
    for (; dir[o] && o + 1 < n; ++o) out[o] = dir[o] == '_' ? ' ' : dir[o];
    out[o] = 0;
}

static bool read_creator(const char* path, char* out, size_t n)
{
    out[0] = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        const char* p = strstr(line, "Creator=");
        if (!p) continue;
        p += 8;
        if (*p == '\'' || *p == '"') ++p;
        size_t o = 0;
        while (*p && *p != '\'' && *p != '"' && *p != '\r' && *p != '\n' && o + 1 < n) out[o++] = *p++;
        out[o] = 0;
        break;
    }
    fclose(f);
    return out[0] != 0;
}

bool user_level_path(const char* dir, int level, char* out, size_t n)
{
    char f[32];
    snprintf(f, sizeof f, "level%d.lev", level + 1);
    sub_path(out, n, "mylevels", dir, f);
    return true;
}

bool level_path(const Pack& p, int level, char* out, size_t n)
{
    char f[32];
    snprintf(f, sizeof f, "level%d.lev", level + 1);
    sub_path(out, n, "mylevels", p.dir, f);
    if (plat::file_exists(out)) return true;
    if (p.user) return false;
    sub_path(out, n, "levelpacks", p.dir, f);
    return plat::file_exists(out);
}

int count_levels(const Pack& p)
{
    char path[700];
    int n = 0;
    while (n < MAX_LEVELS && level_path(p, n, path, sizeof path)) ++n;
    return n;
}

static int cmp(const void* a, const void* b)
{
    const Pack* x = (const Pack*)a;
    const Pack* y = (const Pack*)b;
    return strcasecmp(x->dir, y->dir);
}

static bool find(const char* dir)
{
    for (int i = 0; i < count; ++i)
        if (strcasecmp(list[i].dir, dir) == 0) return true;
    return false;
}

static void add_from(const char* folder, bool user)
{
    char path[700];
    plat::data_path(path, sizeof path, folder);
    DIR* d = opendir(path);
    if (!d) return;
    const int first = count;
    while (struct dirent* e = readdir(d)) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof list[0].dir) continue;
        char sub[760];
        sub_path(sub, sizeof sub, folder, e->d_name, nullptr);
        struct stat st;
        if (stat(sub, &st) != 0 || !(st.st_mode & S_IFDIR)) continue;
        if (user && find(e->d_name)) continue;                 // override of a shipped pack: already listed
        if (count >= MAX_PACKS) break;
        Pack& p = list[count];
        memset(&p, 0, sizeof p);
        snprintf(p.dir, sizeof p.dir, "%s", e->d_name);
        display_name(p.dir, p.name, sizeof p.name);
        p.user = user;
        p.count = count_levels(p);
        char cr[760];
        sub_path(cr, sizeof cr, user ? "mylevels" : "levelpacks", p.dir, "credits.dat");
        read_creator(cr, p.creator, sizeof p.creator);
        if (p.count > 0 || user) ++count;                       // an empty user pack stays visible for the editor
    }
    closedir(d);
    qsort(list + first, (size_t)(count - first), sizeof list[0], cmp);
}

void scan()
{
    count = 0;
    add_from("levelpacks", false);
    add_from("mylevels", true);
}

bool load_level(blips::World& w, const Pack& p, int level)
{
    char path[700];
    if (!level_path(p, level, path, sizeof path)) return false;
    return w.LoadFile(path);
}

bool valid_name(const char* name)
{
    const size_t n = strlen(name);
    if (n < 1 || n > 16) return false;
    for (size_t i = 0; i < n; ++i)
        if (!(isupper((unsigned char)name[i]) || isdigit((unsigned char)name[i]) || name[i] == '_')) return false;
    return true;
}

bool create_pack(const char* name, const char* creator)
{
    if (!valid_name(name)) return false;
    char path[700];
    plat::data_path(path, sizeof path, "mylevels");
    plat::make_dir(path);
    sub_path(path, sizeof path, "mylevels", name, nullptr);
    plat::make_dir(path);
    sub_path(path, sizeof path, "mylevels", name, "credits.dat");
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "[Credits]\nCreator='%s'\n", creator && *creator ? creator : "me");
    fclose(f);
    return true;
}

bool save_level(const char* dir, int level, const blips::World& w)
{
    char path[700];
    user_level_path(dir, level, path, sizeof path);
    return w.SaveFile(path);
}

bool delete_level(const char* dir, int level, int total)
{
    char a[700], b[700];
    user_level_path(dir, level, a, sizeof a);
    remove(a);
    for (int i = level + 1; i < total; ++i) {
        user_level_path(dir, i, a, sizeof a);
        user_level_path(dir, i - 1, b, sizeof b);
        if (rename(a, b) != 0) return false;
    }
    return true;
}

}  // namespace packs
