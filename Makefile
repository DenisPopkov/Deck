# Deck — Linux build helpers (CMake + Ninja).
# Full guide: scripts/BUILD-LINUX.md

BUILD_DIR ?= build
CONFIG ?= Release
JOBS ?= $(shell nproc 2>/dev/null || echo 4)
GENERATOR ?= Ninja
VERSION := $(shell sed -n 's/^project(Deck VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)

.PHONY: all configure configure-pi build build-pi run test package clean distclean deps-help help

all: build

help:
	@echo "Deck Linux targets:"
	@echo "  make deps-help   show distro packages for JUCE GUI build"
	@echo "  make configure   run CMake (Ninja, Release, Pi tape OFF)"
	@echo "  make configure-pi / build-pi   local Pi tape dev build only"
	@echo "  make build       compile Deck"
	@echo "  make run         launch built binary"
	@echo "  make test        build and run DeckTests"
	@echo "  make package     create dist/Deck-$(VERSION)-Linux.tar.gz"
	@echo "  make clean       remove build artifacts"
	@echo ""
	@echo "Variables: BUILD_DIR=$(BUILD_DIR) CONFIG=$(CONFIG) JOBS=$(JOBS)"

deps-help:
	@echo "Debian / Ubuntu:"
	@echo "  sudo apt-get install -y build-essential cmake ninja-build git pkg-config \\"
	@echo "    libasound2-dev libfreetype6-dev libfontconfig1-dev libcurl4-openssl-dev \\"
	@echo "    libx11-dev libxrandr-dev libxcursor-dev libxinerama-dev libxext-dev libxrender-dev \\"
	@echo "    libgl-dev libglu1-mesa-dev"
	@echo ""
	@echo "Fedora:"
	@echo "  sudo dnf install -y gcc-c++ cmake ninja-build git pkg-config \\"
	@echo "    alsa-lib-devel freetype-devel fontconfig-devel libcurl-devel \\"
	@echo "    libX11-devel libXrandr-devel libXcursor-devel libXinerama-devel libXext-devel libXrender-devel \\"
	@echo "    mesa-libGL-devel mesa-libGLU-devel"
	@echo ""
	@echo "Optional (GstPEAQ runtime): gstreamer1.0-plugins-bad"

configure:
	cmake -C cmake/ProdOptions.cmake -B $(BUILD_DIR) -G $(GENERATOR) -DCMAKE_BUILD_TYPE=$(CONFIG) -DCASSETTE_BUILD_TESTS=OFF

configure-pi:
	cmake -C cmake/PiTapeDevOptions.cmake -B build-pi -G $(GENERATOR) -DCMAKE_BUILD_TYPE=$(CONFIG) -DCASSETTE_BUILD_TESTS=ON

build-pi: configure-pi
	cmake --build build-pi --target Deck --parallel $(JOBS)

build: configure
	cmake --build $(BUILD_DIR) --target Deck --parallel $(JOBS)
	@echo ""
	@echo "Built: $$(./scripts/find_deck_binary.sh $(BUILD_DIR) $(CONFIG))"

run: build
	@./scripts/find_deck_binary.sh $(BUILD_DIR) $(CONFIG) --run

test: configure
	cmake -C cmake/ProdOptions.cmake -B $(BUILD_DIR) -G $(GENERATOR) -DCMAKE_BUILD_TYPE=$(CONFIG) -DCASSETTE_BUILD_TESTS=ON
	cmake --build $(BUILD_DIR) --target DeckTests --parallel $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure

package: build
	@./scripts/package_linux.sh $(BUILD_DIR) $(CONFIG) $(VERSION)

clean:
	cmake --build $(BUILD_DIR) --target clean 2>/dev/null || true

distclean:
	rm -rf $(BUILD_DIR) dist/Linux
