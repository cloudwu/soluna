#ifndef soluna_app_event_h
#define soluna_app_event_h

static const char SOLUNA_EVENT_MOUSE_MOVE[] = "mouse_move";
static const char SOLUNA_EVENT_MOUSE_BUTTON[] = "mouse_button";
static const char SOLUNA_EVENT_MOUSE_SCROLL[] = "mouse_scroll";
static const char SOLUNA_EVENT_MOUSE[] = "mouse";
static const char SOLUNA_EVENT_TOUCH_BEGIN[] = "touch_begin";
static const char SOLUNA_EVENT_TOUCH_MOVED[] = "touch_moved";
static const char SOLUNA_EVENT_TOUCH_END[] = "touch_end";
static const char SOLUNA_EVENT_TOUCH_CANCELLED[] = "touch_cancelled";
static const char SOLUNA_EVENT_TOUCH[] = "touch";
static const char SOLUNA_EVENT_WINDOW_RESIZE[] = "window_resize";
static const char SOLUNA_EVENT_WINDOW[] = "window";
static const char SOLUNA_EVENT_CHAR[] = "char";
static const char SOLUNA_EVENT_KEY[] = "key";
static const char SOLUNA_EVENT_CLIPBOARD_PASTED[] = "clipboard_pasted";
static const char SOLUNA_EVENT_MESSAGE[] = "message";

struct event_message {
	const char *typestr;
	const char *str;
	int p1;
	int p2;
	int p3;
};

static inline void
get_xy(struct event_message *em, float x, float y) {
	float dpi_scale = sapp_dpi_scale();
	float inv;
	if (dpi_scale <= 0.0f) {
		dpi_scale = 1.0f;
		inv = 1.0f;
	} else {
		inv = 1.0f / dpi_scale;
	}
	float logical_x = x * inv;
	float logical_y = y * inv;
	if (logical_x >= 0.0f)
		em->p1 = (int)(logical_x + 0.5f);
	else
		em->p1 = (int)(logical_x - 0.5f);
	if (logical_y >= 0.0f)
		em->p2 = (int)(logical_y + 0.5f);
	else
		em->p2 = (int)(logical_y - 0.5f);
}

static inline void
mouse_message(struct event_message *em, const sapp_event* ev) {
	switch (ev->type) {
	case SAPP_EVENTTYPE_MOUSE_MOVE:
		em->typestr = SOLUNA_EVENT_MOUSE_MOVE;
		get_xy(em, ev->mouse_x, ev->mouse_y);
		break;
	case SAPP_EVENTTYPE_MOUSE_DOWN:
	case SAPP_EVENTTYPE_MOUSE_UP:
		em->typestr = SOLUNA_EVENT_MOUSE_BUTTON;
		em->p1 = ev->mouse_button;
		em->p2 = ev->type == SAPP_EVENTTYPE_MOUSE_DOWN;
		break;
	case SAPP_EVENTTYPE_MOUSE_SCROLL:
		em->typestr = SOLUNA_EVENT_MOUSE_SCROLL;
		em->p1 = ev->scroll_y;
		em->p2 = ev->scroll_x;
		break;
	default:
		em->typestr = SOLUNA_EVENT_MOUSE;
		em->p1 = ev->type;
		break;
	}
}

static inline void
touch_message(struct event_message *em, const sapp_event* ev) {
	// todo : support multi touch points
	// get 1st touch point now
    const sapp_touchpoint *t = &ev->touches[0];
	get_xy(em, t->pos_x, t->pos_y);
	em->p3 = t->changed;
	switch (ev->type) {
	case SAPP_EVENTTYPE_TOUCHES_BEGAN:
		em->typestr = SOLUNA_EVENT_TOUCH_BEGIN;
		break;
	case SAPP_EVENTTYPE_TOUCHES_MOVED:
		em->typestr = SOLUNA_EVENT_TOUCH_MOVED;
		break;
	case SAPP_EVENTTYPE_TOUCHES_ENDED:
		em->typestr = SOLUNA_EVENT_TOUCH_END;
		break;
	case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
		em->typestr = SOLUNA_EVENT_TOUCH_CANCELLED;
		break;
	default:
		em->typestr = SOLUNA_EVENT_TOUCH;
		break;
	}
}

static inline void
window_message(struct event_message *em, const sapp_event *ev) {
	switch (ev->type) {
	case SAPP_EVENTTYPE_RESIZED:
		em->typestr = SOLUNA_EVENT_WINDOW_RESIZE;
		em->p1 = ev->window_width;
		em->p2 = ev->window_height;
		break;
	default:
		em->typestr = SOLUNA_EVENT_WINDOW;
		em->p1 = ev->type;
		break;
	}
}

static inline void
key_message(struct event_message *em, const sapp_event *ev) {
	switch (ev->type) {
	case SAPP_EVENTTYPE_CHAR:
		em->typestr = SOLUNA_EVENT_CHAR;
		em->p1 = (int)ev->char_code;
		em->p2 = 0;
		break;
	default:
		em->typestr = SOLUNA_EVENT_KEY;
		em->p1 = (int)ev->key_code;
		em->p2 = ev->type == SAPP_EVENTTYPE_KEY_DOWN;
		break;
	}
}

static inline void
app_event_unpack(struct event_message *em, const sapp_event* ev) {
	em->typestr = NULL;
	em->str = NULL;
	em->p1 = 0;
	em->p2 = 0;
	em->p3 = 0;
	switch (ev->type) {
	case SAPP_EVENTTYPE_MOUSE_MOVE:
	case SAPP_EVENTTYPE_MOUSE_DOWN:
	case SAPP_EVENTTYPE_MOUSE_UP:
	case SAPP_EVENTTYPE_MOUSE_SCROLL:
	case SAPP_EVENTTYPE_MOUSE_ENTER:
	case SAPP_EVENTTYPE_MOUSE_LEAVE:
		mouse_message(em, ev);
		break;
	case SAPP_EVENTTYPE_TOUCHES_BEGAN:
	case SAPP_EVENTTYPE_TOUCHES_MOVED:
	case SAPP_EVENTTYPE_TOUCHES_ENDED:
	case SAPP_EVENTTYPE_TOUCHES_CANCELLED:
		touch_message(em, ev);
		break;
	case SAPP_EVENTTYPE_RESIZED:
		window_message(em, ev);
		break;
	case SAPP_EVENTTYPE_CHAR:
	case SAPP_EVENTTYPE_KEY_DOWN:
	case SAPP_EVENTTYPE_KEY_UP:
		key_message(em, ev);
		break;
	case SAPP_EVENTTYPE_CLIPBOARD_PASTED:
		em->typestr = SOLUNA_EVENT_CLIPBOARD_PASTED;
		em->str = sapp_get_clipboard_string();
		if (em->str == NULL) {
			em->str = "";
		}
		break;
	default:
		em->typestr = SOLUNA_EVENT_MESSAGE;
		em->p1 = ev->type;
		break;
	}
}

#endif
