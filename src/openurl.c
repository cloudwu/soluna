#include <lua.h>
#include <lauxlib.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#include <windows.h>

static void
open_url(lua_State *L, const char *url) {
	int n = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
	if (n == 0)
		luaL_error(L, "Invalid url string : %s", url);
	void * buf = lua_newuserdatauv(L, n * sizeof(WCHAR), 0);
	MultiByteToWideChar(CP_UTF8, 0, url, -1, (WCHAR *)buf, n);
	ShellExecuteW(NULL, L"open", (WCHAR *)buf, NULL, NULL, SW_SHOWNORMAL);
}

#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#if defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#endif

extern void soluna_wasm_open_url(const char *url);

static void
soluna_wasm_call_open_url(const char *url) {
#if defined(__EMSCRIPTEN_PTHREADS__)
  if (!emscripten_is_main_browser_thread()) {
    emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, soluna_wasm_open_url, url);
    return;
  }
#endif
  soluna_wasm_open_url(url);
}

static void
open_url(lua_State *L, const char *url) {
  soluna_wasm_call_open_url(url);
}

#elif defined(__APPLE__) || defined(__linux__)

#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

static void
open_url(lua_State *L, const char *url) {
  pid_t pid = fork();
  if (pid < 0) {
    luaL_error(L, "fork() failed");
  } else if (pid == 0) {
    // child
#if defined(__APPLE__)
    execl("/usr/bin/open", "open", url, (char *)NULL);
#else
    execl("/usr/bin/xdg-open", "xdg-open", url, (char *)NULL);
#endif
    // if execl return, it's error
    _exit(127);
  } else {
    // parent
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      luaL_error(L, "failed to open url: %s", url);
    }
  }
}

#else

static void
open_url(lua_State *L, const char *url) {
}

#endif

static int
lurl_open(lua_State *L) {
	const char * url = luaL_checkstring(L, 1);
	open_url(L, url);
	return 0;
}

int
luaopen_url(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "open", lurl_open },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}
