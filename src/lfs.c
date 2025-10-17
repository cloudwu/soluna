#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#define LONGPATH_MAX 4096

#include <windows.h>
#include <Shlobj.h>
#include <sys/stat.h>
#include <wchar.h>

#define STAT_STRUCT struct _stati64
#define STAT_FUNC _wstati64

#ifndef S_ISDIR
#define S_ISDIR(mode)  (mode&_S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode)  (mode&_S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(mode)  (0)
#endif
#ifndef S_ISSOCK
#define S_ISSOCK(mode)  (0)
#endif
#ifndef S_ISFIFO
#define S_ISFIFO(mode)  (0)
#endif
#ifndef S_ISCHR
#define S_ISCHR(mode)  (mode&_S_IFCHR)
#endif
#ifndef S_ISBLK
#define S_ISBLK(mode)  (0)
#endif

static int
utf8_filename(lua_State *L, const wchar_t * winfilename, int wsz, char *utf8buffer, int sz) {
	int result = WideCharToMultiByte(CP_UTF8, 0, winfilename, wsz, utf8buffer, sz, NULL, NULL);
	if (result == 0)
		return luaL_error(L, "convert to utf-8 filename fail");
	if (wsz < 0)	// not include end \0
		return result - 1;
	if (result >= sz)
		return luaL_error(L, "convert to utf-8 filename : buffer overflow");
	utf8buffer[result] = 0;
	return result;
}

#define DIR_METATABLE "SOLUNA_DIR"

struct dir_data {
	HANDLE findfile;
	int closed;
};

static int
windows_filename(lua_State *L, const char * utf8filename, int usz, wchar_t * winbuffer, int wsz) {
	int result = MultiByteToWideChar(CP_UTF8, 0, utf8filename, usz, winbuffer, wsz);
	if (result == 0)
		return luaL_error(L, "convert to windows utf-16 filename fail");
	if (result < 0)
		return result - 1;
	if (result >= wsz)
		return luaL_error(L, "convert to windows utf-16 filename : buffer overflow");
	winbuffer[result] = 0;
	return result;
}

static void
system_error(lua_State *L, DWORD errcode) {
	wchar_t * errormsg;
	DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		errcode, 0,
		(void *)&errormsg, sizeof(errormsg),
		NULL);
	if (n == 0) {
		lua_pushfstring(L, "Unknown error %04X", errcode);
	} else {
		int i;
		for (i=n;i>=0;i--) {
			if (errormsg[i] == 0 || errormsg[i] == '\n' || errormsg[i] == '\r')
				--n;
			else {
				break;
			}
		}
		char tmp[LONGPATH_MAX];
		int len = utf8_filename(L, errormsg, n, tmp, LONGPATH_MAX);
		lua_pushlstring(L, tmp, len);
		HeapFree(GetProcessHeap(), 0, errormsg);
	}
}

static int
error_return(lua_State *L) {
	lua_pushnil(L);
	system_error(L, GetLastError());
	return 2;
}

static void
push_filename(lua_State *L, WIN32_FIND_DATAW *data) {
	char firstname[LONGPATH_MAX];
	int ulen = utf8_filename(L, data->cFileName, -1, firstname, LONGPATH_MAX);
	lua_pushlstring(L, firstname, ulen);
}

static int
dir_iter(lua_State *L) {
	struct dir_data *d = luaL_checkudata(L, 1, DIR_METATABLE);
	luaL_argcheck (L, d->closed == 0, 1, "closed directory");
	if (d->findfile == INVALID_HANDLE_VALUE) {
		// no find found
		d->closed = 1;
		return 0;
	}
	if (lua_getuservalue(L, 1) == LUA_TSTRING) {
		// find time
		lua_pushnil(L);
		lua_setuservalue(L, 1);
		return 1;
	} else {
		WIN32_FIND_DATAW data;
		if (FindNextFileW(d->findfile, &data)) {
			push_filename(L, &data);
			return 1;
		} else {
			DWORD errcode = GetLastError();
			FindClose(d->findfile);
			d->findfile = INVALID_HANDLE_VALUE;
			d->closed = 1;
			if (errcode == ERROR_NO_MORE_FILES)
				return 0;
			lua_pushnil(L);
			system_error(L, errcode);
			return 2;
		}
	}
}

