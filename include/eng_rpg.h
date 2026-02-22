/**
 * include/eng_rpg.h — はじむ RPGエンジン 公開 API
 *
 * バトルシステム、データベース、ダイアログ、セーブ/ロード、
 * フラグ/変数管理を提供する。
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== データベース ======================== */

/** アクター (プレイヤーキャラ/敵共通) */
typedef struct {
    char    name[64];
    int     hp, max_hp;
    int     mp, max_mp;
    int     atk, def, spd, luk;
    int     level, exp, next_exp;
    bool    alive;
    /* v1.1.0 追加 */
    uint32_t status;      /* RPG_Status フラグ (複合可) */
    int      equip[4];    /* 装備スロット: 0=武器 1=防具 2=兜 3=アクセサリ (item_id, 0=なし) */
} RPG_Actor;

/** アイテム */
typedef struct {
    char    name[64];
    char    desc[128];
    int     type;       /* 0=回復, 1=装備, 2=キーアイテム */
    int     effect;     /* 回復量 / 効果値 */
    int     price;
    bool    used;
} RPG_Item;

/** スキル */
typedef struct {
    char    name[64];
    char    desc[128];
    int     mp_cost;
    int     power;
    int     target;     /* 0=単体敵, 1=全体敵, 2=単体味方, 3=自分 */
    bool    used;
} RPG_Skill;

#define RPG_MAX_ACTORS  64
#define RPG_MAX_ITEMS   256
#define RPG_MAX_SKILLS  128

/* ── アクター DB ────────────────────────────────────────*/
/** アクター登録/更新。id = 1〜MAX_ACTORS。 */
void       rpg_actor_set(int id, const RPG_Actor* a);
RPG_Actor* rpg_actor_get(int id);
/** アクターを初期化して登録 */
void       rpg_actor_init(int id, const char* name,
                           int hp, int mp, int atk, int def, int spd);

/* ── アイテム DB ────────────────────────────────────────*/
void      rpg_item_set(int id, const RPG_Item* it);
RPG_Item* rpg_item_get(int id);
void      rpg_item_init(int id, const char* name, const char* desc,
                         int type, int effect, int price);

/* ── スキル DB ─────────────────────────────────────────*/
void       rpg_skill_set(int id, const RPG_Skill* sk);
RPG_Skill* rpg_skill_get(int id);
void       rpg_skill_init(int id, const char* name, const char* desc,
                           int mp_cost, int power, int target);

/* ======================== インベントリ ======================== */
#define RPG_MAX_INVENTORY 64

void rpg_inventory_add(int item_id, int count);
void rpg_inventory_remove(int item_id, int count);
int  rpg_inventory_count(int item_id);
bool rpg_inventory_has(int item_id);

/* ======================== バトル ======================== */

/** バトルアクション種別 */
typedef enum {
    RPG_ACT_ATTACK  = 0,
    RPG_ACT_SKILL   = 1,
    RPG_ACT_ITEM    = 2,
    RPG_ACT_DEFEND  = 3,
    RPG_ACT_FLEE    = 4,
} RPG_ActionType;

/** バトル結果 */
typedef enum {
    RPG_BATTLE_RUNNING = 0,
    RPG_BATTLE_WIN     = 1,
    RPG_BATTLE_LOSE    = 2,
    RPG_BATTLE_FLED    = 3,
} RPG_BattleState;

/** パーティ/敵のIDリスト最大8(max 4 vs 4) */
#define RPG_PARTY_MAX 4

typedef struct {
    int party[RPG_PARTY_MAX];  /* actor_id; 0 = 空き */
    int enemy[RPG_PARTY_MAX];  /* actor_id; 0 = 空き */
    int party_size;
    int enemy_size;
    int turn;
    RPG_BattleState state;
    int last_damage;
    int last_actor_id;
    int last_target_id;
    char last_msg[128];
} RPG_Battle;

/** バトル初期化。party[]/enemy[] は actor_id の配列、0終端。 */
void             rpg_battle_init(RPG_Battle* b, const int* party, const int* enemy);
/** 1アクションを処理する (actor_id が行動)。 */
void             rpg_battle_do_action(RPG_Battle* b, int actor_id,
                                       RPG_ActionType act, int target_id, int param);
/** 全生存確認を行い state を更新する。 */
RPG_BattleState  rpg_battle_check(RPG_Battle* b);
/** 次のターン (order by spd)。行動するactor_idを返す。 */
int              rpg_battle_next_actor(RPG_Battle* b);

/** ダメージ計算 (ATK vs DEF; 乱数あり)。 */
int rpg_calc_damage(int atk, int def);

/** 経験値獲得・レベルアップ処理。 */
void rpg_gain_exp(int actor_id, int exp);

/* ======================== ダイアログ ======================== */

#define RPG_MSG_MAX_LEN 512
#define RPG_MSG_QUEUE   16

typedef struct {
    char    text[RPG_MSG_MAX_LEN];
    char    speaker[64];
    int     char_pos;      /* 現在表示文字数 */
    int     total_chars;   /* 総文字数 */
    float   timer;         /* 文字送りタイマー */
    float   char_interval; /* 1文字あたり秒数 (デフォルト 0.03) */
    bool    finished;      /* 全文表示済み */
} RPG_DialogMsg;

typedef struct {
    RPG_DialogMsg queue[RPG_MSG_QUEUE];
    int head, tail, count;
} RPG_Dialog;

