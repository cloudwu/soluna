#ifndef SOLUNA_LINUX_IME_H
#define SOLUNA_LINUX_IME_H

#include <stdbool.h>

bool soluna_linux_ensure_im(void);
void soluna_linux_on_rect_cleared(void);
void soluna_linux_update_spot(void);
void soluna_linux_focus_in(void);
void soluna_linux_focus_out(void);
void soluna_linux_shutdown_ime(void);
bool soluna_linux_should_skip_event(const sapp_event *ev);
void soluna_linux_handle_event(const sapp_event *ev);

#endif /* SOLUNA_LINUX_IME_H */