static int
dir_close(lua_State *L) {
	struct dir_data *d = luaL_checkudata(L, 1, DIR_METATABLE);
	if (d->findfile != INVALID_HANDLE_VALUE) {
		FindClose(d->findfile);
		d->findfile = INVALID_HANDLE_VALUE;
	}
	d->closed = 1;
	return 0;
}

static int
ldir(lua_State *L) {
	size_t sz;
	const char * pathname = luaL_checklstring(L, 1, &sz);
	wchar_t winname[LONGPATH_MAX-3];
	int winsz = windows_filename(L, pathname, sz, winname, LONGPATH_MAX-3);
	winname[winsz] = '\\';
	winname[winsz+1] = '*';
	winname[winsz+2] = 0;
	WIN32_FIND_DATAW data;
	HANDLE findfile = FindFirstFileW(winname, &data);
	lua_pushcfunction(L, dir_iter);
	if (findfile == INVALID_HANDLE_VALUE) {
		DWORD errcode = GetLastError();
		if (errcode == ERROR_FILE_NOT_FOUND) {
			struct dir_data *d = lua_newuserdata(L, sizeof(*d));
			d->findfile = INVALID_HANDLE_VALUE;
			d->closed = 0;
		} else {
			system_error(L, errcode);
			return lua_error(L);
		}
	} else {
		struct dir_data *d = lua_newuserdata(L, sizeof(*d));
		d->findfile = findfile;
		d->closed = 0;
		push_filename(L, &data);
		lua_setuservalue(L, -2);	// set firstname
	}

	if (luaL_newmetatable(L, DIR_METATABLE)) {
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction (L, dir_iter);
		lua_setfield(L, -2, "next");
		lua_pushcfunction (L, dir_close);
		lua_setfield(L, -2, "close");
		lua_pushcfunction (L, dir_close);
		lua_setfield (L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return 2;
}

static int
lpersonaldir(lua_State *L) {
	wchar_t document[LONGPATH_MAX] = {0};
	if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, document) == S_OK) {
		char utf8path[LONGPATH_MAX];
		int sz = utf8_filename(L, document, -1, utf8path, LONGPATH_MAX);
		lua_pushlstring(L, utf8path, sz);
		return 1;
	} else {
		return error_return(L);
	}
}

static int
lcurrentdir(lua_State *L) {
	wchar_t path[LONGPATH_MAX];
	char utf8path[LONGPATH_MAX];
	DWORD sz = GetCurrentDirectoryW(LONGPATH_MAX, path);
	if (sz == 0) {
		return error_return(L);
	}
	int usz = utf8_filename(L, path, -1, utf8path, LONGPATH_MAX);
	lua_pushlstring(L, utf8path, usz);
	return 1;
}

