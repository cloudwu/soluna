#ifndef SOLUNA_WASM_IME_H
#define SOLUNA_WASM_IME_H

#if defined(__EMSCRIPTEN__)

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#if defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#endif
#include <wchar.h>
#include <uchar.h>
#include <locale.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "../../ime_state.h"

void soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat);
extern void soluna_wasm_setup_ime(void);
extern void soluna_wasm_dom_show(float x, float y, float w, float h);
extern void soluna_wasm_dom_hide(void);
extern void soluna_wasm_dom_set_font(const char *name, float size);

static const int SOLUNA_WASM_CHAR_QUEUE_CAP = 32;
static uint32_t g_soluna_wasm_expected_chars[32];
static int g_soluna_wasm_expected_count = 0;
static uint32_t g_soluna_wasm_ignore_chars[32];
static int g_soluna_wasm_ignore_count = 0;
static bool g_soluna_wasm_composing = false;
static bool g_soluna_wasm_locale_ready = false;
static int g_soluna_wasm_block_keys = 0;

static void
soluna_wasm_char_queue_push(uint32_t *buffer, int *count, int max, uint32_t code) {
    if (*count == max) {
        memmove(buffer, buffer + 1, (size_t)(max - 1) * sizeof(uint32_t));
        buffer[max - 1] = code;
    } else {
        buffer[*count] = code;
        (*count)++;
    }
}

static bool
soluna_wasm_char_queue_consume(uint32_t *buffer, int *count, uint32_t code) {
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

static void
soluna_wasm_call_setup(void) {
#if defined(__EMSCRIPTEN_PTHREADS__)
    if (!emscripten_is_main_browser_thread()) {
        emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_V, soluna_wasm_setup_ime);
        return;
    }
#endif
    soluna_wasm_setup_ime();
}

static void
soluna_wasm_call_show(float x, float y, float w, float h) {
#if defined(__EMSCRIPTEN_PTHREADS__)
    if (!emscripten_is_main_browser_thread()) {
        emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VFFFF, soluna_wasm_dom_show, x, y, w, h);
        return;
    }
#endif
    soluna_wasm_dom_show(x, y, w, h);
}

static void
soluna_wasm_call_hide(void) {
#if defined(__EMSCRIPTEN_PTHREADS__)
    if (!emscripten_is_main_browser_thread()) {
        emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_V, soluna_wasm_dom_hide);
        return;
    }
#endif
    soluna_wasm_dom_hide();
}

static void
soluna_wasm_call_set_font(const char *name, float size) {
#if defined(__EMSCRIPTEN_PTHREADS__)
    if (!emscripten_is_main_browser_thread()) {
        emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VIF, soluna_wasm_dom_set_font, (intptr_t)name, size);
        return;
    }
#endif
    soluna_wasm_dom_set_font(name, size);
}

static void
soluna_wasm_reset_queues(void) {
    g_soluna_wasm_expected_count = 0;
    g_soluna_wasm_ignore_count = 0;
}

void
soluna_wasm_set_font(const char *name, float size) {
    soluna_wasm_call_setup();
    soluna_wasm_call_set_font(name, size);
}

static void
soluna_wasm_ensure_locale(void) {
    if (g_soluna_wasm_locale_ready) {
        return;
    }
    if (!setlocale(LC_CTYPE, "C.UTF-8")) {
        if (!setlocale(LC_CTYPE, "en_US.UTF-8")) {
            setlocale(LC_CTYPE, "C");
        }
    }
    g_soluna_wasm_locale_ready = true;
}

