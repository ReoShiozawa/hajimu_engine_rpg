/**
 * src/eng_extra.c — ゴールド / ショップ / 状態異常 / 選択肢 / 装備
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "eng_rpg.h"
#include <string.h>
#include <stdio.h>

/* ======================== ゴールド ======================== */

static int g_gold = 0;

int rpg_gold_get(void)          { return g_gold; }
void rpg_gold_set(int amount)   { g_gold = amount < 0 ? 0 : amount; }
void rpg_gold_add(int amount)   { g_gold += amount; if (g_gold < 0) g_gold = 0; }
bool rpg_gold_spend(int amount) {
    if (amount < 0 || g_gold < amount) return false;
    g_gold -= amount;
    return true;
}

/* ======================== ショップ ======================== */

bool rpg_shop_buy(int item_id, int count) {
    if (count <= 0) return false;
    RPG_Item* it = rpg_item_get(item_id);
    if (!it) return false;
    int total = it->price * count;
    if (!rpg_gold_spend(total)) return false;
    rpg_inventory_add(item_id, count);
    return true;
}

bool rpg_shop_sell(int item_id, int count) {
    if (count <= 0) return false;
    if (rpg_inventory_count(item_id) < count) return false;
    RPG_Item* it = rpg_item_get(item_id);
    int gain = it ? (it->price / 2) * count : 0;
    rpg_inventory_remove(item_id, count);
    rpg_gold_add(gain);
    return true;
}

/* ======================== 状態異常 ======================== */

void rpg_actor_set_status(int id, uint32_t flags) {
    RPG_Actor* a = rpg_actor_get(id);
    if (a) a->status = flags;
}

uint32_t rpg_actor_get_status(int id) {
    RPG_Actor* a = rpg_actor_get(id);
    return a ? a->status : RPG_STATUS_NONE;
}

bool rpg_actor_has_status(int id, RPG_Status s) {
    RPG_Actor* a = rpg_actor_get(id);
    return a ? (a->status & (uint32_t)s) != 0 : false;
}

void rpg_actor_add_status(int id, RPG_Status s) {
    RPG_Actor* a = rpg_actor_get(id);
    if (a) a->status |= (uint32_t)s;
}

void rpg_actor_cure_status(int id, RPG_Status s) {
    RPG_Actor* a = rpg_actor_get(id);
    if (a) a->status &= ~(uint32_t)s;
}

void rpg_actor_cure_all_status(int id) {
    RPG_Actor* a = rpg_actor_get(id);
    if (a) a->status = RPG_STATUS_NONE;
}

int rpg_status_tick(int actor_id) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a || !a->alive) return 0;
    int damage = 0;
    /* 毒: max_hp の 1/8 ダメージ (最低 1) */
    if (a->status & RPG_STATUS_POISON) {
        damage = a->max_hp / 8;
        if (damage < 1) damage = 1;
        a->hp -= damage;
        if (a->hp <= 0) { a->hp = 0; a->alive = false; }
    }
    return damage;
}

/* ======================== 選択肢 ======================== */

void rpg_choice_init(RPG_ChoiceMenu* m) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->selected = -1;
    m->active   = false;
}

void rpg_choice_add(RPG_ChoiceMenu* m, const char* text) {
    if (!m || m->count >= RPG_MAX_CHOICES || !text) return;
    strncpy(m->choices[m->count], text, 63);
    m->choices[m->count][63] = '\0';
    m->count++;
    m->active = true;
}

void rpg_choice_select(RPG_ChoiceMenu* m, int index) {
    if (!m) return;
    if (index >= 0 && index < m->count) {
        m->selected = index;
        m->active   = false;
    }
}

bool rpg_choice_is_active(const RPG_ChoiceMenu* m) {
    return m ? m->active : false;
}

int rpg_choice_selected(const RPG_ChoiceMenu* m) {
    return m ? m->selected : -1;
}

const char* rpg_choice_text(const RPG_ChoiceMenu* m, int index) {
    if (!m || index < 0 || index >= m->count) return "";
    return m->choices[index];
}

int rpg_choice_count(const RPG_ChoiceMenu* m) {
    return m ? m->count : 0;
}

/* ======================== 装備 ======================== */

static RPG_ChoiceMenu g_choice_menu;  /* シングルトン選択肢 */

void rpg_equip_set(int actor_id, RPG_EquipSlot slot, int item_id) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a || slot < 0 || slot >= 4) return;
    a->equip[(int)slot] = item_id;
}

int rpg_equip_get(int actor_id, RPG_EquipSlot slot) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a || slot < 0 || slot >= 4) return 0;
    return a->equip[(int)slot];
}

