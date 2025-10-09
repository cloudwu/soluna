.PHONY : all clean shader

BUILD=build
BIN=bin
APPNAME=soluna.exe
CC?=gcc
# msvc support
#CC=cl
LD=$(CC)
LUA_EXE=$(BUILD)/lua.exe
SHDC=sokol-shdc.exe
VERSION=$(shell git rev-parse HEAD)

#for msvc
ifeq ($(CC),cl)
 CCPP=cl
 CFLAGS=-utf-8 -W3 -O2
 OUTPUT_O=-c -Fo:
 OUTPUT_EXE=-Fe:
 STDC=-std:c11 -experimental:c11atomics
 STDCPP=-std:c++20
 SUBSYSTEM=-LINK -SUBSYSTEM:WINDOWS -ENTRY:"mainCRTStartup"
 LDFLAGS=$(SUBSYSTEM) xinput.lib Ws2_32.lib ntdll.lib Imm32.lib
else
 CCPP=g++
 CFLAGS=-Wall -O2
 OUTPUT_O=-c -o
 OUTPUT_EXE=-o
 STDC=-std=c99 -lm
 STDCPP=-std=c++20
 SUBSYSTEM=-Wl,-subsystem,windows
 LDFLAGS=-lkernel32 -luser32 -lshell32 -lgdi32 -ldxgi -ld3d11 -lwinmm -lws2_32 -lntdll -lxinput -limm32 -lstdc++ $(SUBSYSTEM)
endif

all : $(BIN)/$(APPNAME)

3RDINC=-I3rd
YOGAINC=-I3rd/yoga

LUAINC=-I3rd/lua
LUASRC:=$(wildcard 3rd/lua/*.c 3rd/lua/*.h)
WINFILE:=src/winfile.c

$(LUA_EXE) : $(LUASRC) $(WINFILE)
	$(CC) $(CFLAGS) -o $@ 3rd/lua/onelua.c $(WINFILE) -DMAKE_LUA -Dfopen=fopen_utf8

COMPILE_C=$(CC) $(CFLAGS) $(STDC) $(OUTPUT_O) $@ $<
COMPILE_LUA=$(LUA_EXE) script/lua2c.lua $< $@
COMPILE_DATALIST=$(LUA_EXE) script/datalist2c.lua $< $@

LUA_O=$(BUILD)/onelua.o

$(LUA_O) : $(LUASRC)
	$(CC) $(CFLAGS) $(OUTPUT_O) $@ 3rd/lua/onelua.c -DMAKE_LIB -Dfopen=fopen_utf8

SHADER_SRC=$(wildcard src/*.glsl)
SHADER_O=$(patsubst src/%.glsl,$(BUILD)/%.glsl.h,$(SHADER_SRC))
SHADERINC=-I$(BUILD)

$(BUILD)/%.glsl.h : src/%.glsl
	$(SHDC) --input $< --output $@ --slang hlsl4 --format sokol

shader : $(SHADER_O)

MAIN_FULL=$(wildcard src/*.c)
MAIN_C=$(notdir $(MAIN_FULL))
MAIN_O=$(patsubst %.c,$(BUILD)/soluna_%.o,$(MAIN_C))

$(MAIN_O) : $(SHADER_O)

LTASK_FULL=$(wildcard 3rd/ltask/src/*.c)
LTASK_C=$(notdir $(LTASK_FULL))
LTASK_O=$(patsubst %.c,$(BUILD)/ltask_%.o,$(LTASK_C))

LTASK_LUASRC=\
  3rd/ltask/service/root.lua\
  3rd/ltask/service/timer.lua\
  $(wildcard 3rd/ltask/lualib/*.lua src/lualib/*.lua src/service/*.lua)

LTASK_LUACODE=$(patsubst %.lua, $(BUILD)/%.lua.h, $(notdir $(LTASK_LUASRC)))

DATALIST_SRC=$(wildcard src/data/*.dl)

DATALIST_CODE=$(patsubst %.dl, $(BUILD)/%.dl.h, $(notdir $(DATALIST_SRC)))

ZLIB_INC=-I3rd/zlib
ZLIB_FULL=$(wildcard 3rd/zlib/*.c)
ZLIB_C = $(notdir $(ZLIB_FULL))
ZLIB_O = $(patsubst %.c,$(BUILD)/zlib_%.o,$(ZLIB_C))
MINIZIP_FULL=\
  3rd\zlib\contrib/minizip/ioapi.c\
  3rd\zlib\contrib/minizip/unzip.c\
  3rd\zlib\contrib/minizip/zip.c\
  3rd\zlib\contrib/minizip/iowin32.c
MINIZIP_C = $(notdir $(MINIZIP_FULL))
MINIZIP_O = $(patsubst %.c,$(BUILD)/minizip_%.o,$(MINIZIP_C))

$(LTASK_LUACODE) $(DATALIST_CODE) : | $(LUA_EXE)

$(BUILD)/%.lua.h : 3rd/ltask/service/%.lua
	$(COMPILE_LUA)

$(BUILD)/%.lua.h : 3rd/ltask/lualib/%.lua
	$(COMPILE_LUA)

$(BUILD)/%.lua.h : src/lualib/%.lua
	$(COMPILE_LUA)

$(BUILD)/%.lua.h : src/service/%.lua
	$(COMPILE_LUA)

$(BUILD)/%.dl.h : src/data/%.dl
	$(COMPILE_DATALIST)

$(BUILD)/soluna_embedlua.o : src/embedlua.c $(LTASK_LUACODE) $(DATALIST_CODE)
	$(COMPILE_C) -I$(BUILD) $(LUAINC)

$(BUILD)/soluna_entry.o : src/entry.c src/version.h
	$(COMPILE_C) $(LUAINC) $(3RDINC) -DSOLUNA_HASH_VERSION=\"$(VERSION)\"

$(BUILD)/soluna_%.o : src/%.c
	$(COMPILE_C) $(LUAINC) $(3RDINC) $(SHADERINC) $(YOGAINC)
	
$(BUILD)/ltask_%.o : 3rd/ltask/src/%.c
	$(COMPILE_C) $(LUAINC) -D_WIN32_WINNT=0x0601 -DLTASK_EXTERNAL_OPENLIBS=soluna_openlibs
	
DATALIST_O=$(BUILD)/datalist.o

$(DATALIST_O) : 3rd/datalist/datalist.c
	$(COMPILE_C) $(LUAINC)
	
YOGASRC:=$(wildcard 3rd/yoga/yoga/*.cpp $(addsuffix *.cpp,$(wildcard 3rd/yoga/yoga/*/)))

$(BUILD)/yoga.o : src/yogaone.cpp $(YOGASRC)
	$(CCPP) $(STDCPP) $(OUTPUT_O) $@ $< $(YOGAINC) $(CFLAGS)
	
$(BUILD)/zlib_%.o : 3rd/zlib/%.c
	$(COMPILE_C) $(ZLIB_INC)

$(BUILD)/minizip_%.o : 3rd/zlib/contrib/minizip/%.c
	$(COMPILE_C) $(ZLIB_INC)

$(BIN)/$(APPNAME): $(MAIN_O) $(LTASK_O) $(LUA_O) $(DATALIST_O) $(BUILD)/yoga.o $(ZLIB_O) $(MINIZIP_O)
	$(LD) $(OUTPUT_EXE) $@ $^ $(LDFLAGS)
	
clean :
	rm -f $(BIN)/*.exe $(BUILD)/*.o $(BUILD)/*.h
