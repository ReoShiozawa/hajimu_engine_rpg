/**
 * src/plugin.c — はじむ RPGエンジン プラグインエントリー
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "hajimu_plugin.h"
#include "eng_rpg.h"
#include <string.h>

/* グローバルバトル・ダイアログをシングルトンで保持 */
static RPG_Battle g_battle;
static RPG_Dialog g_dialog;
static bool g_battle_init = false;
static bool g_dialog_init = false;

static RPG_Dialog* dlg(void) {
    if (!g_dialog_init) { rpg_dialog_init(&g_dialog); g_dialog_init = true; }
    return &g_dialog;
}

/* ── ヘルパーマクロ ─────────────────────────────────────*/
#define ARG_NUM(i) ((i) < argc && args[(i)].type == VALUE_NUMBER ? args[(i)].number : 0.0)
#define ARG_STR(i) ((i) < argc && args[(i)].type == VALUE_STRING  ? args[(i)].string.data : "")
#define ARG_INT(i) ((int)ARG_NUM(i))
#define ARG_F(i)   ((float)ARG_NUM(i))
#define ARG_B(i)   ((i) < argc && args[(i)].type == VALUE_BOOL ? args[(i)].boolean : false)
#define NUM(v)     hajimu_number((double)(v))
#define BVAL(v)    hajimu_bool((bool)(v))
#define NUL        hajimu_null()
#define STR(v)     hajimu_string(v)

/* ── データベース ────────────────────────────────────────*/
static Value fn_キャラ登録(int argc, Value* args) {
    rpg_actor_init(ARG_INT(0), ARG_STR(1),
                   ARG_INT(2), ARG_INT(3),
                   ARG_INT(4), ARG_INT(5), ARG_INT(6));
    return NUL;
}
static Value fn_アイテム登録(int argc, Value* args) {
    rpg_item_init(ARG_INT(0), ARG_STR(1), ARG_STR(2),
                  ARG_INT(3), ARG_INT(4), ARG_INT(5));
    return NUL;
}
static Value fn_スキル登録(int argc, Value* args) {
    rpg_skill_init(ARG_INT(0), ARG_STR(1), ARG_STR(2),
                   ARG_INT(3), ARG_INT(4), ARG_INT(5));
    return NUL;
}
static Value fn_キャラ名取得(int argc, Value* args) {
    RPG_Actor* a = rpg_actor_get(ARG_INT(0));
    return a ? STR(a->name) : STR("");
}
static Value fn_キャラHP取得(int argc, Value* args)     { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->hp:0); }
static Value fn_キャラ最大HP取得(int argc, Value* args) { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->max_hp:0); }
static Value fn_キャラMP取得(int argc, Value* args)     { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->mp:0); }
static Value fn_キャラ最大MP取得(int argc, Value* args) { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->max_mp:0); }
static Value fn_キャラATK取得(int argc, Value* args)    { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->atk:0); }
static Value fn_キャラDEF取得(int argc, Value* args)    { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->def:0); }
static Value fn_キャラSPD取得(int argc, Value* args)    { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->spd:0); }
static Value fn_キャラLv取得(int argc, Value* args)     { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->level:0); }
static Value fn_キャラEXP取得(int argc, Value* args)    { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return NUM(a?a->exp:0); }
static Value fn_キャラ生存確認(int argc, Value* args)   { RPG_Actor* a=rpg_actor_get(ARG_INT(0)); return BVAL(a&&a->alive); }
static Value fn_キャラHP設定(int argc, Value* args) {
    RPG_Actor* a = rpg_actor_get(ARG_INT(0));
    if (a) { a->hp = ARG_INT(1); if(a->hp>a->max_hp) a->hp=a->max_hp; if(a->hp<=0){a->hp=0;a->alive=false;} }
    return NUL;
}
static Value fn_経験値付与(int argc, Value* args) { rpg_gain_exp(ARG_INT(0),ARG_INT(1)); return NUL; }

