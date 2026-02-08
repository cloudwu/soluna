#ifndef SOLUNA_IME_STATE_H
#define SOLUNA_IME_STATE_H

#include <stdbool.h>
#include <stdint.h>

struct soluna_ime_rect_state {
    float x;
    float y;
    float w;
    float h;
    uint32_t text_color;
    bool valid;
};

extern struct soluna_ime_rect_state g_soluna_ime_rect;

#endif /* SOLUNA_IME_STATE_H */
