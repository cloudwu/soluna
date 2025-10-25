#ifndef SOLUNA_MACOS_IME_H
#define SOLUNA_MACOS_IME_H

#include <stdbool.h>

void soluna_macos_install_ime(void);
void soluna_macos_hide_ime_label(void);
void soluna_macos_apply_ime_rect(void);
void soluna_macos_set_ime_font(const char *font_name, float height_px);
bool soluna_macos_is_composition_active(void);

#endif /* SOLUNA_MACOS_IME_H */
