#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "yoga/Yoga.h"

#define FlexDirection 1
#define Justify 2
#define Align 4
#define Wrap 8
#define Display 16
#define PositionType 32

#define ENUM(x, what) { YG##x##ToString(YG##x##what), (x) << 16 | (YG##x##what) },

struct enum_string {
	const char * name;
	int value;
};

struct set_number {
	void (*set)(YGNodeRef node, float width);
	void (*setPercent)(YGNodeRef node, float width);
	void (*setAuto)(YGNodeRef node);
	void (*setMaxContent)(YGNodeRef node);
	void (*setFitContent)(YGNodeRef node);
	void (*setStretch)(YGNodeRef node);
};

struct set_edge_number {
	void (*set)(YGNodeRef node, YGEdge edge, float v);
	void (*setPercent)(YGNodeRef node, YGEdge edge, float v);
	void (*setAuto)(YGNodeRef node, YGEdge edge);
};

struct set_two_number {
	void (*set)(YGNodeRef node, YGGutter gutter, float v);
	void (*setPercent)(YGNodeRef node, YGGutter gutter, float v);
};

static int
lnodeNew(lua_State *L) {
	YGNodeRef node = YGNodeNew();
	if (lua_islightuserdata(L, 1)) {
		YGNodeRef parent = lua_touserdata(L, 1);
		size_t n = YGNodeGetChildCount(parent);
		YGNodeInsertChild(parent, node, n);
	}
	lua_pushlightuserdata(L, node);
	return 1;
}

static int
lnodeFree(lua_State *L) {
	YGNodeRef node = lua_touserdata(L, 1);
	YGNodeFreeRecursive(node);
	return 0;
}

static int
lnodeRemove(lua_State *L) {
	YGNodeRef parent = lua_touserdata(L, 1);
	YGNodeRef node = lua_touserdata(L, 2);
	YGNodeRemoveChild(parent, node);
	return 0;
}

static int
lnodeCalc(lua_State *L) {
	YGNodeRef node = lua_touserdata(L, 1);
	YGNodeCalculateLayout(node, YGUndefined, YGUndefined, YGDirectionLTR);
	return 0;
}

struct pos {
	float x;
	float y;
};

static void
get_pos(struct pos *p, YGNodeRef node) {
	p->x = 0;
	p->y = 0;
	
	while (node) {
		p->x += YGNodeLayoutGetLeft(node);
		p->y += YGNodeLayoutGetTop(node);
		node = YGNodeGetParent(node);
	}
}

static int
lnodeGet(lua_State *L) {
	YGNodeRef node = lua_touserdata(L, 1);
	struct pos p;
	get_pos(&p, node);
	float r[] = {
		p.x,
		p.y,
		YGNodeLayoutGetWidth(node),
		YGNodeLayoutGetHeight(node)
	};
	int i;
	for (i=0;i<4;i++) {
		lua_pushnumber(L, r[i]);
	}
	return 4;
}

typedef void (*setfunc)(lua_State *L, YGNodeRef node);

static inline int
is_whitespace(char c) {
	return c =='\0' || c == ' ' || c == '\t';
}

static void
setNumberString(lua_State *L, YGNodeRef node, const char *v, const struct set_number *setter) {
	char* endptr = NULL;
	float number = strtof(v, &endptr);
	if (*endptr == '%') {
		setter->setPercent(node, number);
	} else if (is_whitespace(*endptr)) {
		setter->set(node, number);
	} else if (setter->setAuto && strcmp(v, "auto") == 0) {
		setter->setAuto(node);
	} else if (strcmp(v, "stretch") == 0) {
		setter->setStretch(node);
	} else if (strcmp(v, "max-content") == 0) {
		setter->setMaxContent(node);
	} else if (strcmp(v, "fit-content") == 0) {
		setter->setFitContent(node);
	} else {
		luaL_error(L, "Invalid number %s", v);
	}
}

static void
setNumber(lua_State *L, YGNodeRef node, const struct set_number *setter) {
	if (lua_type(L, -1) == LUA_TNUMBER) {
		float v = lua_tonumber(L, -1);
		setter->set(node, v);
	} else {
		const char * v = luaL_checkstring(L, -1);
		setNumberString(L, node, v, setter);
	}
}

