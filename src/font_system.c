#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)

#include <windows.h>
#define MAX_NAME 1024

static void *
free_data(void *ud, void *ptr, size_t oszie, size_t nsize) {
	free(ptr);
	return NULL;
}

static int
lttfdata(lua_State *L) {
	const char * familyName = luaL_checkstring(L, 1);
	WCHAR familyNameW[MAX_NAME];
	int n = MultiByteToWideChar(CP_UTF8,0,(const char*)familyName,-1,familyNameW,MAX_NAME);
	if (n == 0 || n > LF_FACESIZE)
		return luaL_error(L, "Invalid family name %s", familyName);
	HDC hdc = CreateCompatibleDC(0);
	LOGFONTW lf;
	memset(&lf, 0, sizeof(LOGFONT));
	memcpy(lf.lfFaceName, familyNameW, n * sizeof(WCHAR));
	lf.lfCharSet = DEFAULT_CHARSET;
	HFONT hfont = CreateFontIndirectW(&lf); 
	if (!hfont) {
		DeleteDC(hdc);
		return luaL_error(L, "Create font failed: %d", GetLastError());
	}
    HGDIOBJ oldobj = SelectObject(hdc, hfont);
	uint32_t tags[2] = {0x66637474/*ttcf*/, 0};
	int i;
	DWORD bytes = 0;
	char *buf = NULL;
	for (i=0;i<2;i++) {
		uint32_t tag = tags[i];
		bytes = GetFontData(hdc, tag, 0, 0, 0);
        if (bytes != GDI_ERROR) {
			buf = malloc(bytes+1);//lua_newuserdatauv(L, bytes, 0);
			if (buf == NULL)
				return luaL_error(L, "Out of memory : sysfont");
			buf[bytes] = 0;
			bytes = GetFontData(hdc, tag, 0, (void *)buf, bytes);
			if (bytes != GDI_ERROR) {
				break;
			} else {
				free(buf);
				bytes = 0;
			}
		}
	}
	SelectObject(hdc, oldobj);
	DeleteObject(hfont);
	DeleteDC(hdc);
	if (bytes == 0) {
		return luaL_error(L, "Read font data failed");
	}
	lua_pushexternalstring(L, buf, bytes, free_data , NULL);
	return 1;
}

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#define kCTFontTableTtcf 'ttcf'

static void *
free_data(void *ud, void *ptr, size_t oszie, size_t nsize) {
	free(ptr);
	return NULL;
}

static CFDataRef read_font_file_data(CFURLRef url) {
	CFReadStreamRef stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url);
	if (!stream) {
		return NULL;
	}
	
	if (!CFReadStreamOpen(stream)) {
		CFRelease(stream);
		return NULL;
	}
	
	CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
	UInt8 buffer[8192];
	CFIndex bytesRead;
	
	while ((bytesRead = CFReadStreamRead(stream, buffer, sizeof(buffer))) > 0) {
		CFDataAppendBytes(data, buffer, bytesRead);
	}
	
	CFReadStreamClose(stream);
	CFRelease(stream);
	
	if (bytesRead < 0) {
		CFRelease(data);
		return NULL;
	}
	
	return data;
}