void rpg_dialog_init(RPG_Dialog* d);
/** メッセージをキューに追加。 */
void rpg_dialog_push(RPG_Dialog* d, const char* text, const char* speaker);
/** 更新。dt=デルタ時間。 */
void rpg_dialog_update(RPG_Dialog* d, float dt);
/** 表示速度を変数で設定 (秒/文字)。 */
void rpg_dialog_set_speed(RPG_Dialog* d, float secs_per_char);
/** 現在メッセージを既読にして次へ進める。 */
void rpg_dialog_next(RPG_Dialog* d);
/** 現在表示中のメッセージを返す (null=空)。 */
const RPG_DialogMsg* rpg_dialog_current(const RPG_Dialog* d);
/** キューが空かどうか。 */
bool rpg_dialog_empty(const RPG_Dialog* d);

/* ======================== フラグ・変数 ======================== */

#define RPG_MAX_FLAGS 256
#define RPG_MAX_VARS  256
#define RPG_KEY_LEN   64

void  rpg_flag_set(const char* key, bool val);
bool  rpg_flag_get(const char* key);
void  rpg_var_set(const char* key, double val);
double rpg_var_get(const char* key);

/* ======================== セーブ/ロード ======================== */

#define RPG_SAVE_SLOTS 9

/**
 * スロットにデータをセーブ。
 * data_type に応じて以下を自動シリアライズ:
 *   "actor"  : RPG_Actorデータ (id_start〜id_end)
 *   "item"   : RPG_Item DB
 *   "flag"   : フラグ全件
 *   "var"    : 変数全件
 * 戻り値: true=成功
 */
bool rpg_save(int slot);

/**
 * スロットからセーブデータを復元。
 * 戻り値: true=成功
 */
bool rpg_load(int slot);

/** セーブデータが存在するか。 */
bool rpg_save_exists(int slot);

/** セーブデータを削除。 */
void rpg_save_delete(int slot);

/* ======================== ゴールド ======================== */

int  rpg_gold_get(void);
void rpg_gold_set(int amount);
void rpg_gold_add(int amount);          /* マイナス値で減算 */
bool rpg_gold_spend(int amount);        /* 所持金が足りない場合 false */

/* ======================== ショップ ======================== */

/** ゴールドでアイテムを購入 (インベントリに追加)。足りない場合 false。 */
bool rpg_shop_buy(int item_id, int count);

/** インベントリのアイテムを売却 (買値の半額)。所持していない場合 false。 */
bool rpg_shop_sell(int item_id, int count);

/* ======================== 状態異常 ======================== */

/** 状態異常フラグ (複合可能) */
typedef enum {
    RPG_STATUS_NONE     = 0,
    RPG_STATUS_POISON   = 1,   /* 毎ターン max_hp の 1/8 ダメージ */
    RPG_STATUS_SLEEP    = 2,   /* 行動不能 (攻撃で解除) */
    RPG_STATUS_CONFUSE  = 4,   /* 行動がランダム */
    RPG_STATUS_PARALYZE = 8,   /* 行動不能 */
    RPG_STATUS_BLIND    = 16,  /* 命中率低下 (ダメージ半減) */
} RPG_Status;

void       rpg_actor_set_status(int id, uint32_t flags);
uint32_t   rpg_actor_get_status(int id);
bool       rpg_actor_has_status(int id, RPG_Status s);
void       rpg_actor_add_status(int id, RPG_Status s);
void       rpg_actor_cure_status(int id, RPG_Status s);
void       rpg_actor_cure_all_status(int id);
/** 毒ダメージなどターン経過処理。ダメージ量を返す (0=無し)。 */
int        rpg_status_tick(int actor_id);

/* ======================== 選択肢 ======================== */

#define RPG_MAX_CHOICES 8

typedef struct {
    char choices[RPG_MAX_CHOICES][64];
    int  count;
    int  selected;   /* -1=未選択 */
    bool active;
} RPG_ChoiceMenu;

void        rpg_choice_init(RPG_ChoiceMenu* m);
void        rpg_choice_add(RPG_ChoiceMenu* m, const char* text);
void        rpg_choice_select(RPG_ChoiceMenu* m, int index);
bool        rpg_choice_is_active(const RPG_ChoiceMenu* m);
int         rpg_choice_selected(const RPG_ChoiceMenu* m);
const char* rpg_choice_text(const RPG_ChoiceMenu* m, int index);
int         rpg_choice_count(const RPG_ChoiceMenu* m);

/* ======================== 装備 ======================== */

typedef enum {
    RPG_EQUIP_WEAPON    = 0,  /* 武器 */
    RPG_EQUIP_ARMOR     = 1,  /* 防具 */
    RPG_EQUIP_HELMET    = 2,  /* 兜 */
    RPG_EQUIP_ACCESSORY = 3,  /* アクセサリ */
} RPG_EquipSlot;

/** スロットにアイテムを装備 (item_id=0 で外す)。 */
void rpg_equip_set(int actor_id, RPG_EquipSlot slot, int item_id);

/** スロットの装備 item_id を取得 (0=なし)。 */
int  rpg_equip_get(int actor_id, RPG_EquipSlot slot);

/**
 * 全装備のeffect値をステータスに反映する。
 * item.type==1(装備) の item.effect を ATK に、
 * item.type==1 で id が奇数なら DEF に加算 (シンプルな例)。
 * 実際のゲームでは独自ロジックで上書き可能。
 */
void rpg_equip_apply_stats(int actor_id);

#ifdef __cplusplus
}
#endif
