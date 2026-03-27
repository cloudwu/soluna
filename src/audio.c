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
	lua_pushlightuserdata(L, (void *)e);
	
	return 2;
}

static int
laudio_deinit(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	ma_engine *engine = (ma_engine *)lua_touserdata(L, 1);
	ma_engine_uninit(engine);

	return 0;
}

/*
// todo : call ma_sound_init_from_file()

static int
laudio_load(lua_State *L) {
	return 0;
}

static int
laudio_unload(lua_State *L) {
	return 0;
}
*/

static int
laudio_play(lua_State *L) {
	luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
	ma_engine *engine = (ma_engine *)lua_touserdata(L, 1);
	const char *filename = luaL_checkstring(L, 2);
	
	ma_engine_play_sound(engine, filename, NULL);
	return 0;
}

int
luaopen_soluna_audio(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "init", laudio_init },
		{ "init_vfs", laudio_init_vfs },
		{ "deinit", laudio_deinit },
		{ "play", laudio_play },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
