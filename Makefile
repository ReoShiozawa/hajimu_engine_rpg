# =============================================================================
# engine_rpg — はじむ言語用プラグイン
# クロスプラットフォーム Makefile (macOS / Linux / Windows MinGW)
# =============================================================================

PLUGIN_NAME = engine_rpg
BUILD_DIR   = build
OUTPUT      = $(BUILD_DIR)/$(PLUGIN_NAME).hjp

# OS 判定 ($(OS) は Windows CMD/PowerShell で "Windows_NT" になる)
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    INSTALL_DIR := $(USERPROFILE)/.hajimu/plugins
    NCPU        := $(NUMBER_OF_PROCESSORS)
else
    DETECTED_OS := $(shell uname -s 2>/dev/null || echo Unknown)
    INSTALL_DIR := $(HOME)/.hajimu/plugins
    NCPU        := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
endif

CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Release -Wno-dev
.PHONY: all clean install uninstall

all: $(OUTPUT)

$(OUTPUT): CMakeLists.txt
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS)
	cmake --build $(BUILD_DIR) -j$(NCPU)
	@echo "  ビルド完了: $(OUTPUT)"
clean:
ifeq ($(OS),Windows_NT)
	-rmdir /S /Q $(BUILD_DIR) 2>NUL
else
	rm -rf $(BUILD_DIR)
endif
	@echo "  クリーン完了"
install: all
ifeq ($(OS),Windows_NT)
	if not exist "$(INSTALL_DIR)\$(PLUGIN_NAME)" mkdir "$(INSTALL_DIR)\$(PLUGIN_NAME)"
	copy /Y $(OUTPUT) "$(INSTALL_DIR)\$(PLUGIN_NAME)"
else
	@mkdir -p $(INSTALL_DIR)/$(PLUGIN_NAME)
	cp $(OUTPUT) $(INSTALL_DIR)/$(PLUGIN_NAME)/
endif
	@echo "  インストール完了: $(INSTALL_DIR)/$(PLUGIN_NAME)/"

uninstall:
ifeq ($(OS),Windows_NT)
	-rmdir /S /Q "$(INSTALL_DIR)\$(PLUGIN_NAME)" 2>NUL
else
	rm -rf $(INSTALL_DIR)/$(PLUGIN_NAME)
endif
	@echo "  アンインストール完了"

# ── クロスプラットフォームビルド (macOS ホストでビルドして配布) ──
DIST        = dist
JP_CMAKE    = $(abspath $(firstword $(wildcard ../../jp/cmake ../jp/cmake)))
HAJIMU_INC  = $(abspath $(firstword $(wildcard ../../jp/include ../jp/include ./include)))
LINUX_TC    = $(JP_CMAKE)/toolchain_linux_x64.cmake
WIN_TC      = $(JP_CMAKE)/toolchain_windows_x64.cmake
CROSS_FLAGS = -DCMAKE_BUILD_TYPE=Release -Wno-dev -DHAJIMU_INCLUDE_DIR=$(HAJIMU_INC)

.PHONY: build-all build-macos build-linux build-windows

build-all: build-macos build-linux build-windows
	@echo "  全プラットフォームビルド完了: $(DIST)/"

build-macos:
	@mkdir -p $(DIST)
	cmake -S . -B build_macos $(CROSS_FLAGS)
	cmake --build build_macos -j$(NCPU)
	cp build_macos/$(PLUGIN_NAME).hjp $(DIST)/$(PLUGIN_NAME)-macos.hjp
	@echo "  macOS: $(DIST)/$(PLUGIN_NAME)-macos.hjp"

build-linux:
	@mkdir -p $(DIST)
	cmake -S . -B build_linux $(CROSS_FLAGS) -DCMAKE_TOOLCHAIN_FILE=$(LINUX_TC)
	cmake --build build_linux -j$(NCPU)
	cp build_linux/$(PLUGIN_NAME).hjp $(DIST)/$(PLUGIN_NAME)-linux-x64.hjp
	@echo "  Linux: $(DIST)/$(PLUGIN_NAME)-linux-x64.hjp"

build-windows:
	@mkdir -p $(DIST)
	cmake -S . -B build_win $(CROSS_FLAGS) -DCMAKE_TOOLCHAIN_FILE=$(WIN_TC)
	cmake --build build_win -j$(NCPU)
	cp build_win/$(PLUGIN_NAME).hjp $(DIST)/$(PLUGIN_NAME)-windows-x64.hjp
	@echo "  Windows: $(DIST)/$(PLUGIN_NAME)-windows-x64.hjp"
