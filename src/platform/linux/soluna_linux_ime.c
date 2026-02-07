#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <wchar.h>
#include <uchar.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <locale.h>

#include "sokol/sokol_app.h"

#include "ime_state.h"
#include "ime_char_filter.h"
#include "soluna_linux_ime.h"

void soluna_emit_char(uint32_t codepoint, uint32_t modifiers, bool repeat);

static XIM g_soluna_linux_im = NULL;
static XIC g_soluna_linux_ic = NULL;
static bool g_soluna_linux_xim_failed = false;
static bool g_soluna_linux_has_focus = false;
static bool g_soluna_linux_locale_ready = false;

static const int SOLUNA_LINUX_CHAR_QUEUE_CAP = 32;
static uint32_t g_soluna_linux_expected_chars[32];
static int g_soluna_linux_expected_count = 0;
static uint32_t g_soluna_linux_ignore_chars[32];
static int g_soluna_linux_ignore_count = 0;

static inline struct soluna_ime_char_filter_state
soluna_linux_char_filter_state(void) {
    return (struct soluna_ime_char_filter_state) {
        .expected_chars = g_soluna_linux_expected_chars,
        .expected_count = &g_soluna_linux_expected_count,
        .ignore_chars = g_soluna_linux_ignore_chars,
        .ignore_count = &g_soluna_linux_ignore_count,
        .capacity = SOLUNA_LINUX_CHAR_QUEUE_CAP,
    };
}

static void
soluna_linux_reset_char_queues(void) {
    soluna_ime_char_filter_reset(soluna_linux_char_filter_state());
}

static Display *
soluna_linux_display(void) {
    return (Display *)sapp_x11_get_display();
}

static Window
soluna_linux_window(void) {
    return (Window)(uintptr_t)sapp_x11_get_window();
}

static void
soluna_linux_ensure_locale(void) {
    if (g_soluna_linux_locale_ready) {
        return;
    }
    if (!setlocale(LC_CTYPE, "")) {
        fprintf(stderr, "soluna: failed to set locale\n");
    }
    if (!XSupportsLocale()) {
        fprintf(stderr, "soluna: current locale is not supported by Xlib\n");
    }
    XSetLocaleModifiers("");
    g_soluna_linux_locale_ready = true;
}

static void
soluna_linux_set_spot(short x, short y) {
    if (!g_soluna_linux_ic) {
        return;
    }
    XVaNestedList preedit = XVaCreateNestedList(0,
        XNSpotLocation, &(XPoint){ x, y },
        NULL);
    if (!preedit) {
        return;
    }
    XSetICValues(g_soluna_linux_ic, XNPreeditAttributes, preedit, NULL);
    XFree(preedit);
}

bool
soluna_linux_ensure_im(void) {
    if (g_soluna_linux_ic) {
        return true;
    }
    if (g_soluna_linux_xim_failed) {
        return false;
    }
    Display *dpy = soluna_linux_display();
    Window win = soluna_linux_window();
    if (!dpy || !win) {
        return false;
    }
    soluna_linux_ensure_locale();
    XIM im = XOpenIM(dpy, NULL, NULL, NULL);
    if (!im) {
        g_soluna_linux_xim_failed = true;
        return false;
    }
    XIC ic = XCreateIC(
        im,
        XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
        XNClientWindow, win,
        XNFocusWindow, win,
        NULL);
    if (!ic) {
        XCloseIM(im);
        return false;
    }
    g_soluna_linux_im = im;
    g_soluna_linux_ic = ic;
    soluna_linux_set_spot(0, 0);
    return true;
}

void
soluna_linux_update_spot(void) {
    if (!g_soluna_ime_rect.valid) {
        return;
    }
    if (!soluna_linux_ensure_im()) {
        return;
    }
    float scale = sapp_dpi_scale();
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    float caret_x = g_soluna_ime_rect.x;
    float caret_y = g_soluna_ime_rect.y + g_soluna_ime_rect.h;
    if (caret_x < 0.0f) {
        caret_x = 0.0f;
    }
    if (caret_y < 0.0f) {
        caret_y = 0.0f;
    }
    short spot_x = (short)(caret_x * scale + 0.5f);
    short spot_y = (short)(caret_y * scale + 0.5f);
    soluna_linux_set_spot(spot_x, spot_y);
}

static void
soluna_linux_emit_utf8(const char *text, int len, uint32_t mods, bool repeat) {
    if (!text || len <= 0) {
        return;
    }
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    const char *ptr = text;
    const char *end = text + len;
    bool first = true;
    while (ptr < end) {
        char32_t ch = 0;
        size_t consumed = mbrtoc32(&ch, ptr, (size_t)(end - ptr), &state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            memset(&state, 0, sizeof(state));
            ++ptr;
            continue;
        }
        if (consumed == 0) {
            consumed = 1;
        }
        soluna_ime_char_filter_push_expected(soluna_linux_char_filter_state(), (uint32_t)ch);
        soluna_emit_char((uint32_t)ch, mods, first ? repeat : false);
        first = false;
        ptr += consumed;
    }
}