static int
lttfdata(lua_State *L) {
	const char *familyName = luaL_checkstring(L, 1);
	
	CFStringRef fontNameStr = CFStringCreateWithCString(kCFAllocatorDefault, 
														familyName, 
														kCFStringEncodingUTF8);
	if (!fontNameStr) {
		return luaL_error(L, "Failed to create font name string");
	}
	
	CFDictionaryRef attributes = CFDictionaryCreate(
		kCFAllocatorDefault,
		(const void**)&kCTFontFamilyNameAttribute,
		(const void**)&fontNameStr,
		1,
		&kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks
	);
	
	CTFontDescriptorRef descriptor = CTFontDescriptorCreateWithAttributes(attributes);
	CFRelease(attributes);
	CFRelease(fontNameStr);
	
	if (!descriptor) {
		return luaL_error(L, "Failed to create font descriptor");
	}
	
	CTFontRef font = CTFontCreateWithFontDescriptor(descriptor, 12.0, NULL);
	CFRelease(descriptor);
	
	if (!font) {
		return luaL_error(L, "Failed to create font for family: %s", familyName);
	}
	
	CFDataRef fontData = NULL;
	
	CFDataRef ttcfData = CTFontCopyTable(font, kCTFontTableTtcf, kCTFontTableOptionNoOptions);
	if (ttcfData) {
		fontData = ttcfData;
	} else {
		CTFontDescriptorRef fontDesc = CTFontCopyFontDescriptor(font);
		if (fontDesc) {
			CFURLRef fontURL = CTFontDescriptorCopyAttribute(fontDesc, kCTFontURLAttribute);
			if (fontURL) {
				fontData = read_font_file_data(fontURL);
				CFRelease(fontURL);
			}
			CFRelease(fontDesc);
		}
	}
	
	CFRelease(font);
	
	if (!fontData) {
		return luaL_error(L, "Failed to get font data for family: %s", familyName);
	}
	
	CFIndex dataLength = CFDataGetLength(fontData);
	char *buf = malloc(dataLength + 1);
	if (!buf) {
		CFRelease(fontData);
		return luaL_error(L, "Out of memory : sysfont");
	}
	
	CFDataGetBytes(fontData, CFRangeMake(0, dataLength), (UInt8*)buf);
	buf[dataLength] = 0;
	CFRelease(fontData);
	
	lua_pushexternalstring(L, buf, dataLength, free_data, NULL);
	return 1;
}

#elif defined(__EMSCRIPTEN__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void *
free_data(void *ud, void *ptr, size_t osize, size_t nsize) {
  return NULL;
}

static int
lttfdata(lua_State *L) {
  return 0;
}

#elif defined(__linux__)

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>

static void *
free_data(void *ud, void *ptr, size_t oszie, size_t nsize) {
	free(ptr);
	return NULL;
}

static int
lttfdata(lua_State *L) {
	const char *familyName = luaL_checkstring(L, 1);

	if (!FcInit()) {
		return luaL_error(L, "Failed to initialize fontconfig");
	}

	FcPattern *pattern = FcNameParse((const FcChar8*)familyName);
	if (!pattern) {
		FcFini();
		return luaL_error(L, "Failed to parse font name: %s", familyName);
	}

	FcConfigSubstitute(NULL, pattern, FcMatchPattern);
	FcDefaultSubstitute(pattern);

	FcResult result;
	FcPattern *match = FcFontMatch(NULL, pattern, &result);
	FcPatternDestroy(pattern);

	if (!match || result != FcResultMatch) {
		if (match) FcPatternDestroy(match);
		FcFini();
		return luaL_error(L, "Font not found: %s", familyName);
	}

	FcChar8 *filename;
	if (FcPatternGetString(match, FC_FILE, 0, &filename) != FcResultMatch) {
		FcPatternDestroy(match);
		FcFini();
		return luaL_error(L, "Failed to get font file path for: %s", familyName);
	}

	FILE *file = fopen((const char*)filename, "rb");
	if (!file) {
		FcPatternDestroy(match);
		FcFini();
		return luaL_error(L, "Failed to open font file: %s", filename);
	}

	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (fileSize <= 0) {
		fclose(file);
		FcPatternDestroy(match);
		FcFini();
		return luaL_error(L, "Invalid font file size: %s", filename);
	}

	char *buf = malloc(fileSize + 1);
	if (!buf) {
		fclose(file);
		FcPatternDestroy(match);
		FcFini();
		return luaL_error(L, "Out of memory : sysfont");
	}

	size_t bytesRead = fread(buf, 1, fileSize, file);
	fclose(file);
	FcPatternDestroy(match);
	FcFini();

	if (bytesRead != fileSize) {
		free(buf);
		return luaL_error(L, "Failed to read font file: %s", filename);
	}

	buf[fileSize] = 0;

	lua_pushexternalstring(L, buf, fileSize, free_data, NULL);
	return 1;
}

#else

static int
lttfdata(lua_State *L) {
	return luaL_error(L, "Unsupport system font");
}

#endif

int
luaopen_font_system(lua_State *L) {
	luaL_Reg l[] = {
		{ "ttfdata", lttfdata },
		{ NULL, NULL },
	};
	luaL_newlib(L, l);
	return 1;
}