static void
lsetWidth(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetWidth,
		YGNodeStyleSetWidthPercent,
		YGNodeStyleSetWidthAuto,
		YGNodeStyleSetWidthMaxContent,
		YGNodeStyleSetWidthFitContent,
	};
	setNumber(L, node, &setter);
}

static void
lsetMinWidth(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetMinWidth,
		YGNodeStyleSetMinWidthPercent,
		NULL,
		YGNodeStyleSetMinWidthMaxContent,
		YGNodeStyleSetMinWidthFitContent,
	};
	setNumber(L, node, &setter);
}

static void
lsetMaxWidth(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetMaxWidth,
		YGNodeStyleSetMaxWidthPercent,
		NULL,
		YGNodeStyleSetMaxWidthMaxContent,
		YGNodeStyleSetMaxWidthFitContent,
	};
	setNumber(L, node, &setter);
}

static void
lsetHeight(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetHeight,
		YGNodeStyleSetHeightPercent,
		YGNodeStyleSetHeightAuto,
		YGNodeStyleSetHeightMaxContent,
		YGNodeStyleSetHeightFitContent,
	};
	setNumber(L, node, &setter);
}

static void
lsetMinHeight(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetMinHeight,
		YGNodeStyleSetMinHeightPercent,
		NULL,
		YGNodeStyleSetMinHeightMaxContent,
		YGNodeStyleSetMinHeightFitContent,
	};
	setNumber(L, node, &setter);
}

static void
lsetMaxHeight(lua_State *L, YGNodeRef node) {
	static const struct set_number setter = {
		YGNodeStyleSetMaxHeight,
		YGNodeStyleSetMaxHeightPercent,
		NULL,
		YGNodeStyleSetMaxHeightMaxContent,
		YGNodeStyleSetMaxHeightFitContent,
	};
	setNumber(L, node, &setter);
}

static const char *
skip_whitespace(const char *v) {
	while(*v == ' ' || *v == '\t') {
		++v;
	}
	return v;
}

static int
count_words(const char *v) {
	int n = 0;
	do {
		v = skip_whitespace(v);
		if (*v != '\0') {
			++n;
			while (!is_whitespace(*v))
				++v;
		}
	} while (*v != '\0');
	return n;
}

static const char *
setEdgeNumber(lua_State *L, YGNodeRef node, YGEdge edge, const char *v, const struct set_edge_number *setter) {
	v = skip_whitespace(v);
	char* endptr = NULL;
	float number = strtof(v, &endptr);
	if (is_whitespace(*endptr)) {
		setter->set(node, edge, number);
		return endptr;
	} else if (setter->setPercent && *endptr == '%') {
		setter->setPercent(node, edge, number);
		return endptr+1;
	} else if (setter->setAuto && memcmp("auto", v, 4) == 0) {
		if (!is_whitespace(v[4]))
			luaL_error(L, "Invalid number %s", v);
		setter->setAuto(node, edge);
		return v + 4;
	} else {
		luaL_error(L, "Invalid number %s", v);
	}
	return NULL;
}

static void
setFourNumber(lua_State *L, YGNodeRef node, const struct set_edge_number *setter) {
	if (lua_type(L, -1) == LUA_TNUMBER) {
		float v = lua_tonumber(L, -1);
		setter->set(node, YGEdgeAll, v);
	} else {
		const char * v = luaL_checkstring(L, -1);
		switch (count_words(v)) {
		case 1:
			setEdgeNumber(L, node, YGEdgeAll, v, setter);
			break;
		case 2:
			v = setEdgeNumber(L, node, YGEdgeVertical, v, setter);
			setEdgeNumber(L, node, YGEdgeHorizontal, v, setter);
			break;
		case 3:
			v = setEdgeNumber(L, node, YGEdgeTop, v, setter);
			v = setEdgeNumber(L, node, YGEdgeHorizontal, v, setter);
			setEdgeNumber(L, node, YGEdgeBottom, v, setter);
			break;
		case 4:
			v = setEdgeNumber(L, node, YGEdgeTop, v, setter);
			v = setEdgeNumber(L, node, YGEdgeEnd, v, setter);
			v = setEdgeNumber(L, node, YGEdgeBottom, v, setter);
			setEdgeNumber(L, node, YGEdgeStart, v, setter);
			break;
		default:
			luaL_error(L, "Invalid numbers %s", v);
		}
	}
}