static bool
soluna_linux_handle_keypress(XKeyEvent *kev) {
    if (!kev) {
        return false;
    }
    Display *dpy = soluna_linux_display();
    if (!dpy || kev->display != dpy || kev->window != soluna_linux_window()) {
        return false;
    }
    if (!soluna_linux_ensure_im()) {
        return false;
    }
    char local_buf[128];
    char *buf = local_buf;
    int cap = sizeof(local_buf);
    KeySym ks = 0;
    Status status = 0;
    int len = Xutf8LookupString(g_soluna_linux_ic, kev, buf, cap, &ks, &status);
    if (status == XBufferOverflow) {
        buf = (char *)malloc((size_t)len);
        if (!buf) {
            return false;
        }
        cap = len;
        len = Xutf8LookupString(g_soluna_linux_ic, kev, buf, cap, &ks, &status);
    }
    bool handled = false;
    if (status == XLookupChars || status == XLookupBoth) {
        uint32_t mods = 0;
        if (kev->state & ShiftMask) mods |= SAPP_MODIFIER_SHIFT;
        if (kev->state & ControlMask) mods |= SAPP_MODIFIER_CTRL;
        if (kev->state & Mod1Mask) mods |= SAPP_MODIFIER_ALT;
        bool repeat = (kev->type == KeyPress) && (kev->state & (1 << 14));
        soluna_linux_emit_utf8(buf, len, mods, repeat);
        handled = true;
    }
    if (buf != local_buf) {
        free(buf);
    }
    return handled;
}

static bool
soluna_linux_filter_event(Display *dpy, XEvent *event) {
    if (!event) {
        return true;
    }
    if (XFilterEvent(event, None)) {
        return true;
    }
    if (event->type == KeyPress) {
        if (event->xkey.display != soluna_linux_display() || event->xkey.window != soluna_linux_window()) {
            return false;
        }
        if (soluna_linux_handle_keypress(&event->xkey)) {
            return true;
        }
    }
    return false;
}

static int (*soluna_linux_real_XNextEvent)(Display *, XEvent *) = NULL;

static int
soluna_linux_XNextEvent(Display *display, XEvent *event) {
    if (!soluna_linux_real_XNextEvent) {
        soluna_linux_real_XNextEvent = (int (*)(Display *, XEvent *))dlsym(RTLD_NEXT, "XNextEvent");
        if (!soluna_linux_real_XNextEvent) {
            fprintf(stderr, "soluna: failed to resolve XNextEvent\n");
            abort();
        }
    }
    for (;;) {
        int r = soluna_linux_real_XNextEvent(display, event);
        if (r != 0) {
            return r;
        }
        if (!soluna_linux_filter_event(display, event)) {
            return 0;
        }
    }
}

int
XNextEvent(Display *display, XEvent *event) {
    return soluna_linux_XNextEvent(display, event);
}

static void
soluna_linux_focus_reset(void) {
    soluna_linux_reset_char_queues();
}

void
soluna_linux_focus_in(void) {
    g_soluna_linux_xim_failed = false;
    if (!soluna_linux_ensure_im()) {
        return;
    }
    if (!g_soluna_linux_has_focus) {
        XSetICFocus(g_soluna_linux_ic);
        g_soluna_linux_has_focus = true;
    }
    soluna_linux_update_spot();
}

void
soluna_linux_focus_out(void) {
    if (!g_soluna_linux_ic) {
        return;
    }
    if (g_soluna_linux_has_focus) {
        XUnsetICFocus(g_soluna_linux_ic);
        g_soluna_linux_has_focus = false;
    }
    soluna_linux_focus_reset();
}

void
soluna_linux_shutdown_ime(void) {
    if (g_soluna_linux_ic) {
        XDestroyIC(g_soluna_linux_ic);
        g_soluna_linux_ic = NULL;
    }
    if (g_soluna_linux_im) {
        XCloseIM(g_soluna_linux_im);
        g_soluna_linux_im = NULL;
    }
    g_soluna_linux_has_focus = false;
    g_soluna_linux_xim_failed = false;
    soluna_linux_focus_reset();
}

void
soluna_linux_on_rect_cleared(void) {
    if (soluna_linux_ensure_im()) {
        soluna_linux_set_spot(0, 0);
    }
    soluna_linux_focus_reset();
}

bool
soluna_linux_should_skip_event(const sapp_event *ev) {
    if (!ev || ev->type != SAPP_EVENTTYPE_CHAR) {
        return false;
    }
    return soluna_ime_char_filter_should_skip(soluna_linux_char_filter_state(), ev->char_code);
}

void
soluna_linux_handle_event(const sapp_event *ev) {
    if (!ev) {
        return;
    }
    switch (ev->type) {
    case SAPP_EVENTTYPE_FOCUSED:
        soluna_linux_focus_in();
        if (g_soluna_ime_rect.valid) {
            soluna_linux_update_spot();
        }
        break;
    case SAPP_EVENTTYPE_UNFOCUSED:
        soluna_linux_focus_out();
        break;
    case SAPP_EVENTTYPE_RESIZED:
        if (g_soluna_ime_rect.valid) {
            soluna_linux_update_spot();
        }
        break;
    default:
        break;
    }
}
