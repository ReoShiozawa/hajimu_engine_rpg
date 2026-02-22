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
