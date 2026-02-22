/**
 * src/eng_save.c — セーブ/ロード + フラグ/変数管理
 *
 * セーブデータは ~/.hajimu/saves/save_{slot}.dat に保存する。
 * 簡易バイナリ形式: 固定ヘッダー + アクター配列 + アイテム在庫 + フラグ + 変数。
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "eng_rpg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ── フラグ・変数 (線形探索ハッシュ) ─────────────────────*/
typedef struct { char key[RPG_KEY_LEN]; bool  val; bool used; } FlagEntry;
typedef struct { char key[RPG_KEY_LEN]; double val; bool used; } VarEntry;

static FlagEntry g_flags[RPG_MAX_FLAGS];
static VarEntry  g_vars[RPG_MAX_VARS];

void rpg_flag_set(const char* key, bool val) {
    for (int i = 0; i < RPG_MAX_FLAGS; ++i) {
        if (!g_flags[i].used || strcmp(g_flags[i].key, key) == 0) {
            strncpy(g_flags[i].key, key, RPG_KEY_LEN-1);
            g_flags[i].val  = val;
            g_flags[i].used = true;
            return;
        }
    }
}
bool rpg_flag_get(const char* key) {
    for (int i = 0; i < RPG_MAX_FLAGS; ++i)
        if (g_flags[i].used && strcmp(g_flags[i].key, key) == 0)
            return g_flags[i].val;
    return false;
}
void rpg_var_set(const char* key, double val) {
    for (int i = 0; i < RPG_MAX_VARS; ++i) {
        if (!g_vars[i].used || strcmp(g_vars[i].key, key) == 0) {
            strncpy(g_vars[i].key, key, RPG_KEY_LEN-1);
            g_vars[i].val  = val;
            g_vars[i].used = true;
            return;
        }
    }
}
double rpg_var_get(const char* key) {
    for (int i = 0; i < RPG_MAX_VARS; ++i)
        if (g_vars[i].used && strcmp(g_vars[i].key, key) == 0)
            return g_vars[i].val;
    return 0.0;
}

/* ── セーブファイルパス ──────────────────────────────────*/
static void save_path(int slot, char* buf, size_t n) {
    const char* home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, n, "%s/.hajimu/saves/save_%02d.dat", home, slot);
}

static void ensure_dir(void) {
    const char* home = getenv("HOME");
    if (!home) home = ".";
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/.hajimu/saves", home);
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

/* ── セーブフォーマット ──────────────────────────────────*/
#define SAVE_MAGIC  0x52504753U  /* "SERP" → "RPGS" */
#define SAVE_VER    1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t actor_count;
    uint32_t flag_count;
    uint32_t var_count;
} SaveHeader;

/* アクターは rpg_actor_get で直接アクセスするため、外部リンク利用 */
extern RPG_Actor* rpg_actor_get(int id);

bool rpg_save(int slot) {
    if (slot < 0 || slot >= RPG_SAVE_SLOTS) return false;
    ensure_dir();
    char path[256];
    save_path(slot, path, sizeof(path));
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "[eng_rpg] セーブ失敗: %s\n", path); return false; }

    /* ヘッダー */
    SaveHeader hdr = {
        .magic   = SAVE_MAGIC,
        .version = SAVE_VER,
        .actor_count = RPG_MAX_ACTORS,
        .flag_count  = RPG_MAX_FLAGS,
        .var_count   = RPG_MAX_VARS,
    };
    fwrite(&hdr, sizeof(hdr), 1, f);

    /* アクター */
    for (int i = 1; i <= RPG_MAX_ACTORS; ++i) {
        RPG_Actor* a = rpg_actor_get(i);
        if (a) fwrite(a, sizeof(RPG_Actor), 1, f);
        else   { RPG_Actor empty = {0}; fwrite(&empty, sizeof(empty), 1, f); }
    }

    /* フラグ */
    fwrite(g_flags, sizeof(g_flags), 1, f);

    /* 変数 */
    fwrite(g_vars, sizeof(g_vars), 1, f);

    fclose(f);
    return true;
}

bool rpg_load(int slot) {
    if (slot < 0 || slot >= RPG_SAVE_SLOTS) return false;
    char path[256];
    save_path(slot, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    SaveHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 || hdr.magic != SAVE_MAGIC) {
        fclose(f); return false;
    }

    /* アクター */
    for (int i = 1; i <= RPG_MAX_ACTORS && i <= (int)hdr.actor_count; ++i) {
        RPG_Actor a;
        if (fread(&a, sizeof(a), 1, f) == 1) {
            extern void rpg_actor_set(int id, const RPG_Actor* a);
            rpg_actor_set(i, &a);
        }
    }

    /* フラグ */
    fread(g_flags, sizeof(g_flags), 1, f);

    /* 変数 */
    fread(g_vars, sizeof(g_vars), 1, f);

    fclose(f);
    return true;
}

bool rpg_save_exists(int slot) {
    char path[256];
    save_path(slot, path, sizeof(path));
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void rpg_save_delete(int slot) {
    char path[256];
    save_path(slot, path, sizeof(path));
    remove(path);
}
