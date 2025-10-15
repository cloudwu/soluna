#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>
#include <string.h>
#include "mutex.h"

// It's the same with XINPUT_GAMEAD
static const char * gamepad_buttons[] = {
	"UP",
	"DOWN",
	"LEFT",
	"RIGHT",
	"START",
	"BACK",
	"LS",
	"RS",
	"LB",
	"RB",
	"U0",	// Undef
	"U1",	// undef
	"A",
	"B",
	"X",
	"Y",
};

#define BUTTON_COUNT (sizeof(gamepad_buttons)/sizeof(gamepad_buttons[0]))

struct gamepad_state {
	uint32_t packet;
	uint16_t buttons;
	uint8_t lt;
	uint8_t rt;
	int16_t ls_x;
	int16_t ls_y;
	int16_t rs_x;
	int16_t rs_y;
};

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#include <windows.h>
#include <xinput.h>

static int
gamepad_getstate(int index, struct gamepad_state *state) {
	XINPUT_STATE * result = (XINPUT_STATE *)state;
	DWORD err = XInputGetState(index, result);
	if (err == ERROR_SUCCESS)
		return 0;
	// should be ERROR_DEVICE_NOT_CONNECTED
	return 1;
}

#elif defined(__APPLE__)

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <CoreFoundation/CoreFoundation.h>

static IOHIDManagerRef hid_manager = NULL;
static CFMutableArrayRef gamepad_devices = NULL;

static void gamepad_init() {
    if (hid_manager != NULL) return;

    hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    gamepad_devices = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    CFMutableDictionaryRef matching = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    int usage_page = kHIDPage_GenericDesktop;
    int usage = kHIDUsage_GD_GamePad;

    CFNumberRef page_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage_page);
    CFNumberRef usage_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);

    CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsagePageKey), page_ref);
    CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsageKey), usage_ref);

    IOHIDManagerSetDeviceMatching(hid_manager, matching);
    IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone);

    CFSetRef device_set = IOHIDManagerCopyDevices(hid_manager);
    if (device_set) {
        CFIndex count = CFSetGetCount(device_set);
        IOHIDDeviceRef *devices = malloc(count * sizeof(IOHIDDeviceRef));
        CFSetGetValues(device_set, (const void**)devices);

        for (CFIndex i = 0; i < count; i++) {
            CFArrayAppendValue(gamepad_devices, devices[i]);
        }

        free(devices);
        CFRelease(device_set);
    }

    CFRelease(page_ref);
    CFRelease(usage_ref);
    CFRelease(matching);
}

static int
gamepad_getstate(int index, struct gamepad_state *state) {
    gamepad_init();
    
    memset(state, 0, sizeof(struct gamepad_state));
    
    if (index < 0 || index >= CFArrayGetCount(gamepad_devices)) {
        return 1;
    }
    
    IOHIDDeviceRef device = (IOHIDDeviceRef)CFArrayGetValueAtIndex(gamepad_devices, index);
    if (!device) {
        return 1;
    }
    
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
    if (!elements) {
        return 1;
    }
    
    CFIndex element_count = CFArrayGetCount(elements);
    
    for (CFIndex i = 0; i < element_count; i++) {
        IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        IOHIDElementType type = IOHIDElementGetType(element);
        
        if (type == kIOHIDElementTypeInput_Button || 
            type == kIOHIDElementTypeInput_Axis) {
            
            IOHIDValueRef value_ref;
            if (IOHIDDeviceGetValue(device, element, &value_ref) == kIOReturnSuccess) {
                CFIndex value = IOHIDValueGetIntegerValue(value_ref);
                uint32_t usage_page = IOHIDElementGetUsagePage(element);
                uint32_t usage = IOHIDElementGetUsage(element);
                
                if (usage_page == kHIDPage_Button) {
                    if (usage >= 1 && usage <= 16) {
                        if (value) {
                            state->buttons |= (1 << (usage - 1));
                        }
                    }
                }
                else if (usage_page == kHIDPage_GenericDesktop) {
                    CFIndex min = IOHIDElementGetLogicalMin(element);
                    CFIndex max = IOHIDElementGetLogicalMax(element);
                    
                    int16_t normalized = (int16_t)(((value - min) * 65535) / (max - min) - 32768);
                    
                    switch (usage) {
                        case kHIDUsage_GD_X:
                            state->ls_x = normalized;
                            break;
                        case kHIDUsage_GD_Y:
                            state->ls_y = -normalized;
                            break;
                        case kHIDUsage_GD_Z:
                            state->rs_x = normalized;
                            break;
                        case kHIDUsage_GD_Rz:
                            state->rs_y = -normalized;
                            break;
                        case kHIDUsage_GD_Rx:
                            state->lt = (uint8_t)((normalized + 32768) >> 8);
                            break;
                        case kHIDUsage_GD_Ry:
                            state->rt = (uint8_t)((normalized + 32768) >> 8);
                            break;
                    }
                }
            }
        }
    }
    
    CFRelease(elements);
    
    static uint32_t packet_counter = 0;
    state->packet = ++packet_counter;
    
    return 0;
}