static int
lchdir(lua_State *L) {
	size_t sz;
	const char * utf8path = luaL_checklstring(L, 1, &sz);
	wchar_t path[LONGPATH_MAX];
	windows_filename(L, utf8path, sz, path, LONGPATH_MAX);
	if (SetCurrentDirectoryW(path) == 0) {
		return error_return(L);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static const char *
mode2string (unsigned short mode) {
	if ( S_ISREG(mode) )
		return "file";
	else if ( S_ISDIR(mode) )
		return "directory";
	else if ( S_ISLNK(mode) )
		return "link";
	else if ( S_ISSOCK(mode) )
		return "socket";
	else if ( S_ISFIFO(mode) )
		return "named pipe";
	else if ( S_ISCHR(mode) )
		return "char device";
	else if ( S_ISBLK(mode) )
		return "block device";
	else
		return "other";
}

/* inode protection mode */
static void push_st_mode (lua_State *L, STAT_STRUCT *info) {
	lua_pushstring (L, mode2string (info->st_mode));
}
/* device inode resides on */
static void push_st_dev (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_dev);
}
/* inode's number */
static void push_st_ino (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_ino);
}
/* number of hard links to the file */
static void push_st_nlink (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer)info->st_nlink);
}
/* user-id of owner */
static void push_st_uid (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer)info->st_uid);
}
/* group-id of owner */
static void push_st_gid (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer)info->st_gid);
}
/* device type, for special file inode */
static void push_st_rdev (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_rdev);
}
/* time of last access */
static void push_st_atime (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_atime);
}
/* time of last data modification */
static void push_st_mtime (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_mtime);
}
/* time of last file status change */
static void push_st_ctime (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer) info->st_ctime);
}
/* file size, in bytes */
static void push_st_size (lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger (L, (lua_Integer)info->st_size);
}

static const char *perm2string (unsigned short mode) {
	static char perms[10] = "---------";
	int i;
	for (i=0;i<9;i++) perms[i]='-';
	if (mode  & _S_IREAD)
		{ perms[0] = 'r'; perms[3] = 'r'; perms[6] = 'r'; }
	if (mode  & _S_IWRITE)
		{ perms[1] = 'w'; perms[4] = 'w'; perms[7] = 'w'; }
	if (mode  & _S_IEXEC)
		{ perms[2] = 'x'; perms[5] = 'x'; perms[8] = 'x'; }
	return perms;
}

/* permssions string */
static void push_st_perm (lua_State *L, STAT_STRUCT *info) {
	lua_pushstring (L, perm2string (info->st_mode));
}

typedef void (*_push_function) (lua_State *L, STAT_STRUCT *info);

struct _stat_members {
	const char *name;
	_push_function push;
};

struct _stat_members members[] = {
	{ "mode",         push_st_mode },
	{ "dev",          push_st_dev },
	{ "ino",          push_st_ino },
	{ "nlink",        push_st_nlink },
	{ "uid",          push_st_uid },
	{ "gid",          push_st_gid },
	{ "rdev",         push_st_rdev },
	{ "access",       push_st_atime },
	{ "modification", push_st_mtime },
	{ "change",       push_st_ctime },
	{ "size",         push_st_size },
	{ "permissions",  push_st_perm },
	{ NULL, NULL }
};

/*
** Get file or symbolic link information
*/
static int
file_info (lua_State *L) {
	STAT_STRUCT info;
	size_t sz;
	int i;
	const char * utf8path = luaL_checklstring(L, 1, &sz);
	wchar_t file[LONGPATH_MAX];
	windows_filename(L, utf8path, sz, file, LONGPATH_MAX);

	if (STAT_FUNC(file,	&info))	{
			lua_pushnil(L);
			lua_pushfstring(L, "cannot obtain information from file	'%s': %s", file, strerror(errno));
			lua_pushinteger(L, errno);
			return 3;
	}
	if (lua_isstring (L, 2)) {
			const char *member = lua_tostring (L, 2);
			for	(i = 0;	members[i].name; i++) {
					if (strcmp(members[i].name,	member)	== 0) {
							/* push	member value and return	*/
							members[i].push	(L,	&info);
							return 1;
					}
			}
			/* member not found	*/
			return luaL_error(L, "invalid attribute	name '%s'",	member);
	}
	/* creates a table if none is given, removes extra arguments */
	lua_settop(L, 2);
	if (!lua_istable (L, 2)) {
			lua_newtable (L);
	}
	/* stores all members in table on top of the stack */
	for	(i = 0;	members[i].name; i++) {
			lua_pushstring (L, members[i].name);
			members[i].push	(L,	&info);
			lua_rawset (L, -3);
	}
	return 1;
}

