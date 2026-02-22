/**
 * src/eng_db.c — アクター/アイテム/スキル DB + インベントリ
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "eng_rpg.h"
#include <string.h>
#include <stdio.h>

/* ── グローバルデータベース ─────────────────────────────*/
static RPG_Actor  g_actors[RPG_MAX_ACTORS + 1];  /* [0] 未使用, [1..MAX] */
static RPG_Item   g_items[RPG_MAX_ITEMS  + 1];
static RPG_Skill  g_skills[RPG_MAX_SKILLS + 1];

typedef struct { int item_id; int count; } InvEntry;
static InvEntry g_inv[RPG_MAX_INVENTORY];

/* ── アクター ────────────────────────────────────────────*/
void rpg_actor_set(int id, const RPG_Actor* a) {
    if (id < 1 || id > RPG_MAX_ACTORS || !a) return;
    g_actors[id] = *a;
}
RPG_Actor* rpg_actor_get(int id) {
    if (id < 1 || id > RPG_MAX_ACTORS) return NULL;
    return &g_actors[id];
}
void rpg_actor_init(int id, const char* name,
                    int hp, int mp, int atk, int def, int spd) {
    if (id < 1 || id > RPG_MAX_ACTORS) return;
    RPG_Actor* a = &g_actors[id];
    memset(a, 0, sizeof(*a));
    strncpy(a->name, name, 63);
    a->hp = a->max_hp = hp;
    a->mp = a->max_mp = mp;
    a->atk = atk; a->def = def; a->spd = spd; a->luk = 10;
    a->level = 1; a->exp = 0; a->next_exp = 100;
    a->alive = true;
}

/* ── アイテム ────────────────────────────────────────────*/
void rpg_item_set(int id, const RPG_Item* it) {
    if (id < 1 || id > RPG_MAX_ITEMS || !it) return;
    g_items[id] = *it; g_items[id].used = true;
}
RPG_Item* rpg_item_get(int id) {
    if (id < 1 || id > RPG_MAX_ITEMS || !g_items[id].used) return NULL;
    return &g_items[id];
}
void rpg_item_init(int id, const char* name, const char* desc,
                    int type, int effect, int price) {
    if (id < 1 || id > RPG_MAX_ITEMS) return;
    RPG_Item* it = &g_items[id];
    memset(it, 0, sizeof(*it));
    strncpy(it->name, name, 63);
    strncpy(it->desc, desc, 127);
    it->type = type; it->effect = effect; it->price = price;
    it->used = true;
}

/* ── スキル ─────────────────────────────────────────────*/
void rpg_skill_set(int id, const RPG_Skill* sk) {
    if (id < 1 || id > RPG_MAX_SKILLS || !sk) return;
    g_skills[id] = *sk; g_skills[id].used = true;
}
RPG_Skill* rpg_skill_get(int id) {
    if (id < 1 || id > RPG_MAX_SKILLS || !g_skills[id].used) return NULL;
    return &g_skills[id];
}
void rpg_skill_init(int id, const char* name, const char* desc,
                     int mp_cost, int power, int target) {
    if (id < 1 || id > RPG_MAX_SKILLS) return;
    RPG_Skill* sk = &g_skills[id];
    memset(sk, 0, sizeof(*sk));
    strncpy(sk->name, name, 63);
    strncpy(sk->desc, desc, 127);
    sk->mp_cost = mp_cost; sk->power = power; sk->target = target;
    sk->used = true;
}

/* ── インベントリ ────────────────────────────────────────*/
void rpg_inventory_add(int item_id, int count) {
    for (int i = 0; i < RPG_MAX_INVENTORY; ++i) {
        if (g_inv[i].item_id == item_id) { g_inv[i].count += count; return; }
    }
    for (int i = 0; i < RPG_MAX_INVENTORY; ++i) {
        if (g_inv[i].item_id == 0) { g_inv[i].item_id = item_id; g_inv[i].count = count; return; }
    }
    fprintf(stderr, "[eng_rpg] インベントリ満杯\n");
}
void rpg_inventory_remove(int item_id, int count) {
    for (int i = 0; i < RPG_MAX_INVENTORY; ++i) {
        if (g_inv[i].item_id == item_id) {
            g_inv[i].count -= count;
            if (g_inv[i].count <= 0) { g_inv[i].item_id = 0; g_inv[i].count = 0; }
            return;
        }
    }
}
int rpg_inventory_count(int item_id) {
    for (int i = 0; i < RPG_MAX_INVENTORY; ++i)
        if (g_inv[i].item_id == item_id) return g_inv[i].count;
    return 0;
}
bool rpg_inventory_has(int item_id) { return rpg_inventory_count(item_id) > 0; }
