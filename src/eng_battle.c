/**
 * src/eng_battle.c — ターン制バトルエンジン
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "eng_rpg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/* ── 乱数ヘルパー ────────────────────────────────────────*/
static bool g_rand_init = false;
static int rpg_rand(int lo, int hi) {
    if (!g_rand_init) { srand((unsigned)time(NULL)); g_rand_init = true; }
    if (lo >= hi) return lo;
    return lo + rand() % (hi - lo + 1);
}

/* ── ダメージ計算 ────────────────────────────────────────*/
int rpg_calc_damage(int atk, int def) {
    int base = atk * 4 - def * 2;
    if (base < 1) base = 1;
    int var = (int)(base * 0.1f);
    return base + rpg_rand(-var, var);
}

/* ── 経験値・レベルアップ ────────────────────────────────*/
void rpg_gain_exp(int actor_id, int exp) {
    RPG_Actor* a = rpg_actor_get(actor_id);
    if (!a) return;
    a->exp += exp;
    while (a->exp >= a->next_exp) {
        a->exp -= a->next_exp;
        a->level++;
        a->next_exp = (int)(a->next_exp * 1.5f);
        /* レベルアップでステータス上昇 */
        int gain_hp = 10 + rpg_rand(0, 10);
        int gain_mp = 5  + rpg_rand(0, 5);
        a->max_hp += gain_hp;
        a->max_mp += gain_mp;
        a->hp     += gain_hp;  /* 全回復 */
        a->mp     += gain_mp;
        a->atk++;
        a->def++;
        a->spd    += rpg_rand(0, 1);
        printf("[RPG] %s Lv.%d → Lv.%d!\n", a->name, a->level-1, a->level);
    }
}

/* ── バトル初期化 ────────────────────────────────────────*/
void rpg_battle_init(RPG_Battle* b, const int* party, const int* enemy) {
    if (!b) return;
    memset(b, 0, sizeof(*b));
    for (int i = 0; i < RPG_PARTY_MAX && party[i]; ++i) {
        b->party[i] = party[i];
        b->party_size++;
    }
    for (int i = 0; i < RPG_PARTY_MAX && enemy[i]; ++i) {
        b->enemy[i] = enemy[i];
        b->enemy_size++;
    }
    b->state = RPG_BATTLE_RUNNING;
    b->turn  = 1;
    snprintf(b->last_msg, sizeof(b->last_msg), "バトル開始！");
}

/* ── アクション処理 ─────────────────────────────────────*/
void rpg_battle_do_action(RPG_Battle* b, int actor_id,
                            RPG_ActionType act, int target_id, int param) {
    if (!b || b->state != RPG_BATTLE_RUNNING) return;
    RPG_Actor* actor = rpg_actor_get(actor_id);
    RPG_Actor* target = rpg_actor_get(target_id);
    if (!actor) return;

    b->last_actor_id  = actor_id;
    b->last_target_id = target_id;
    b->last_damage    = 0;

    switch (act) {
    case RPG_ACT_ATTACK:
        if (!target || !target->alive) {
            snprintf(b->last_msg, sizeof(b->last_msg), "ターゲットが存在しない。");
            break;
        }
        {
            int dmg = rpg_calc_damage(actor->atk, target->def);
            b->last_damage = dmg;
            target->hp -= dmg;
            if (target->hp <= 0) { target->hp = 0; target->alive = false; }
            snprintf(b->last_msg, sizeof(b->last_msg),
                     "%s が %s に %d ダメージ！%s",
                     actor->name, target->name, dmg,
                     target->alive ? "" : " 倒した！");
        }
        break;

    case RPG_ACT_SKILL:
        {
            RPG_Skill* sk = rpg_skill_get(param);
            if (!sk) { snprintf(b->last_msg,sizeof(b->last_msg),"スキルが無い。"); break; }
            if (actor->mp < sk->mp_cost) {
                snprintf(b->last_msg,sizeof(b->last_msg),"MPが足りない！"); break;
            }
            actor->mp -= sk->mp_cost;
            if (target && target->alive) {
                int dmg = rpg_calc_damage(actor->atk + sk->power, target->def);
                b->last_damage = dmg;
                target->hp -= dmg;
                if (target->hp <= 0) { target->hp = 0; target->alive = false; }
                snprintf(b->last_msg, sizeof(b->last_msg),
                         "%s が %s を使用！%s に %d ダメージ！",
                         actor->name, sk->name, target->name, dmg);
            }
        }
        break;

    case RPG_ACT_ITEM:
        {
            RPG_Item* it = rpg_item_get(param);
            if (!it || !rpg_inventory_has(param)) {
                snprintf(b->last_msg,sizeof(b->last_msg),"アイテムが無い！"); break;
            }
            RPG_Actor* tgt = rpg_actor_get(target_id);
            if (tgt && it->type == 0) {
                tgt->hp += it->effect;
                if (tgt->hp > tgt->max_hp) tgt->hp = tgt->max_hp;
                if (!tgt->alive && tgt->hp > 0) tgt->alive = true;
            }
            rpg_inventory_remove(param, 1);
            snprintf(b->last_msg, sizeof(b->last_msg),
                     "%s が %s を使用！", actor->name, it->name);
        }
        break;

    case RPG_ACT_DEFEND:
        snprintf(b->last_msg, sizeof(b->last_msg), "%s は防御した！", actor->name);
        break;

    case RPG_ACT_FLEE:
        b->state = RPG_BATTLE_FLED;
        snprintf(b->last_msg, sizeof(b->last_msg), "逃げ出した！");
        break;
    }

    rpg_battle_check(b);
}

/* ── 生存チェック ────────────────────────────────────────*/
RPG_BattleState rpg_battle_check(RPG_Battle* b) {
    if (!b || b->state != RPG_BATTLE_RUNNING) return b ? b->state : RPG_BATTLE_RUNNING;
    bool party_alive = false, enemy_alive = false;
    for (int i = 0; i < b->party_size; ++i) {
        RPG_Actor* a = rpg_actor_get(b->party[i]);
        if (a && a->alive) { party_alive = true; break; }
    }
    for (int i = 0; i < b->enemy_size; ++i) {
        RPG_Actor* a = rpg_actor_get(b->enemy[i]);
        if (a && a->alive) { enemy_alive = true; break; }
    }
    if (!party_alive) b->state = RPG_BATTLE_LOSE;
    else if (!enemy_alive) b->state = RPG_BATTLE_WIN;
    return b->state;
}

/* ── 次のアクター (SPD順) ──────────────────────────────*/
int rpg_battle_next_actor(RPG_Battle* b) {
    if (!b || b->state != RPG_BATTLE_RUNNING) return 0;
    int best_id = 0, best_spd = -1;
    /* 全参加者から生存者最高速を選択 */
    for (int i = 0; i < b->party_size; ++i) {
        RPG_Actor* a = rpg_actor_get(b->party[i]);
        if (a && a->alive && a->spd > best_spd) {
            best_spd = a->spd; best_id = b->party[i];
        }
    }
    for (int i = 0; i < b->enemy_size; ++i) {
        RPG_Actor* a = rpg_actor_get(b->enemy[i]);
        if (a && a->alive && a->spd > best_spd) {
            best_spd = a->spd; best_id = b->enemy[i];
        }
    }
    b->turn++;
    return best_id;
}