void rpg_equip_apply_stats(int actor_id) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a) return;
    /* ベースステータスを保持するため、一旦 equip 由来ボーナスを加算するだけ。
     * 実際のゲームでは装備変更時にリセット→再加算する設計が一般的。
     * ここでは シンプルに: スロット0(武器) → ATK加算, スロット1(防具) → DEF加算,
     *                      スロット2(兜)   → DEF加算, スロット3(アクセ)→ LUK加算 */
    for (int s = 0; s < 4; s++) {
        int iid = a->equip[s];
        if (iid == 0) continue;
        RPG_Item* it = rpg_item_get(iid);
        if (!it || it->type != 1) continue;   /* type==1 = 装備品 */
        switch (s) {
            case RPG_EQUIP_WEAPON:    a->atk += it->effect; break;
            case RPG_EQUIP_ARMOR:     a->def += it->effect; break;
            case RPG_EQUIP_HELMET:    a->def += it->effect; break;
            case RPG_EQUIP_ACCESSORY: a->luk += it->effect; break;
        }
    }
}

/* グローバル選択肢メニューへのアクセサ (plugin.c から使用) */
RPG_ChoiceMenu* rpg_global_choice(void) { return &g_choice_menu; }

/* ======================== スキル習得/忘却 ======================== */
/* 64bit マスクでアクターごとのスキル習得状態を管理 (skill_id % 64) */

static uint64_t g_actor_skills[RPG_MAX_ACTORS + 1]; /* [0] 未使用 */

void rpg_actor_learn_skill(int actor_id, int skill_id) {
    if (actor_id < 1 || actor_id > RPG_MAX_ACTORS || skill_id < 0) return;
    g_actor_skills[actor_id] |= (1ULL << (skill_id % 64));
}

void rpg_actor_forget_skill(int actor_id, int skill_id) {
    if (actor_id < 1 || actor_id > RPG_MAX_ACTORS || skill_id < 0) return;
    g_actor_skills[actor_id] &= ~(1ULL << (skill_id % 64));
}

bool rpg_actor_has_skill(int actor_id, int skill_id) {
    if (actor_id < 1 || actor_id > RPG_MAX_ACTORS || skill_id < 0) return false;
    return (g_actor_skills[actor_id] >> (skill_id % 64)) & 1ULL;
}

/* ======================== HP/MP 回復 ======================== */

void rpg_actor_heal_hp(int actor_id, int amount) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a || amount <= 0) return;
    a->hp += amount;
    if (a->hp > a->max_hp) a->hp = a->max_hp;
    if (a->hp > 0) a->alive = true;
}

void rpg_actor_heal_mp(int actor_id, int amount) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a || amount <= 0) return;
    a->mp += amount;
    if (a->mp > a->max_mp) a->mp = a->max_mp;
}

/* ======================== パーティ ======================== */

/* RPG_PARTY_MAX はヘッダーで =4 (Battle最大数) → パーティ管理は独自に8まで */
#define PARTY_MGR_MAX 8
static int  g_party[PARTY_MGR_MAX];
static int  g_party_size = 0;

void rpg_party_clear(void) { g_party_size = 0; }

bool rpg_party_add(int actor_id) {
    if (g_party_size >= PARTY_MGR_MAX) return false;
    for (int i = 0; i < g_party_size; i++)
        if (g_party[i] == actor_id) return false; /* 重複防止 */
    g_party[g_party_size++] = actor_id;
    return true;
}

bool rpg_party_remove(int actor_id) {
    for (int i = 0; i < g_party_size; i++) {
        if (g_party[i] == actor_id) {
            for (int j = i; j < g_party_size - 1; j++)
                g_party[j] = g_party[j + 1];
            g_party_size--;
            return true;
        }
    }
    return false;
}

int  rpg_party_size(void) { return g_party_size; }

int  rpg_party_get(int index) {
    if (index < 0 || index >= g_party_size) return 0;
    return g_party[index];
}

bool rpg_party_has(int actor_id) {
    for (int i = 0; i < g_party_size; i++)
        if (g_party[i] == actor_id) return true;
    return false;
}

/* ======================== アクターステータス設定 ======================== */

bool rpg_actor_set_stat(int id, const char* stat, int value) {
    RPG_Actor* a = rpg_actor_get(id);
    if (!a || !stat) return false;
    if (strcmp(stat, "hp")  == 0) { a->hp  = value; } else
    if (strcmp(stat, "mp")  == 0) { a->mp  = value; } else
    if (strcmp(stat, "atk") == 0) { a->atk = value; } else
    if (strcmp(stat, "def") == 0) { a->def = value; } else
    if (strcmp(stat, "spd") == 0) { a->spd = value; } else
    if (strcmp(stat, "luk") == 0) { a->luk = value; } else
    if (strcmp(stat, "max_hp") == 0) { a->max_hp = value; } else
    if (strcmp(stat, "max_mp") == 0) { a->max_mp = value; } else
    if (strcmp(stat, "level")  == 0) { a->level  = value; } else
    if (strcmp(stat, "exp")    == 0) { a->exp    = value; } else
    { return false; }
    return true;
}