/* ── インベントリ ────────────────────────────────────────*/
static Value fn_アイテム追加(int argc, Value* args)   { rpg_inventory_add(ARG_INT(0),ARG_INT(1)); return NUL; }
static Value fn_アイテム削除(int argc, Value* args)   { rpg_inventory_remove(ARG_INT(0),ARG_INT(1)); return NUL; }
static Value fn_アイテム所持数(int argc, Value* args) { return NUM(rpg_inventory_count(ARG_INT(0))); }
static Value fn_アイテム所持確認(int argc, Value* args){ return BVAL(rpg_inventory_has(ARG_INT(0))); }
static Value fn_アイテム名取得(int argc, Value* args) {
    RPG_Item* it = rpg_item_get(ARG_INT(0));
    return it ? STR(it->name) : STR("");
}

/* ── バトル ─────────────────────────────────────────────*/
static Value fn_バトル開始(int argc, Value* args) {
    /* 引数: party_id1, party_id2, ..., 0, enemy_id1, ..., 0 */
    int party[RPG_PARTY_MAX+1] = {0};
    int enemy[RPG_PARTY_MAX+1] = {0};
    int pi = 0, ei = 0, i = 0;
    /* args[0..] に enemy 開始インデックス */
    for (i = 0; i < argc && pi < RPG_PARTY_MAX; ++i) {
        int id = ARG_INT(i);
        if (id == 0) { i++; break; }
        party[pi++] = id;
    }
    for (; i < argc && ei < RPG_PARTY_MAX; ++i) {
        int id = ARG_INT(i);
        if (id == 0) break;
        enemy[ei++] = id;
    }
    rpg_battle_init(&g_battle, party, enemy);
    g_battle_init = true;
    return NUL;
}
static Value fn_バトルアクション(int argc, Value* args) {
    if (!g_battle_init) return NUL;
    rpg_battle_do_action(&g_battle, ARG_INT(0),
                         (RPG_ActionType)ARG_INT(1), ARG_INT(2), ARG_INT(3));
    return NUL;
}
static Value fn_バトル状態(int argc, Value* args)   { return NUM(g_battle_init ? g_battle.state : -1); }
static Value fn_バトルメッセージ(int argc, Value* args) {
    return g_battle_init ? STR(g_battle.last_msg) : STR("");
}
static Value fn_バトル次アクター(int argc, Value* args){ return NUM(g_battle_init ? rpg_battle_next_actor(&g_battle) : 0); }
static Value fn_ダメージ計算(int argc, Value* args)  { return NUM(rpg_calc_damage(ARG_INT(0),ARG_INT(1))); }
static Value fn_バトルターン(int argc, Value* args)  { return NUM(g_battle_init ? g_battle.turn : 0); }
static Value fn_最後ダメージ(int argc, Value* args)  { return NUM(g_battle_init ? g_battle.last_damage : 0); }

/* ── ダイアログ ─────────────────────────────────────────*/
static Value fn_メッセージ追加(int argc, Value* args) {
    rpg_dialog_push(dlg(), ARG_STR(0), argc>1?ARG_STR(1):"");
    return NUL;
}
static Value fn_メッセージ更新(int argc, Value* args) {
    rpg_dialog_update(dlg(), ARG_F(0)); return NUL;
}
static Value fn_メッセージ次へ(int argc, Value* args) { rpg_dialog_next(dlg()); return NUL; }
static Value fn_メッセージ空(int argc, Value* args)   { return BVAL(rpg_dialog_empty(dlg())); }
static Value fn_メッセージ速度設定(int argc, Value* args) {
    rpg_dialog_set_speed(dlg(), ARG_F(0)); return NUL;
}
static Value fn_現在メッセージ取得(int argc, Value* args) {
    const RPG_DialogMsg* m = rpg_dialog_current(dlg());
    if (!m) return STR("");
    /* char_pos 文字分だけ返す */
    static char buf[RPG_MSG_MAX_LEN];
    int n = m->char_pos < RPG_MSG_MAX_LEN ? m->char_pos : RPG_MSG_MAX_LEN-1;
    strncpy(buf, m->text, n); buf[n] = '\0';
    return STR(buf);
}
static Value fn_現在話者取得(int argc, Value* args) {
    const RPG_DialogMsg* m = rpg_dialog_current(dlg());
    return m ? STR(m->speaker) : STR("");
}
static Value fn_メッセージ完了(int argc, Value* args) {
    const RPG_DialogMsg* m = rpg_dialog_current(dlg());
    return BVAL(m && m->finished);
}