static const char *
setGapNumber(lua_State *L, YGNodeRef node, YGGutter gutter, const char *v, const struct set_two_number *setter) {
	v = skip_whitespace(v);
	char* endptr = NULL;
	float number = strtof(v, &endptr);
	if (is_whitespace(*endptr)) {
		setter->set(node, gutter, number);
		return endptr;
	} else if (setter->setPercent && *endptr == '%') {
		setter->setPercent(node, gutter, number);
		return endptr+1;
	} else {
		luaL_error(L, "Invalid number %s", v);
	}
	return NULL;
}

static void
setTwoNumber(lua_State *L, YGNodeRef node, const struct set_two_number *setter) {
	if (lua_type(L, -1) == LUA_TNUMBER) {
		float v = lua_tonumber(L, -1);
		setter->set(node, YGGutterAll, v);
	} else {
		const char * v = luaL_checkstring(L, -1);
		switch (count_words(v)) {
		case 1:
			setGapNumber(L, node, YGGutterAll, v, setter);
			break;
		case 2:
			v = setGapNumber(L, node, YGGutterRow, v, setter);
			setGapNumber(L, node, YGGutterColumn, v, setter);
			break;
		default:
			luaL_error(L, "Invalid numbers %s", v);
		}
	}
}

static const char *
getNumber(lua_State *L, const char *v, float *num) {
	char* endptr = NULL;
	*num = strtof(v, &endptr);
	if (!is_whitespace(*endptr))
		luaL_error(L, "Invalid number %s", v);
	return endptr;
}

static void
setFlexBasis(lua_State *L, YGNodeRef node, const char *v) {
	static const struct set_number setter = {
		YGNodeStyleSetFlexBasis,
		YGNodeStyleSetFlexBasisPercent,
		YGNodeStyleSetFlexBasisAuto,
		YGNodeStyleSetFlexBasisMaxContent,
		YGNodeStyleSetFlexBasisFitContent,
	};
	v = skip_whitespace(v);
	setNumberString(L, node, v, &setter);
}

static void
lsetFlex(lua_State *L, YGNodeRef node) {
	if (lua_type(L, -1) == LUA_TNUMBER) {
		float v = lua_tonumber(L, -1);
		YGNodeStyleSetFlex(node, v);
	} else {
		const char * v = luaL_checkstring(L, -1);
		char* endptr = NULL;
		float number;
		
		// https://developer.mozilla.org/en-US/docs/Web/CSS/flex
		switch (count_words(v)) {
		case 1:
			// only one word
			number = strtof(v, &endptr);
			if (is_whitespace(*endptr)) {
				// is number, example : 1
				// flex-glow 1 0%
				YGNodeStyleSetFlex(node, number);
			} else {
				// not a number, example : 50%
				// 1 1 flex-basis
				YGNodeStyleSetFlexGrow(node, 1);
				YGNodeStyleSetFlexShrink(node, 1);
				setFlexBasis(L, node, v);
			}
			break;
		case 2:
			v = getNumber(L, v, &number);
			YGNodeStyleSetFlexGrow(node, number);
			number = strtof(v, &endptr);
			if (is_whitespace(*endptr)) {
				YGNodeStyleSetFlexShrink(node, number);
				YGNodeStyleSetFlexBasisPercent(node, 0);
			} else {
				YGNodeStyleSetFlexShrink(node, 1);
				setFlexBasis(L, node, v);
			}
			break;
		case 3:
			v = getNumber(L, v, &number);
			YGNodeStyleSetFlexGrow(node, number);
			v = getNumber(L, v, &number);
			YGNodeStyleSetFlexShrink(node, number);
			setFlexBasis(L, node, v);
			break;
		default:
			luaL_error(L, "Invalid flex %s", v);
		}
	}
}

static void
lsetMargin(lua_State *L, YGNodeRef node) {
	static const struct set_edge_number setter = {
		YGNodeStyleSetMargin,
		YGNodeStyleSetMarginPercent,
		YGNodeStyleSetMarginAuto,
	};
	setFourNumber(L, node, &setter);
}