/* ======================== アイテム使用 ======================== */

bool rpg_item_use(int actor_id, int item_id) {
    if (rpg_inventory_count(item_id) <= 0) return false;
    RPG_Item* it = rpg_item_get(item_id);
    if (!it) return false;
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a) return false;
    /* 回復系アイテム (type==0) のみ即座に適用 */
    if (it->type == 0) {
        a->hp += it->effect;
        if (a->hp > a->max_hp) a->hp = a->max_hp;
        if (a->hp > 0 && !a->alive) a->alive = true;
    }
    rpg_inventory_remove(item_id, 1);
    return true;
}

/* ======================== ビジュアルノベルシステム ======================== */

#define NOVEL_CHAR_SLOTS 3
#define NOVEL_BACKLOG_MAX 16

static char g_novel_bg[256];
static char g_novel_char_path[NOVEL_CHAR_SLOTS][256];
static char g_novel_char_expr[NOVEL_CHAR_SLOTS][64];
static bool g_novel_auto      = false;
static bool g_novel_skip      = false;
static float g_novel_auto_delay = 2.0f;

/* バックログ (リングバッファ) */
typedef struct { char speaker[64]; char text[256]; } BacklogEntry;
static BacklogEntry g_backlog[NOVEL_BACKLOG_MAX];
static int g_backlog_count = 0;
static int g_backlog_head  = 0;  /* 次書き込み位置 */

void rpg_novel_set_bg(const char* path) {
    if (path) snprintf(g_novel_bg, sizeof(g_novel_bg), "%s", path);
    else g_novel_bg[0] = '\0';
}
const char* rpg_novel_get_bg(void) { return g_novel_bg; }

void rpg_novel_set_char(int slot, const char* path, const char* expr) {
    if (slot < 0 || slot >= NOVEL_CHAR_SLOTS) return;
    snprintf(g_novel_char_path[slot], 256, "%s", path ? path : "");
    snprintf(g_novel_char_expr[slot], 64,  "%s", expr ? expr : "");
}
const char* rpg_novel_get_char_path(int slot) {
    if (slot < 0 || slot >= NOVEL_CHAR_SLOTS) return "";
    return g_novel_char_path[slot];
}
const char* rpg_novel_get_char_expr(int slot) {
    if (slot < 0 || slot >= NOVEL_CHAR_SLOTS) return "";
    return g_novel_char_expr[slot];
}
void rpg_novel_clear_char(int slot) {
    if (slot < 0 || slot >= NOVEL_CHAR_SLOTS) return;
    g_novel_char_path[slot][0] = '\0';
    g_novel_char_expr[slot][0] = '\0';
}

void rpg_novel_set_auto(bool on)          { g_novel_auto = on; }
bool rpg_novel_get_auto(void)             { return g_novel_auto; }
void rpg_novel_set_skip(bool on)          { g_novel_skip = on; }
bool rpg_novel_get_skip(void)             { return g_novel_skip; }
void rpg_novel_set_auto_delay(float sec)  { g_novel_auto_delay = sec; }
float rpg_novel_get_auto_delay(void)      { return g_novel_auto_delay; }

/* バックログ */
void rpg_novel_backlog_push(const char* speaker, const char* text) {
    BacklogEntry* e = &g_backlog[g_backlog_head];
    snprintf(e->speaker, 64,  "%s", speaker ? speaker : "");
    snprintf(e->text,    256, "%s", text    ? text    : "");
    g_backlog_head = (g_backlog_head + 1) % NOVEL_BACKLOG_MAX;
    if (g_backlog_count < NOVEL_BACKLOG_MAX) g_backlog_count++;
}

int rpg_novel_backlog_count(void) { return g_backlog_count; }

static int backlog_index(int i) {
    /* i=0 が最新。リングバッファの古い順を逆引き */
    int start = (g_backlog_head - g_backlog_count + NOVEL_BACKLOG_MAX) % NOVEL_BACKLOG_MAX;
    return (start + i) % NOVEL_BACKLOG_MAX;
}

const char* rpg_novel_backlog_speaker(int i) {
    if (i < 0 || i >= g_backlog_count) return "";
    return g_backlog[backlog_index(i)].speaker;
}
const char* rpg_novel_backlog_text(int i) {
    if (i < 0 || i >= g_backlog_count) return "";
    return g_backlog[backlog_index(i)].text;
}
