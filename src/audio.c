#include <lua.h>
#include <lauxlib.h>

#include "zipreader.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define MA_NO_WIN32_FILEIO
#define MA_NO_MP3
#define MA_NO_FLAC
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

FILE * fopen_utf8(const char *filename, const char *mode);

static ma_result
vfs_open_local(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
	FILE* pFileStd;
	const char* pOpenModeStr;

	MA_ASSERT(pFilePath != NULL);
	MA_ASSERT(openMode  != 0);
	MA_ASSERT(pFile     != NULL);

	(void)pVFS;

	if ((openMode & MA_OPEN_MODE_READ) != 0) {
		if ((openMode & MA_OPEN_MODE_WRITE) != 0) {
			pOpenModeStr = "r+";
		} else {
			pOpenModeStr = "rb";
		}
	} else {
		pOpenModeStr = "wb";
	}
	
	pFileStd = fopen_utf8(pFilePath, pOpenModeStr);
	
	if (pFileStd == NULL) {
		return MA_ERROR;
	}

    *pFile = pFileStd;

    return MA_SUCCESS;
}

struct custom_vfs {
	ma_default_vfs base;
	struct zipreader_name *zipnames;
};

struct custom_engine {
	struct ma_engine engine;
	struct ma_resource_manager rm;
	struct custom_vfs vfs;
};

struct audio_group {
	ma_sound_group group;
	int alive;
};

struct audio_sound {
	ma_sound sound;
	int alive;
};

#define AUDIO_GROUP_METATABLE "SOLUNA_AUDIO_GROUP"
#define AUDIO_SOUND_METATABLE "SOLUNA_AUDIO_SOUND"

static struct custom_engine *
check_engine(lua_State *L, int index) {
	luaL_checktype(L, index, LUA_TLIGHTUSERDATA);
	return (struct custom_engine *)lua_touserdata(L, index);
}

static struct audio_group *
check_group(lua_State *L, int index) {
	struct audio_group *group = (struct audio_group *)luaL_checkudata(L, index, AUDIO_GROUP_METATABLE);
	luaL_argcheck(L, group->alive, index, "closed audio group");
	return group;
}

static struct audio_sound *
check_sound(lua_State *L, int index) {
	struct audio_sound *sound = (struct audio_sound *)luaL_checkudata(L, index, AUDIO_SOUND_METATABLE);
	luaL_argcheck(L, sound->alive, index, "closed audio sound");
	return sound;
}

static int
push_error(lua_State *L, ma_result r) {
	lua_pushnil(L);
	lua_pushstring(L, ma_result_description(r));
	return 2;
}

static int
laudio_group_uninit(lua_State *L) {
	struct audio_group *group = (struct audio_group *)luaL_checkudata(L, 1, AUDIO_GROUP_METATABLE);
	if (group->alive) {
		ma_sound_group_uninit(&group->group);
		group->alive = 0;
	}
	return 0;
}

static int
laudio_sound_uninit(lua_State *L) {
	struct audio_sound *sound = (struct audio_sound *)luaL_checkudata(L, 1, AUDIO_SOUND_METATABLE);
	if (sound->alive) {
		ma_sound_uninit(&sound->sound);
		sound->alive = 0;
	}
	return 0;
}

static ma_result
zr_open(ma_vfs* pVFS, const char* pFilePath, ma_uint32 openMode, ma_vfs_file* pFile) {
	struct custom_vfs *vfs = (struct custom_vfs *)pVFS;
	if (openMode != MA_OPEN_MODE_READ)
		return MA_NOT_IMPLEMENTED;
	zipreader_file zf = zipreader_open(vfs->zipnames, pFilePath);
	if (zf == NULL) {
		return MA_ERROR;
	}
	*pFile = (ma_vfs_file)zf;
	return MA_SUCCESS;
}

static ma_result
zr_close(ma_vfs* pVFS, ma_vfs_file file) {
	(void)pVFS;
	zipreader_close((zipreader_file)file);
	return MA_SUCCESS;
}

static ma_result
zr_read(ma_vfs* pVFS, ma_vfs_file file, void* pDst, size_t sizeInBytes, size_t* pBytesRead) {
	(void)pVFS;
	int bytes = (int)sizeInBytes;
	if (bytes!= sizeInBytes || bytes < 0)
		return MA_OUT_OF_RANGE;
	int rd = zipreader_read((zipreader_file)file, pDst, bytes);
	if (rd < 0)
		return MA_IO_ERROR;
	*pBytesRead = rd;
	return MA_SUCCESS;
}