/* ── フラグ・変数 ────────────────────────────────────────*/
static Value fn_フラグ設定(int argc, Value* args) { rpg_flag_set(ARG_STR(0),ARG_B(1)); return NUL; }
static Value fn_フラグ取得(int argc, Value* args) { return BVAL(rpg_flag_get(ARG_STR(0))); }
static Value fn_変数設定(int argc, Value* args)   { rpg_var_set(ARG_STR(0),ARG_NUM(1)); return NUL; }
static Value fn_変数取得(int argc, Value* args)   { return NUM(rpg_var_get(ARG_STR(0))); }

/* ── セーブ/ロード ────────────────────────────────────────*/
static Value fn_セーブ(int argc, Value* args)        { return BVAL(rpg_save(ARG_INT(0))); }
static Value fn_ロード(int argc, Value* args)        { return BVAL(rpg_load(ARG_INT(0))); }
static Value fn_セーブ存在確認(int argc, Value* args) { return BVAL(rpg_save_exists(ARG_INT(0))); }
static Value fn_セーブ削除(int argc, Value* args)    { rpg_save_delete(ARG_INT(0)); return NUL; }

/* ── ゴールド ────────────────────────────────────────────*/
static Value fn_ゴールド取得(int argc, Value* args) { return NUM(rpg_gold_get()); }
static Value fn_ゴールド設定(int argc, Value* args) { rpg_gold_set(ARG_INT(0)); return NUL; }
static Value fn_ゴールド加算(int argc, Value* args) { rpg_gold_add(ARG_INT(0)); return NUL; }
static Value fn_ゴールド消費(int argc, Value* args) { return BVAL(rpg_gold_spend(ARG_INT(0))); }

/* ── ショップ ────────────────────────────────────────────*/
static Value fn_アイテム購入(int argc, Value* args) { return BVAL(rpg_shop_buy(ARG_INT(0),ARG_INT(1))); }
static Value fn_アイテム売却(int argc, Value* args) { return BVAL(rpg_shop_sell(ARG_INT(0),ARG_INT(1))); }

/* ── 状態異常 ────────────────────────────────────────────*/
static Value fn_状態異常追加(int argc, Value* args) { rpg_actor_add_status(ARG_INT(0),(RPG_Status)ARG_INT(1)); return NUL; }
static Value fn_状態異常取得(int argc, Value* args) { return NUM(rpg_actor_get_status(ARG_INT(0))); }
static Value fn_状態異常確認(int argc, Value* args) { return BVAL(rpg_actor_has_status(ARG_INT(0),(RPG_Status)ARG_INT(1))); }
static Value fn_状態異常回復(int argc, Value* args) { rpg_actor_cure_status(ARG_INT(0),(RPG_Status)ARG_INT(1)); return NUL; }
static Value fn_状態異常全回復(int argc, Value* args){ rpg_actor_cure_all_status(ARG_INT(0)); return NUL; }
static Value fn_状態異常経過(int argc, Value* args) { return NUM(rpg_status_tick(ARG_INT(0))); }

