#ifndef SOLUNA_IME_CHAR_FILTER_H
#define SOLUNA_IME_CHAR_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct soluna_ime_char_filter_state {
    uint32_t *expected_chars;
    int *expected_count;
    uint32_t *ignore_chars;
    int *ignore_count;
    int capacity;
};

static inline void
soluna_ime_char_queue_push(uint32_t *buffer, int *count, int max, uint32_t code) {
    if (*count == max) {
        memmove(buffer, buffer + 1, (size_t)(max - 1) * sizeof(uint32_t));
        buffer[max - 1] = code;
    } else {
        buffer[*count] = code;
        (*count)++;
    }
}

static inline bool
soluna_ime_char_queue_consume(uint32_t *buffer, int *count, uint32_t code) {
    for (int i = 0; i < *count; ++i) {
        if (buffer[i] == code) {
            if (i < *count - 1) {
                memmove(buffer + i, buffer + i + 1, (size_t)(*count - i - 1) * sizeof(uint32_t));
            }
            (*count)--;
            return true;
        }
    }
    return false;
}

static inline void
soluna_ime_char_filter_reset(struct soluna_ime_char_filter_state state) {
    *state.expected_count = 0;
    *state.ignore_count = 0;
}

static inline void
soluna_ime_char_filter_push_expected(struct soluna_ime_char_filter_state state, uint32_t code) {
    soluna_ime_char_queue_push(state.expected_chars, state.expected_count, state.capacity, code);
}

static inline bool
soluna_ime_char_filter_should_skip(struct soluna_ime_char_filter_state state, uint32_t code) {
    if (soluna_ime_char_queue_consume(state.expected_chars, state.expected_count, code)) {
        soluna_ime_char_queue_push(state.ignore_chars, state.ignore_count, state.capacity, code);
        return false;
    }
    if (*state.ignore_count > 0) {
        if (soluna_ime_char_queue_consume(state.ignore_chars, state.ignore_count, code)) {
            return true;
        } else if (*state.ignore_count > 0) {
            uint32_t stale = state.ignore_chars[0];
            soluna_ime_char_queue_consume(state.ignore_chars, state.ignore_count, stale);
        }
    }
    return false;
}

#endif /* SOLUNA_IME_CHAR_FILTER_H */