#elif defined(__linux__)

#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>

static int
gamepad_getstate(int index, struct gamepad_state *state) {
    char device_path[32];
    snprintf(device_path, sizeof(device_path), "/dev/input/js%d", index);

    int fd = open(device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return 1;
    }

    static struct {
        uint16_t buttons;
        uint8_t lt, rt;
        int16_t ls_x, ls_y, rs_x, rs_y;
        uint32_t packet;
    } cache = {0};

    struct js_event event;

    while (read(fd, &event, sizeof(event)) == sizeof(event)) {
        cache.packet++;

        if (event.type & JS_EVENT_BUTTON) {
            if (event.number < 16) {
                if (event.value) {
                    cache.buttons |= (1 << event.number);
                } else {
                    cache.buttons &= ~(1 << event.number);
                }
            }
        } else if (event.type & JS_EVENT_AXIS) {
            int16_t value = event.value;
            switch (event.number) {
                case 0: cache.ls_x = value; break;
                case 1: cache.ls_y = -value; break;
                case 2: cache.lt = (value + 32768) >> 8; break;
                case 3: cache.rs_x = value; break;
                case 4: cache.rs_y = -value; break;
                case 5: cache.rt = (value + 32768) >> 8; break;
            }
        }
    }

    close(fd);

    state->packet = cache.packet;
    state->buttons = cache.buttons;
    state->lt = cache.lt;
    state->rt = cache.rt;
    state->ls_x = cache.ls_x;
    state->ls_y = cache.ls_y;
    state->rs_x = cache.rs_x;
    state->rs_y = cache.rs_y;

    return 0;
}

#elif defined(__EMSCRIPTEN__)

static int
gamepad_getstate(int index, struct gamepad_state *state) {
	(void)index;
	(void)state;
	return 1;
}

#else

// todo : linux and mac support
#error Unsupport gamepad

#endif

#define MAX_GAMEPAD 4

static struct gamepad_global {
	mutex_t lock[MAX_GAMEPAD];
	int connected[MAX_GAMEPAD];
	uint32_t packet[MAX_GAMEPAD];
	struct gamepad_state state[MAX_GAMEPAD];
} GAMEPAD;

struct gamepad_local {
	int connected;
	struct gamepad_state state;
};