static ma_result
zr_seek(ma_vfs* pVFS, ma_vfs_file file, ma_int64 offset, ma_seek_origin origin) {
	(void)pVFS;
	int whence;
	switch (origin) {
	case ma_seek_origin_start :
		whence = SEEK_SET;
		break;
	case ma_seek_origin_current :
		whence = SEEK_CUR;
		break;
	case ma_seek_origin_end :
		whence = SEEK_END;
		break;
	default :
		return MA_INVALID_ARGS;
	}
	if (zipreader_seek((zipreader_file)file, offset, whence) != 0) {
		return MA_ERROR;
	}
	return MA_SUCCESS;
}

static ma_result
zr_tell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor) {
	(void)pVFS;
	*pCursor = zipreader_tell((zipreader_file)file);
	if (*pCursor < 0)
		return MA_ERROR;
	return MA_SUCCESS;
}

static ma_result
zr_info(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo) {
	(void)pVFS;
	pInfo->sizeInBytes = zipreader_size((zipreader_file)file);
	return MA_SUCCESS;
}

static int
laudio_init_vfs(lua_State *L) {
	struct custom_engine *e = (struct custom_engine *)lua_touserdata(L, 1);
	luaL_checktype(L, 2, LUA_TUSERDATA);
	e->vfs.zipnames = lua_touserdata(L, 2);
	e->vfs.base.cb.onOpen = zr_open;
	e->vfs.base.cb.onOpenW = NULL;
	e->vfs.base.cb.onClose = zr_close;
	e->vfs.base.cb.onRead = zr_read;
	e->vfs.base.cb.onWrite = NULL;
	e->vfs.base.cb.onSeek = zr_seek;
	e->vfs.base.cb.onTell = zr_tell;
	e->vfs.base.cb.onInfo = zr_info;
	return 0;
}

static int
laudio_init(lua_State *L) {
	struct custom_engine *e = (struct custom_engine *)lua_newuserdatauv(L, sizeof(*e), 0);
	
	ma_default_vfs_init(&e->vfs.base, NULL);
	e->vfs.base.cb.onOpen = vfs_open_local;
	e->vfs.zipnames = NULL;

    ma_resource_manager_config config = ma_resource_manager_config_init();
	config.pVFS = &e->vfs;
	
	ma_result r = ma_resource_manager_init(&config, &e->rm);
	if (r != MA_SUCCESS) {
		return luaL_error(L, "ma_resource_manager_init() error : %s", ma_result_description(r));
	}
		
	ma_engine_config ec = ma_engine_config_init();
	ec.pResourceManager = &e->rm;
	r = ma_engine_init(&ec, &e->engine);
	if (r != MA_SUCCESS) {
		return luaL_error(L, "ma_engine_init() error : %s", ma_result_description(r));
	}
	e->rm.config.decodedFormat = ma_format_f32;
	e->rm.config.decodedSampleRate = ma_engine_get_sample_rate(&e->engine);
	lua_pushlightuserdata(L, (void *)e);
	
	return 2;
}

static int
laudio_deinit(lua_State *L) {
	struct custom_engine *e = check_engine(L, 1);
	ma_engine_uninit(&e->engine);
	ma_resource_manager_uninit(&e->rm);

	return 0;
}

static int
laudio_group_init(lua_State *L) {
	struct custom_engine *e = check_engine(L, 1);
	struct audio_group *group = (struct audio_group *)lua_newuserdatauv(L, sizeof(*group), 0);
	group->alive = 0;
	ma_result r = ma_sound_group_init(&e->engine, 0, NULL, &group->group);
	if (r != MA_SUCCESS) {
		lua_pop(L, 1);
		return push_error(L, r);
	}
	group->alive = 1;
	luaL_setmetatable(L, AUDIO_GROUP_METATABLE);
	return 1;
}

static int
laudio_group_set_volume(lua_State *L) {
	struct audio_group *group = check_group(L, 1);
	float volume = (float)luaL_checknumber(L, 2);
	ma_sound_group_set_volume(&group->group, volume);
	return 0;
}