static void
soluna_wasm_emit_utf8(const char *text, uint32_t mods) {
    if (!text || text[0] == '\0') {
        return;
    }
    soluna_wasm_ensure_locale();
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    const char *ptr = text;
    while (*ptr) {
        char32_t ch = 0;
        size_t consumed = mbrtoc32(&ch, ptr, MB_CUR_MAX, &state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            memset(&state, 0, sizeof(state));
            ++ptr;
            continue;
        }
        if (consumed == 0) {
            consumed = 1;
        }
        soluna_wasm_char_queue_push(g_soluna_wasm_expected_chars, &g_soluna_wasm_expected_count, SOLUNA_WASM_CHAR_QUEUE_CAP, (uint32_t)ch);
        soluna_emit_char((uint32_t)ch, mods, false);
        ptr += consumed;
    }
}

static inline bool
soluna_wasm_is_composing(void) {
    return g_soluna_wasm_composing;
}

void
soluna_wasm_hide(void) {
    soluna_wasm_reset_queues();
    soluna_wasm_call_hide();
    g_soluna_wasm_composing = false;
}

void
soluna_wasm_apply_rect(void) {
    if (!g_soluna_ime_rect.valid) {
        soluna_wasm_hide();
        return;
    }
    soluna_wasm_call_setup();
    soluna_wasm_call_show(g_soluna_ime_rect.x, g_soluna_ime_rect.y, g_soluna_ime_rect.w, g_soluna_ime_rect.h);
}

static inline bool
soluna_wasm_should_block_key_event(const sapp_event *ev) {
    if (!ev) {
        return false;
    }
    bool is_key_event = ev->type == SAPP_EVENTTYPE_KEY_DOWN || ev->type == SAPP_EVENTTYPE_KEY_UP;
    if (g_soluna_wasm_block_keys > 0 && is_key_event) {
        --g_soluna_wasm_block_keys;
        return true;
    }
    if (!soluna_wasm_is_composing()) {
        return false;
    }
    return is_key_event;
}

static inline bool
soluna_wasm_filter_char_event(const sapp_event *ev) {
    if (!ev || ev->type != SAPP_EVENTTYPE_CHAR) {
        return false;
    }
    if (soluna_wasm_char_queue_consume(g_soluna_wasm_expected_chars, &g_soluna_wasm_expected_count, ev->char_code)) {
        soluna_wasm_char_queue_push(g_soluna_wasm_ignore_chars, &g_soluna_wasm_ignore_count, SOLUNA_WASM_CHAR_QUEUE_CAP, ev->char_code);
        return false;
    }
    if (g_soluna_wasm_ignore_count > 0) {
        if (soluna_wasm_char_queue_consume(g_soluna_wasm_ignore_chars, &g_soluna_wasm_ignore_count, ev->char_code)) {
            return true;
        } else if (g_soluna_wasm_ignore_count > 0) {
            uint32_t stale = g_soluna_wasm_ignore_chars[0];
            soluna_wasm_char_queue_consume(g_soluna_wasm_ignore_chars, &g_soluna_wasm_ignore_count, stale);
        }
    }
    return false;
}

static inline void
soluna_wasm_handle_event(const sapp_event *ev) {
    if (!ev) {
        return;
    }
    switch (ev->type) {
    case SAPP_EVENTTYPE_UNFOCUSED:
        soluna_wasm_hide();
        break;
    case SAPP_EVENTTYPE_FOCUSED:
    case SAPP_EVENTTYPE_RESIZED:
        if (g_soluna_ime_rect.valid) {
            soluna_wasm_apply_rect();
        }
        break;
    default:
        break;
    }
}

EMSCRIPTEN_KEEPALIVE void
soluna_wasm_ime_commit(const char *text, int modifiers) {
    soluna_wasm_emit_utf8(text, (uint32_t)modifiers);
}

EMSCRIPTEN_KEEPALIVE void
soluna_wasm_set_composing(int active) {
    g_soluna_wasm_composing = (active != 0);
}

EMSCRIPTEN_KEEPALIVE void
soluna_wasm_block_next_keypair(void) {
    g_soluna_wasm_block_keys = 2;
}

#endif /* __EMSCRIPTEN__ */

#endif /* SOLUNA_WASM_IME_H */
