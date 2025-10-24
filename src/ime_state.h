#ifndef SOLUNA_IME_STATE_H
#define SOLUNA_IME_STATE_H

#include <stdbool.h>

struct soluna_ime_rect_state {
    float x;
    float y;
    float w;
    float h;
    bool valid;
};

extern struct soluna_ime_rect_state g_soluna_ime_rect;

#endif /* SOLUNA_IME_STATE_H */