static int
lrealpath(lua_State *L) {
	size_t sz;
	const char * pathname = luaL_checklstring(L, 1, &sz);
	wchar_t winname[LONGPATH_MAX];
	wchar_t fullname[LONGPATH_MAX];
	windows_filename(L, pathname, sz, winname, LONGPATH_MAX);
	DWORD r = GetFullPathNameW(winname, LONGPATH_MAX, fullname, NULL);
	if (r == 0) {
		return error_return(L);
	}
	if (r > LONGPATH_MAX) {
		return luaL_error(L, "Invalid path %s", pathname);
	}
	char result[LONGPATH_MAX];
	int len = utf8_filename(L, fullname, r, result, LONGPATH_MAX);
	lua_pushlstring(L, result, len);
	return 1;
}

static inline int
create_dir_wchar_(const WCHAR *filenameW) {
	WIN32_FIND_DATAW FindFileData;
	HANDLE h = FindFirstFileW(filenameW, &FindFileData);
	if (h == INVALID_HANDLE_VALUE) {
		// create dir
		if (CreateDirectoryW(filenameW, NULL) == 0)
			return -1;
	} else {
		if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			FindClose(h);
			// dir exist
		} else {
            FindClose(h);
			// not a dir
			return 0;
		}
	}
	return 1;
}

static int
mkdir_utf8(const char *name) {
	WCHAR filenameW[FILENAME_MAX + 0x200 + 1];
	int n = MultiByteToWideChar(CP_UTF8,0,(const char*)name,-1,filenameW,FILENAME_MAX + 0x200);
	if (n == 0)
		return -1;
	return create_dir_wchar_(filenameW);
}

static int
pusherror(lua_State * L) {
	lua_pushnil(L);
	DWORD err = GetLastError();
	LPVOID lpMsgBuf;

	if (FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		0,
		(LPWSTR) &lpMsgBuf,
		0, NULL) == 0) {
		lua_pushstring(L, "FormatMessage failed");
    }
	
	char errtext[1024] = "unknown";
	size_t sz = wcslen((LPWSTR)lpMsgBuf);

	WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)lpMsgBuf, sz, errtext, 1024, NULL, NULL);

	LocalFree(lpMsgBuf);
	
	lua_pushstring(L, errtext);
	lua_pushinteger(L, err);
	return 3;
}

#else

#define LONGPATH_MAX 4096

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>

#define STAT_STRUCT struct stat
#define STAT_FUNC stat

#define DIR_METATABLE "SOLUNA_DIR"

struct dir_data {
	DIR* dir;
	int closed;
};

static void
system_error(lua_State *L, int errcode) {
	lua_pushstring(L, strerror(errcode));
}

static int
error_return(lua_State *L) {
	lua_pushnil(L);
	system_error(L, errno);
	return 2;
}

static int
dir_iter(lua_State *L) {
	struct dir_data *d = luaL_checkudata(L, 1, DIR_METATABLE);
	luaL_argcheck(L, d->closed == 0, 1, "closed directory");
	if (d->dir == NULL) {
		// no find found
		d->closed = 1;
		return 0;
	}
	if (lua_getuservalue(L, 1) == LUA_TSTRING) {
		// first time
		lua_pushnil(L);
		lua_setuservalue(L, 1);
		return 1;
	} else {
		struct dirent *entry = readdir(d->dir);
		if (entry) {
			lua_pushstring(L, entry->d_name);
			return 1;
		} else {
			closedir(d->dir);
			d->dir = NULL;
			d->closed = 1;
			return 0;
		}
	}
}

static int
dir_close(lua_State *L) {
	struct dir_data *d = luaL_checkudata(L, 1, DIR_METATABLE);
	if (d->dir != NULL) {
		closedir(d->dir);
		d->dir = NULL;
	}
	d->closed = 1;
	return 0;
}

