#ifndef SOLUNA_WINDOWS_IME_H
#define SOLUNA_WINDOWS_IME_H

void soluna_win32_install_wndproc(void);
void soluna_win32_apply_ime_rect(void);
void soluna_win32_set_ime_font(const char *font_name, float height_px);
void soluna_win32_reset_ime_font(void);

#endif /* SOLUNA_WINDOWS_IME_H */