/* ── 選択肢 ─────────────────────────────────────────────*/
/* シングルトン RPG_ChoiceMenu を使う */
RPG_ChoiceMenu* rpg_global_choice(void);  /* eng_extra.c から */
static Value fn_選択肢初期化(int argc, Value* args) { rpg_choice_init(rpg_global_choice()); return NUL; }
static Value fn_選択肢追加(int argc, Value* args)   { rpg_choice_add(rpg_global_choice(), ARG_STR(0)); return NUL; }
static Value fn_選択肢選択(int argc, Value* args)   { rpg_choice_select(rpg_global_choice(), ARG_INT(0)); return NUL; }
static Value fn_選択肢アクティブ(int argc, Value* args){ return BVAL(rpg_choice_is_active(rpg_global_choice())); }
static Value fn_選択済(int argc, Value* args)       { return NUM(rpg_choice_selected(rpg_global_choice())); }
static Value fn_選択肢テキスト(int argc, Value* args){ return STR(rpg_choice_text(rpg_global_choice(), ARG_INT(0))); }
static Value fn_選択肢数(int argc, Value* args)     { return NUM(rpg_choice_count(rpg_global_choice())); }

/* ── 装備 ────────────────────────────────────────────────*/
static Value fn_装備設定(int argc, Value* args)       { rpg_equip_set(ARG_INT(0),(RPG_EquipSlot)ARG_INT(1),ARG_INT(2)); return NUL; }
static Value fn_装備取得(int argc, Value* args)       { return NUM(rpg_equip_get(ARG_INT(0),(RPG_EquipSlot)ARG_INT(1))); }
static Value fn_装備ステータス適用(int argc, Value* args){ rpg_equip_apply_stats(ARG_INT(0)); return NUL; }

/* ── スキル習得/忘却 ────────────────────────────────────*/
static Value fn_スキル習得(int argc, Value* args)      { rpg_actor_learn_skill(ARG_INT(0),ARG_INT(1)); return NUL; }
static Value fn_スキル忘却(int argc, Value* args)      { rpg_actor_forget_skill(ARG_INT(0),ARG_INT(1)); return NUL; }
static Value fn_スキル習得確認(int argc, Value* args)  { return BVAL(rpg_actor_has_skill(ARG_INT(0),ARG_INT(1))); }

/* ── HP/MP 回復 ─────────────────────────────────────────*/
static Value fn_HP回復(int argc, Value* args) { rpg_actor_heal_hp(ARG_INT(0),ARG_INT(1)); return NUL; }
static Value fn_MP回復(int argc, Value* args) { rpg_actor_heal_mp(ARG_INT(0),ARG_INT(1)); return NUL; }

/* ── インベントリ一覧 ────────────────────────────────────
 * インベントリ更新() でキャッシュを構築し件数を返す。
 * その後 インベントリアイテムID(i), インベントリ数量取得(i) で参照。
 */
#define INV_BUF_MAX 64
static int g_inv_ids[INV_BUF_MAX];
static int g_inv_cnts[INV_BUF_MAX];
static int g_inv_len = 0;

static Value fn_インベントリ更新(int argc, Value* args) {
    g_inv_len = rpg_inventory_list(g_inv_ids, g_inv_cnts, INV_BUF_MAX);
    return NUM(g_inv_len);
}
static Value fn_インベントリアイテムID(int argc, Value* args) {
    int i = ARG_INT(0);
    return (i >= 0 && i < g_inv_len) ? NUM(g_inv_ids[i]) : NUM(-1);
}
static Value fn_インベントリ数量取得(int argc, Value* args) {
    int i = ARG_INT(0);
    return (i >= 0 && i < g_inv_len) ? NUM(g_inv_cnts[i]) : NUM(0);
}

/* ── プラグイン登録 ─────────────────────────────────────*/
#define FN(name, mn, mx) { #name, fn_##name, mn, mx }