static int
ldir(lua_State *L) {
	size_t sz;
	const char * pathname = luaL_checklstring(L, 1, &sz);
	DIR* dir = opendir(pathname);
	
	lua_pushcfunction(L, dir_iter);
	
	if (dir == NULL) {
		if (errno == ENOENT) {
			struct dir_data *d = lua_newuserdata(L, sizeof(*d));
			d->dir = NULL;
			d->closed = 0;
		} else {
			system_error(L, errno);
			return lua_error(L);
		}
	} else {
		struct dirent *entry = readdir(dir);
		struct dir_data *d = lua_newuserdata(L, sizeof(*d));
		d->dir = dir;
		d->closed = 0;
		if (entry) {
			lua_pushstring(L, entry->d_name);
			lua_setuservalue(L, -2);	// set firstname
		}
	}

	if (luaL_newmetatable(L, DIR_METATABLE)) {
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		lua_pushcfunction(L, dir_iter);
		lua_setfield(L, -2, "next");
		lua_pushcfunction(L, dir_close);
		lua_setfield(L, -2, "close");
		lua_pushcfunction(L, dir_close);
		lua_setfield(L, -2, "__gc");
	}
	lua_setmetatable(L, -2);
	return 2;
}

static int
lpersonaldir(lua_State *L) {
#if defined(__EMSCRIPTEN__)
  lua_pushstring(L, "/");
  return 1;
#else
	struct passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_dir) {
		lua_pushstring(L, pw->pw_dir);
		return 1;
	} else {
		return error_return(L);
	}
#endif
}

static int
lcurrentdir(lua_State *L) {
	char path[LONGPATH_MAX];
	if (getcwd(path, LONGPATH_MAX) == NULL) {
		return error_return(L);
	}
	lua_pushstring(L, path);
	return 1;
}

static int
lchdir(lua_State *L) {
	size_t sz;
	const char * path = luaL_checklstring(L, 1, &sz);
	if (chdir(path) != 0) {
		return error_return(L);
	}
	lua_pushboolean(L, 1);
	return 1;
}

static const char *
mode2string(mode_t mode) {
	if (S_ISREG(mode))
		return "file";
	else if (S_ISDIR(mode))
		return "directory";
	else if (S_ISLNK(mode))
		return "link";
	else if (S_ISSOCK(mode))
		return "socket";
	else if (S_ISFIFO(mode))
		return "named pipe";
	else if (S_ISCHR(mode))
		return "char device";
	else if (S_ISBLK(mode))
		return "block device";
	else
		return "other";
}

/* inode protection mode */
static void push_st_mode(lua_State *L, STAT_STRUCT *info) {
	lua_pushstring(L, mode2string(info->st_mode));
}
/* device inode resides on */
static void push_st_dev(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_dev);
}
/* inode's number */
static void push_st_ino(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_ino);
}
/* number of hard links to the file */
static void push_st_nlink(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer)info->st_nlink);
}
/* user-id of owner */
static void push_st_uid(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer)info->st_uid);
}
/* group-id of owner */
static void push_st_gid(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer)info->st_gid);
}
/* device type, for special file inode */
static void push_st_rdev(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_rdev);
}
/* time of last access */
static void push_st_atime(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_atime);
}
/* time of last data modification */
static void push_st_mtime(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_mtime);
}
/* time of last file status change */
static void push_st_ctime(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer) info->st_ctime);
}
/* file size, in bytes */
static void push_st_size(lua_State *L, STAT_STRUCT *info) {
	lua_pushinteger(L, (lua_Integer)info->st_size);
}

static const char *perm2string(mode_t mode) {
	static char perms[10] = "---------";
	int i;
	for (i=0;i<9;i++) perms[i]='-';
	
	if (mode & S_IRUSR) perms[0] = 'r';
	if (mode & S_IWUSR) perms[1] = 'w';
	if (mode & S_IXUSR) perms[2] = 'x';
	if (mode & S_IRGRP) perms[3] = 'r';
	if (mode & S_IWGRP) perms[4] = 'w';
	if (mode & S_IXGRP) perms[5] = 'x';
	if (mode & S_IROTH) perms[6] = 'r';
	if (mode & S_IWOTH) perms[7] = 'w';
	if (mode & S_IXOTH) perms[8] = 'x';
	
	return perms;
}