static int
lgamepad_update(lua_State *L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	int index = luaL_optinteger(L, 2, 0);
	if (index < 0 || index >= MAX_GAMEPAD) {
		return luaL_error(L, "Invalid gamepad id %d", index);
	}
	struct gamepad_local * last_state = (struct gamepad_local *)lua_touserdata(L, lua_upvalueindex(1));
	last_state += index;
	struct gamepad_state tmp;
	mutex_acquire(GAMEPAD.lock[index]);
	int connected = GAMEPAD.connected[index];
	if (connected) {
		tmp = GAMEPAD.state[index];
	}
	mutex_release(GAMEPAD.lock[index]);
	int i;
	if (connected != last_state->connected) {
		if (connected) {
			// reconnected, set all states
			last_state->state = tmp;
			lua_pushinteger(L, tmp.lt);
			lua_setfield(L, 1, "LT");
			lua_pushinteger(L, tmp.rt);
			lua_setfield(L, 1, "RT");
			lua_pushinteger(L, tmp.ls_x);
			lua_setfield(L, 1, "LS_X");
			lua_pushinteger(L, tmp.ls_y);
			lua_setfield(L, 1, "LS_Y");
			lua_pushinteger(L, tmp.rs_x);
			lua_setfield(L, 1, "RS_X");
			lua_pushinteger(L, tmp.rs_y);
			uint16_t mask = tmp.buttons;
			for (i=0;i<BUTTON_COUNT;i++) {
				lua_pushboolean(L, mask & 1);
				lua_setfield(L, 1, gamepad_buttons[i]);
				mask >>= 1;
			}
		} else {
			// disconnected, clear all states
			memset(&last_state->state, 0, sizeof(last_state->state));
			lua_pushinteger(L, 0);
			lua_setfield(L, 1, "LT");
			lua_pushinteger(L, 0);
			lua_setfield(L, 1, "RT");
			lua_pushinteger(L, 0);
			lua_setfield(L, 1, "LS_X");
			lua_pushinteger(L, 0);
			lua_setfield(L, 1, "LS_Y");
			lua_pushinteger(L, 0);
			lua_setfield(L, 1, "RS_X");
			for (i=0;i<BUTTON_COUNT;i++) {
				lua_pushboolean(L, 0);
				lua_setfield(L, 1, gamepad_buttons[i]);
			}
		}
	} else if (connected) {
		// state change
		struct gamepad_state *state = &last_state->state;
		if (tmp.lt != state->lt) {
			state->lt = tmp.lt;
			lua_pushinteger(L, tmp.lt);
			lua_setfield(L, 1, "LT");
		}
		if (tmp.rt != state->rt) {
			state->rt = tmp.rt;
			lua_pushinteger(L, tmp.rt);
			lua_setfield(L, 1, "RT");
		}
		if (tmp.ls_x != state->ls_x) {
			state->ls_x = tmp.ls_x;
			lua_pushinteger(L, tmp.ls_x);
			lua_setfield(L, 1, "LS_X");
		}
		if (tmp.ls_y != state->ls_y) {
			state->ls_y = tmp.ls_y;
			lua_pushinteger(L, tmp.ls_y);
			lua_setfield(L, 1, "LS_Y");
		}
		if (tmp.rs_x != state->rs_x) {
			state->rs_x = tmp.rs_x;
			lua_pushinteger(L, tmp.rs_x);
			lua_setfield(L, 1, "RS_X");
		}
		if (tmp.rs_y != state->rs_y) {
			state->rs_y = tmp.rs_y;
			lua_pushinteger(L, tmp.rs_y);
			lua_setfield(L, 1, "RS_Y");
		}
		uint16_t mask = tmp.buttons;
		uint16_t last_mask = state->buttons;
		if (mask != last_mask) {
			for (i=0;i<BUTTON_COUNT;i++) {
				int press = mask & 1;
				if (press != (last_mask & 1)) {
					lua_pushboolean(L, press);
					lua_setfield(L, 1, gamepad_buttons[i]);
				}
				mask >>= 1;
				last_mask >>= 1;
			}
			state->buttons = tmp.buttons;
		}
	}
	lua_pushboolean(L, connected);
	lua_setfield(L, 1, "connect");
	last_state->connected = connected;
	lua_pushboolean(L, connected);
	return 1;
}

int
luaopen_gamepad(lua_State *L) {
	luaL_checkversion(L);
	lua_newtable(L);
	struct gamepad_local * state = (struct gamepad_local *)lua_newuserdatauv(L, sizeof(*state) * MAX_GAMEPAD, 0);
	memset(state, 0, sizeof(*state) * MAX_GAMEPAD);
	lua_pushcclosure(L, lgamepad_update, 1);
	lua_setfield(L, -2, "update");
	return 1;
}

static int
lgamepad_device_init(lua_State *L) {
	int i;
	memset(&GAMEPAD, 0, sizeof(GAMEPAD));
	
	for (i=0;i<MAX_GAMEPAD;i++) {
		mutex_init(GAMEPAD.lock[i]);
	}
	return 0;
}

static int
lgamepad_device_deinit(lua_State *L) {
	memset(&GAMEPAD, 0, sizeof(GAMEPAD));
	return 0;
}

static int
lgamepad_device_update(lua_State *L) {
	int i;
	int changed = 0;
	for (i=0;i<MAX_GAMEPAD;i++) {
		mutex_acquire(GAMEPAD.lock[i]);
			int err = gamepad_getstate(i, &GAMEPAD.state[i]);
			if (err) {
				// disconnected
				if (GAMEPAD.connected[i]) {
					GAMEPAD.connected[i] = 0;
					changed = 1;
				}
			} else {
				// connected
				if (GAMEPAD.connected[i]) {
					if (GAMEPAD.packet[i] != GAMEPAD.state[i].packet) {
						changed = 1;
					}
				} else {
					GAMEPAD.connected[i] = 1;
					changed = 1;
				}
				GAMEPAD.packet[i] = GAMEPAD.state[i].packet;
			}
		mutex_release(GAMEPAD.lock[i]);
	}
	lua_pushboolean(L, changed);
	return 1;
}

int
luaopen_gamepad_device(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "init", lgamepad_device_init },
		{ "deinit", lgamepad_device_deinit },
		{ "update", lgamepad_device_update },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