static void
lsetPadding(lua_State *L, YGNodeRef node) {
	static const struct set_edge_number setter = {
		YGNodeStyleSetPadding,
		YGNodeStyleSetPaddingPercent,
		NULL,
	};
	setFourNumber(L, node, &setter);
}

static void
lsetBorder(lua_State *L, YGNodeRef node) {
	static const struct set_edge_number setter = {
		YGNodeStyleSetBorder,
		NULL,
		NULL,
	};
	setFourNumber(L, node, &setter);
}

static void
lsetGap(lua_State *L, YGNodeRef node) {
	static const struct set_two_number setter = {
		YGNodeStyleSetGap,
		YGNodeStyleSetGapPercent,
	};
	setTwoNumber(L, node, &setter);
}

static int
getEnum(lua_State *L, int type, const char *pname) {
	lua_pushvalue(L, -1);
	if (lua_rawget(L, lua_upvalueindex(2)) == LUA_TNUMBER) {
		int v = lua_tointeger(L, -1);
		if (((v >> 16) & type) == type) {
			v &= 0xffff;
			return v;
		}
	}
	return luaL_error(L, "Invalid enum %s for %s", luaL_tolstring(L, -2, NULL), pname);
}

static int
getEnumHigh(lua_State *L, int type, const char *pname) {
	int e = getEnum(L, type, pname);
	return e >> 8;
}

static int
getEnumLow(lua_State *L, int type, const char *pname) {
	int e = getEnum(L, type, pname);
	return e & 0xff;
}

static void
lsetFlexDirection(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetFlexDirection(node, getEnum(L, FlexDirection, "flex-direction"));
}

static void
lsetJustifyContent(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetJustifyContent(node, getEnumHigh(L, Justify, "justify-content"));
}

static void
lsetAlignItems(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetAlignItems(node, getEnumLow(L, Align, "align-items"));
}

static void
lsetAlignContent(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetAlignContent(node, getEnumLow(L, Align, "align-content"));
}

static void
lsetAlignSelf(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetAlignSelf(node, getEnumLow(L, Align, "align-self"));
}

static void
lsetWrap(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetFlexWrap(node, getEnum(L, Wrap, "wrap"));
}

static void
lsetDisplay(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetDisplay(node, getEnum(L, Display, "display"));
}

static void
lsetPosition(lua_State *L, YGNodeRef node) {
	YGNodeStyleSetPositionType(node, getEnum(L, PositionType, "position"));
}

static void
setPosition(lua_State *L, YGNodeRef node, YGEdge edge) {
	static const struct set_edge_number setter = {
		YGNodeStyleSetPosition,
		YGNodeStyleSetPositionPercent,
		YGNodeStyleSetPositionAuto,
	};
	if (lua_type(L, -1) == LUA_TNUMBER) {
		float v = luaL_checknumber(L, -1);
		setter.set(node, edge, v);
	} else {
		const char *v = luaL_checkstring(L, -1);
		setEdgeNumber(L, node, edge, v, &setter);
	}
}


static void
lsetTop(lua_State *L, YGNodeRef node) {
	setPosition(L, node, YGEdgeTop);
}

static void
lsetBottom(lua_State *L, YGNodeRef node) {
	setPosition(L, node, YGEdgeBottom);
}

static void
lsetLeft(lua_State *L, YGNodeRef node) {
	setPosition(L, node, YGEdgeLeft);
}

static void
lsetRight(lua_State *L, YGNodeRef node) {
	setPosition(L, node, YGEdgeRight);
}

static void
lsetAspectRatio(lua_State *L, YGNodeRef node) {
	float v = luaL_checknumber(L, -1);
	YGNodeStyleSetAspectRatio(node, v);
}

static void
set_array(lua_State *L, YGNodeRef node) {
	lua_pushnil(L);
	int top = lua_gettop(L);
	while (lua_next(L, 2) != 0) {
		lua_pushvalue(L, -2);
		if (lua_rawget(L, lua_upvalueindex(1)) == LUA_TLIGHTUSERDATA) {
			setfunc func = (setfunc)lua_touserdata(L, -1);
			lua_pop(L, 1);
			func(L, node);
		}
		lua_settop(L, top);
	}
}