/* permissions string */
static void push_st_perm(lua_State *L, STAT_STRUCT *info) {
	lua_pushstring(L, perm2string(info->st_mode));
}

typedef void (*_push_function) (lua_State *L, STAT_STRUCT *info);

struct _stat_members {
	const char *name;
	_push_function push;
};

struct _stat_members members[] = {
	{ "mode",         push_st_mode },
	{ "dev",          push_st_dev },
	{ "ino",          push_st_ino },
	{ "nlink",        push_st_nlink },
	{ "uid",          push_st_uid },
	{ "gid",          push_st_gid },
	{ "rdev",         push_st_rdev },
	{ "access",       push_st_atime },
	{ "modification", push_st_mtime },
	{ "change",       push_st_ctime },
	{ "size",         push_st_size },
	{ "permissions",  push_st_perm },
	{ NULL, NULL }
};

/*
** Get file or symbolic link information
*/
static int
file_info(lua_State *L) {
	STAT_STRUCT info;
	size_t sz;
	int i;
	const char * path = luaL_checklstring(L, 1, &sz);

	if (STAT_FUNC(path, &info)) {
		lua_pushnil(L);
		lua_pushfstring(L, "cannot obtain information from file '%s': %s", path, strerror(errno));
		lua_pushinteger(L, errno);
		return 3;
	}
	if (lua_isstring(L, 2)) {
		const char *member = lua_tostring(L, 2);
		for (i = 0; members[i].name; i++) {
			if (strcmp(members[i].name, member) == 0) {
				/* push member value and return */
				members[i].push(L, &info);
				return 1;
			}
		}
		/* member not found */
		return luaL_error(L, "invalid attribute name '%s'", member);
	}
	/* creates a table if none is given, removes extra arguments */
	lua_settop(L, 2);
	if (!lua_istable(L, 2)) {
		lua_newtable(L);
	}
	/* stores all members in table on top of the stack */
	for (i = 0; members[i].name; i++) {
		lua_pushstring(L, members[i].name);
		members[i].push(L, &info);
		lua_rawset(L, -3);
	}
	return 1;
}

static int
lrealpath(lua_State *L) {
	size_t sz;
	const char * pathname = luaL_checklstring(L, 1, &sz);
	char resolved[LONGPATH_MAX];
	
	if (realpath(pathname, resolved) == NULL) {
		return error_return(L);
	}
	
	lua_pushstring(L, resolved);
	return 1;
}

#define mkdir_utf8(path) (mkdir((path), \
    S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH))
	
// todo: succ when dir exist
	
static int
pusherror(lua_State * L) {
	lua_pushnil(L);
	lua_pushstring(L, strerror(errno));
	lua_pushinteger(L, errno);
	return 3;
}

#endif

static int
pushresult(lua_State * L, int res) {
	if (res == -1) {
		return pusherror(L);
	} else if (res == 0) {
		lua_pushnil(L);
		lua_pushfstring(L, "%s already exist", lua_tostring(L, 1));
		return 2;
	} else {
		lua_pushboolean(L, 1);
		return 1;
	}
}

static int
lmkdir(lua_State * L) {
	const char *path = luaL_checkstring(L, 1);
	return pushresult(L, mkdir_utf8(path));
}

int
luaopen_localfs(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "personaldir" , lpersonaldir },
		{ "dir", ldir },
		{ "currentdir", lcurrentdir },
		{ "chdir", lchdir },
		{ "attributes", file_info },	// the same with lfs, but support utf-8 filename
		{ "realpath", lrealpath },
		{ "mkdir", lmkdir },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);

	return 1;
}