static HajimuPluginFunc funcs[] = {
    /* データベース */
    FN(キャラ登録,    7, 7), FN(アイテム登録, 6, 6), FN(スキル登録,  6, 6),
    FN(キャラ名取得,  1, 1), FN(キャラHP取得,  1, 1), FN(キャラ最大HP取得, 1, 1),
    FN(キャラMP取得,  1, 1), FN(キャラ最大MP取得, 1, 1),
    FN(キャラATK取得, 1, 1), FN(キャラDEF取得, 1, 1), FN(キャラSPD取得, 1, 1),
    FN(キャラLv取得,  1, 1), FN(キャラEXP取得, 1, 1), FN(キャラ生存確認, 1, 1),
    FN(キャラHP設定,  2, 2), FN(経験値付与,    2, 2),
    /* インベントリ */
    FN(アイテム追加,   2, 2), FN(アイテム削除,   2, 2),
    FN(アイテム所持数, 1, 1), FN(アイテム所持確認, 1, 1), FN(アイテム名取得, 1, 1),
    /* バトル */
    FN(バトル開始,     2, 8), FN(バトルアクション, 4, 4),
    FN(バトル状態,     0, 0), FN(バトルメッセージ, 0, 0),
    FN(バトル次アクター, 0, 0), FN(ダメージ計算, 2, 2),
    FN(バトルターン,   0, 0),   FN(最後ダメージ, 0, 0),
    /* ダイアログ */
    FN(メッセージ追加,   1, 2), FN(メッセージ更新,  1, 1),
    FN(メッセージ次へ,   0, 0), FN(メッセージ空,    0, 0),
    FN(メッセージ速度設定, 1, 1),
    FN(現在メッセージ取得, 0, 0), FN(現在話者取得,  0, 0), FN(メッセージ完了, 0, 0),
    /* フラグ・変数 */
    FN(フラグ設定, 2, 2), FN(フラグ取得, 1, 1),
    FN(変数設定,   2, 2), FN(変数取得,   1, 1),
    /* セーブ/ロード */
    FN(セーブ,   1, 1), FN(ロード,   1, 1),
    FN(セーブ存在確認, 1, 1), FN(セーブ削除, 1, 1),
    /* ゴールド */
    FN(ゴールド取得, 0, 0), FN(ゴールド設定, 1, 1),
    FN(ゴールド加算, 1, 1), FN(ゴールド消費, 1, 1),
    /* ショップ */
    FN(アイテム購入, 2, 2), FN(アイテム売却, 2, 2),
    /* 状態異常 */
    FN(状態異常追加,   2, 2), FN(状態異常取得,   1, 1),
    FN(状態異常確認,   2, 2), FN(状態異常回復,   2, 2),
    FN(状態異常全回復, 1, 1), FN(状態異常経過,   1, 1),
    /* 選択肢 */
    FN(選択肢初期化, 0, 0), FN(選択肢追加, 1, 1), FN(選択肢選択, 1, 1),
    FN(選択肢アクティブ, 0, 0), FN(選択済, 0, 0),
    FN(選択肢テキスト, 1, 1), FN(選択肢数, 0, 0),
    /* 装備 */
    FN(装備設定, 3, 3), FN(装備取得, 2, 2), FN(装備ステータス適用, 1, 1),
    /* スキル習得/忘却 */
    FN(スキル習得,    2, 2), FN(スキル忘却,    2, 2), FN(スキル習得確認, 2, 2),
    /* HP/MP 回復 */
    FN(HP回復, 2, 2), FN(MP回復, 2, 2),
    /* インベントリ一覧 */
    FN(インベントリ更新,      0, 0),
    FN(インベントリアイテムID, 1, 1),
    FN(インベントリ数量取得,   1, 1),
};

HAJIMU_PLUGIN_EXPORT HajimuPluginInfo* hajimu_plugin_init(void) {
    static HajimuPluginInfo info = {
        .name           = "engine_rpg",
        .version        = "1.2.0",
        .author         = "Reo Shiozawa",
        .description    = "はじむ用RPGエンジン (バトル/DB/ダイアログ/セーブ/ゴールド/状態異常/選択肢/装備/スキル/HP回復)",
        .functions      = funcs,
        .function_count = sizeof(funcs) / sizeof(funcs[0]),
    };
    return &info;
}