static void
set_one(lua_State *L, YGNodeRef node) {
	lua_settop(L, 3);
	lua_pushvalue(L, 2);
	if (lua_rawget(L, lua_upvalueindex(1)) == LUA_TLIGHTUSERDATA) {
		setfunc func = (setfunc)lua_touserdata(L, -1);
		lua_pop(L, 1);
		func(L, node);
	} else {
		luaL_error(L, "Invalid attrib name : %s", lua_tostring(L, 2));
	}
}

static int
lnodeSet(lua_State *L) {
	YGNodeRef node = lua_touserdata(L, 1);
	switch(lua_type(L, 2)) {
	case LUA_TTABLE:
		set_array(L, node);
		break;
	case LUA_TSTRING:
		set_one(L, node);
		break;
	default:
		return luaL_error(L, "Set table or key, value");
	}
	return 0;
}

LUAMOD_API int
luaopen_layout_yoga(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "node_new", lnodeNew },
		{ "node_free", lnodeFree },
		{ "node_remove", lnodeRemove },
		{ "node_calc", lnodeCalc },
		{ "node_get", lnodeGet },
		{ "node_set", NULL },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	
	struct {
		const char *name;
		setfunc func;
	} setter [] = {
		{ "width", lsetWidth },
		{ "height", lsetHeight },
		{ "minWidth", lsetMinWidth },
		{ "maxWidth", lsetMaxWidth },
		{ "minHeight", lsetMinHeight },
		{ "maxHeight", lsetMaxHeight },
		{ "direction", lsetFlexDirection },
		{ "justify", lsetJustifyContent },
		{ "alignItems", lsetAlignItems },
		{ "alignContent", lsetAlignContent },
		{ "alignSelf", lsetAlignSelf },
		{ "margin", lsetMargin },
		{ "padding", lsetPadding },
		{ "border", lsetBorder },
		{ "gap", lsetGap },
		{ "wrap", lsetWrap },
		{ "display", lsetDisplay },
		{ "flex", lsetFlex },
		{ "position", lsetPosition },
		{ "top", lsetTop },
		{ "bottom", lsetBottom },
		{ "left", lsetLeft },
		{ "right", lsetRight },
		{ "aspectRatio", lsetAspectRatio },
	};
	int n = sizeof(setter) / sizeof(setter[0]);
	int i;
	lua_createtable(L, n, 0);
	for (i=0;i<n;i++) {
		lua_pushlightuserdata(L, (void *)setter[i].func);
		lua_setfield(L, -2, setter[i].name);
	}

	struct enum_string	estr[] = {
		ENUM(FlexDirection, Column)
		ENUM(FlexDirection, ColumnReverse)
		ENUM(FlexDirection, Row)
		ENUM(FlexDirection, RowReverse)
		ENUM(Justify, FlexStart)
		ENUM(Justify, Center)
		ENUM(Justify, FlexEnd)
		ENUM(Justify, SpaceBetween)
		ENUM(Justify, SpaceAround)
		ENUM(Justify, SpaceEvenly)
		ENUM(Align, Auto)
		ENUM(Align, FlexStart)
		ENUM(Align, Center)
		ENUM(Align, FlexEnd)
		ENUM(Align, Baseline)
		ENUM(Align, SpaceBetween)
		ENUM(Align, SpaceAround)
		ENUM(Align, SpaceEvenly)
		ENUM(Wrap, NoWrap)
		ENUM(Wrap, Wrap)
		ENUM(Wrap, WrapReverse)
		ENUM(Display, Flex)
		ENUM(Display, None)
		ENUM(Display, Contents)
		ENUM(PositionType, Static)
		ENUM(PositionType, Relative)
		ENUM(PositionType, Absolute)
	};
	n = sizeof(estr) / sizeof(estr[0]);
	lua_createtable(L, n, 0);
	for (i=0;i<n;i++) {
		int v = 0;
		if (lua_getfield(L, -1, estr[i].name) == LUA_TNUMBER) {
			v = lua_tointeger(L, -1);
			v = (v & ~0xffff) | ((v & 0xff) << 8);
		}
		lua_pop(L, 1);
		// align use high 8bits
		lua_pushinteger(L, estr[i].value | v);
		lua_setfield(L, -2, estr[i].name);
	}
	lua_pushcclosure(L, lnodeSet, 2);
	lua_setfield(L, -2, "node_set");
	return 1;
}