static int
laudio_sound_init(lua_State *L) {
	struct custom_engine *e = check_engine(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	ma_uint32 flags = (ma_uint32)luaL_optinteger(L, 3, 0);
	struct audio_group *group = NULL;
	if (!lua_isnoneornil(L, 4)) {
		group = check_group(L, 4);
	}

	struct audio_sound *sound = (struct audio_sound *)lua_newuserdatauv(L, sizeof(*sound), 0);
	sound->alive = 0;
	ma_result r = ma_sound_init_from_file(&e->engine, filename, flags, group ? &group->group : NULL, NULL, &sound->sound);
	if (r != MA_SUCCESS) {
		lua_pop(L, 1);
		return push_error(L, r);
	}
	sound->alive = 1;
	luaL_setmetatable(L, AUDIO_SOUND_METATABLE);
	return 1;
}

static int
laudio_sound_start(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	ma_result r = ma_sound_start(&sound->sound);
	if (r != MA_SUCCESS) {
		return push_error(L, r);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int
laudio_sound_stop(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	ma_result r;
	if (lua_isnoneornil(L, 2)) {
		r = ma_sound_stop(&sound->sound);
	} else {
		ma_uint64 fade_ms = (ma_uint64)luaL_checkinteger(L, 2);
		if (fade_ms == 0) {
			r = ma_sound_stop(&sound->sound);
		} else {
			r = ma_sound_stop_with_fade_in_milliseconds(&sound->sound, fade_ms);
		}
	}
	if (r != MA_SUCCESS) {
		return push_error(L, r);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int
laudio_sound_playing(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	lua_pushboolean(L, ma_sound_is_playing(&sound->sound));
	return 1;
}

static int
laudio_sound_set_volume(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	float volume = (float)luaL_checknumber(L, 2);
	ma_sound_set_volume(&sound->sound, volume);
	return 0;
}

static int
laudio_sound_set_pan(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	float pan = (float)luaL_checknumber(L, 2);
	ma_sound_set_pan(&sound->sound, pan);
	return 0;
}

static int
laudio_sound_set_pitch(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	float pitch = (float)luaL_checknumber(L, 2);
	ma_sound_set_pitch(&sound->sound, pitch);
	return 0;
}

static int
laudio_sound_set_looping(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	int looping = lua_toboolean(L, 2);
	ma_sound_set_looping(&sound->sound, looping);
	return 0;
}

static int
laudio_sound_seek(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	float seconds = (float)luaL_checknumber(L, 2);
	ma_result r = ma_sound_seek_to_second(&sound->sound, seconds);
	if (r != MA_SUCCESS) {
		return push_error(L, r);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static int
laudio_sound_tell(lua_State *L) {
	struct audio_sound *sound = check_sound(L, 1);
	float seconds = 0.0f;
	ma_result r = ma_sound_get_cursor_in_seconds(&sound->sound, &seconds);
	if (r != MA_SUCCESS) {
		return push_error(L, r);
	}
	lua_pushnumber(L, seconds);
	return 1;
}

int
luaopen_soluna_audio(lua_State *L) {
	luaL_checkversion(L);
	if (luaL_newmetatable(L, AUDIO_GROUP_METATABLE)) {
		lua_pushcfunction(L, laudio_group_uninit);
		lua_setfield(L, -2, "__gc");
	}
	lua_pop(L, 1);
	if (luaL_newmetatable(L, AUDIO_SOUND_METATABLE)) {
		lua_pushcfunction(L, laudio_sound_uninit);
		lua_setfield(L, -2, "__gc");
	}
	lua_pop(L, 1);
	luaL_Reg l[] = {
		{ "init", laudio_init },
		{ "init_vfs", laudio_init_vfs },
		{ "deinit", laudio_deinit },
		{ "group_init", laudio_group_init },
		{ "group_uninit", laudio_group_uninit },
		{ "group_set_volume", laudio_group_set_volume },
		{ "sound_init", laudio_sound_init },
		{ "sound_uninit", laudio_sound_uninit },
		{ "sound_start", laudio_sound_start },
		{ "sound_stop", laudio_sound_stop },
		{ "sound_playing", laudio_sound_playing },
		{ "sound_set_volume", laudio_sound_set_volume },
		{ "sound_set_pan", laudio_sound_set_pan },
		{ "sound_set_pitch", laudio_sound_set_pitch },
		{ "sound_set_looping", laudio_sound_set_looping },
		{ "sound_seek", laudio_sound_seek },
		{ "sound_tell", laudio_sound_tell },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
