#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#include <windowsx.h>
#include <winnls.h>


#include <stdio.h>

#include "sokol/sokol_app.h"

#include "../../ime_state.h"
#include "soluna_windows_ime.h"

static WNDPROC g_soluna_prev_wndproc = NULL;
static BOOL g_soluna_wndproc_installed = FALSE;
static LOGFONTW g_soluna_ime_font;
static BOOL g_soluna_ime_font_valid = FALSE;
static BOOL g_soluna_composition = FALSE;

static void
soluna_win32_set_candidate_position(HIMC imc, LONG caret_x, LONG caret_y, LONG caret_w, LONG caret_h) {
    RECT exclude_rect;
    exclude_rect.left = caret_x;
    exclude_rect.top = caret_y;
    exclude_rect.right = caret_x + caret_w;
    exclude_rect.bottom = caret_y + caret_h;

    MapWindowPoints((HWND)sapp_win32_get_hwnd(), NULL, (LPPOINT)&exclude_rect, 2);

    CANDIDATEFORM cand;
    memset(&cand, 0, sizeof(cand));
    cand.dwIndex = 0;
    cand.dwStyle = CFS_EXCLUDE;
    cand.rcArea = exclude_rect;
    cand.ptCurrentPos.x = exclude_rect.left;
    cand.ptCurrentPos.y = exclude_rect.bottom;
    ImmSetCandidateWindow(imc, &cand);
}

void
soluna_win32_apply_ime_rect(void) {
    HWND hwnd = (HWND)sapp_win32_get_hwnd();
    if (!hwnd) {
        return;
    }
    HIMC imc = ImmGetContext(hwnd);
    if (!imc) {
        fprintf(stderr, "ImmGetContext failed\n");
        return;
    }
    if (g_soluna_ime_rect.valid) {
        float scale = sapp_dpi_scale();
        if (scale <= 0.0f) {
            scale = 1.0f;
        }
        float rect_top = g_soluna_ime_rect.y;
        if (rect_top < 0.0f) {
            rect_top = 0.0f;
        }
        float rect_height = (g_soluna_ime_rect.h > 0.0f ? g_soluna_ime_rect.h : 1.0f);
        float win_height = (float)sapp_height();
        float rect_bottom = rect_top + rect_height;
        if (win_height > 0.0f && rect_bottom > win_height) {
            rect_bottom = win_height;
        }
        float actual_height = rect_bottom - rect_top;
        if (actual_height <= 0.0f) {
            actual_height = 1.0f;
        }

        LONG caret_x = (LONG)(g_soluna_ime_rect.x * scale + 0.5f);
        LONG caret_y = (LONG)(rect_top * scale + 0.5f);
        LONG caret_w = (LONG)((g_soluna_ime_rect.w > 0.0f ? g_soluna_ime_rect.w : 1.0f) * scale + 0.5f);
        LONG caret_h = (LONG)(actual_height * scale + 0.5f);

        COMPOSITIONFORM cf;
        memset(&cf, 0, sizeof(cf));
        cf.dwStyle = CFS_POINT;
        cf.ptCurrentPos.x = caret_x;
        cf.ptCurrentPos.y = caret_y;
        ImmSetCompositionWindow(imc, &cf);

        soluna_win32_set_candidate_position(imc, caret_x, caret_y, caret_w, caret_h);

        if (g_soluna_ime_font_valid) {
            LOGFONTW lf = g_soluna_ime_font;
            if (lf.lfHeight == 0) {
                lf.lfHeight = -(LONG)caret_h;
            }
            ImmSetCompositionFontW(imc, &lf);
        }
    }
    ImmReleaseContext(hwnd, imc);
}

static LRESULT CALLBACK
soluna_win32_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_IME_COMPOSITION:
    case WM_IME_STARTCOMPOSITION:
        g_soluna_composition = TRUE;
        if (g_soluna_ime_rect.valid) {
            soluna_win32_apply_ime_rect();
        }
        break;
    case WM_IME_ENDCOMPOSITION:
        g_soluna_composition = FALSE;
        break;
    case WM_DESTROY:
        g_soluna_composition = FALSE;
        if (g_soluna_prev_wndproc) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)g_soluna_prev_wndproc);
            g_soluna_prev_wndproc = NULL;
            g_soluna_wndproc_installed = FALSE;
        }
        break;
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (g_soluna_composition) {
            return TRUE;
        }
        break;
    default:
        break;
    }
    if (g_soluna_prev_wndproc) {
        return CallWindowProc(g_soluna_prev_wndproc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void
soluna_win32_install_wndproc(void) {
    if (g_soluna_wndproc_installed) {
        return;
    }
    HWND hwnd = (HWND)sapp_win32_get_hwnd();
    if (!hwnd) {
        return;
    }
    WNDPROC prev = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)soluna_win32_wndproc);
    if (prev) {
        g_soluna_prev_wndproc = prev;
        g_soluna_wndproc_installed = TRUE;
    }
}

void
soluna_win32_set_ime_font(const char *font_name, float height_px) {
    float scale = sapp_dpi_scale();
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    LOGFONTW lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    if (height_px > 0.0f) {
        lf.lfHeight = -(LONG)(height_px * scale + 0.5f);
    }
    if (font_name && font_name[0]) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, font_name, -1, NULL, 0);
        if (wlen > 0 && wlen <= (int)(sizeof(lf.lfFaceName) / sizeof(wchar_t))) {
            MultiByteToWideChar(CP_UTF8, 0, font_name, -1, lf.lfFaceName, wlen);
        }
    }
    g_soluna_ime_font = lf;
    g_soluna_ime_font_valid = TRUE;
}

void
soluna_win32_reset_ime_font(void) {
    g_soluna_ime_font_valid = FALSE;
}
