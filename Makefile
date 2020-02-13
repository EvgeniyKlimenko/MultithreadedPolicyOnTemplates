SRC		:= src
INCLUDE	:= include
LIB		:= lib

ifeq ($(OS),Windows_NT)
SEP := \\
SYSTEM := Windows
BIN := bin$(SEP)$(SYSTEM)$(SEP)
BUILD := build$(SEP)$(SYSTEM)$(SEP)
CC	:= cl.exe
C_FLAGS := /DWIN64 /DDEBUG /D_CRT_SECURE_NO_DEPRECATE /ZI /W4 /EHsc /GR /Fo$(BUILD) /Fa$(BUILD) /Fd$(BUILD) /Fm$(BUILD) /std:c++17
L_FLAGS := /MTd
L_LIBS :=
MSVC_HEADERS_PATH := "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\MSVC\\14.24.28314\\include"
SHARED_HEADERS_PATH := "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.18362.0\\shared"
CRT_HEADERS_PATH := "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.18362.0\\ucrt"
SDK_HEADERS_PATH := "C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.18362.0\\um"
INCLUDE_DIRS := /I $(MSVC_HEADERS_PATH) /I $(SHARED_HEADERS_PATH) /I $(CRT_HEADERS_PATH) /I $(SDK_HEADERS_PATH) /I $(INCLUDE)
MSVC_LIBS_PATH := "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.24.28314\\lib\\x64"
CRT_LIBS_PATH := "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.18362.0\\ucrt\\x64"
SDK_LIBS_PATH := "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.18362.0\\um\\x64"
LIB_DIRS := /LIBPATH:$(MSVC_LIBS_PATH) /LIBPATH:$(CRT_LIBS_PATH) /LIBPATH:$(SDK_LIBS_PATH)
L_OPTS := /link /machine:X64 $(LIB_DIRS)
OUT_FILE := /Fe:
SEP := \\
EXECUTABLE	:= TemplatePolicyDemo.exe
RM := del /f /s /q
MKDIR := md
CREATE_BIN_DIR := if not exist "$(BIN)" $(MKDIR) $(BIN)
CREATE_BUILD_DIR := if not exist "$(BUILD)" $(MKDIR) $(BUILD)
else
SEP := /
SYSTEM := Linux
BIN := bin$(SEP)$(SYSTEM)$(SEP)
BUILD := build$(SEP)$(SYSTEM)$(SEP)
CC := g++
C_FLAGS := -std=c++17 -Wall -Wextra -g
L_FLAGS :=
L_LIBS := -pthread
L_OPTS :=
OUT_FILE := -o
INCLUDE_DIRS := -I$(INCLUDE)
LIB_DIRS := -L$(LIB)
EXECUTABLE := TemplatePolicyDemo
RM := rm -rf
MKDIR := mkdir
CREATE_BIN_DIR := if [ ! -e "$(BIN)" ];then $(MKDIR) $(BIN); fi;
CREATE_BUILD_DIR := if [ ! -e "$(BUILD)" ];then $(MKDIR) $(BUILD); fi;
endif

all: $(BIN)$(EXECUTABLE)

clean:
	$(RM) $(BIN)$(EXECUTABLE)
	$(RM) $(BUILD)

run: all
	./$(BIN)$(EXECUTABLE)

$(BIN)$(EXECUTABLE): $(SRC)/*.cpp
	$(CREATE_BIN_DIR)
	$(CREATE_BUILD_DIR)
	$(CC) $(C_FLAGS) $(INCLUDE_DIRS)  $(L_FLAGS) $(L_LIBS) $^ $(OUT_FILE) $@ $(L_OPTS)