/**
 * src/eng_dialog.c — ダイアログ/メッセージキューシステム
 *
 * Copyright (c) 2026 Reo Shiozawa — MIT License
 */
#include "eng_rpg.h"
#include <string.h>
#include <stdio.h>

void rpg_dialog_init(RPG_Dialog* d) {
    if (!d) return;
    memset(d, 0, sizeof(*d));
}

void rpg_dialog_push(RPG_Dialog* d, const char* text, const char* speaker) {
    if (!d || !text) return;
    if (d->count >= RPG_MSG_QUEUE) {
        fprintf(stderr, "[eng_rpg] ダイアログキュー満杯\n");
        return;
    }
    RPG_DialogMsg* m = &d->queue[d->tail];
    memset(m, 0, sizeof(*m));
    strncpy(m->text, text, RPG_MSG_MAX_LEN - 1);
    if (speaker) strncpy(m->speaker, speaker, 63);
    m->total_chars  = (int)strlen(m->text);
    m->char_interval = 0.03f;
    m->char_pos      = 0;
    m->finished      = false;
    d->tail = (d->tail + 1) % RPG_MSG_QUEUE;
    d->count++;
}

void rpg_dialog_set_speed(RPG_Dialog* d, float secs_per_char) {
    if (!d || d->count == 0) return;
    d->queue[d->head].char_interval = secs_per_char;
}

void rpg_dialog_update(RPG_Dialog* d, float dt) {
    if (!d || d->count == 0) return;
    RPG_DialogMsg* m = &d->queue[d->head];
    if (m->finished) return;

    m->timer += dt;
    while (m->timer >= m->char_interval && m->char_pos < m->total_chars) {
        m->timer -= m->char_interval;
        m->char_pos++;
    }
    if (m->char_pos >= m->total_chars) {
        m->char_pos = m->total_chars;
        m->finished = true;
    }
}

void rpg_dialog_next(RPG_Dialog* d) {
    if (!d || d->count == 0) return;
    RPG_DialogMsg* m = &d->queue[d->head];
    if (!m->finished) {
        /* 全表示 */
        m->char_pos = m->total_chars;
        m->finished = true;
        return;
    }
    /* 次のメッセージへ */
    d->head = (d->head + 1) % RPG_MSG_QUEUE;
    d->count--;
}

const RPG_DialogMsg* rpg_dialog_current(const RPG_Dialog* d) {
    if (!d || d->count == 0) return NULL;
    return &d->queue[d->head];
}

bool rpg_dialog_empty(const RPG_Dialog* d) {
    return !d || d->count == 0;
}
