# HOME Menu forwarder: a small CIA that starts sdmc:/3ds/twinembers/twinembers.3dsx
# through Luma3DS's hb:ldr, so updates only replace the 3DSX.
# Adapted from ZallaxDev's pokeemerald-3Ds-dualscreen forwarder (MIT, NOTICE.md).
#
#   make -C 3ds_port/forwarder            -> dist/TwinEmbers-Forwarder.cia
#
# Needs devkitARM + libctru, Python 3, makerom and bannertool (on PATH, in
# ../tools-bin, or given as MAKEROM= / BANNERTOOL=). No artwork is stored in the
# repository (its source audit admits only text):
#   ICON_SRC=icon.jpg BANNER_SRC=logo.jpg   the owner's pictures, kept outside the
#                                           repository, made into the icon and a
#                                           transparent banner by prepare_art.py (Pillow)
#   (neither)                               two flames drawn by make_art.py
.SUFFIXES:
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB := $(DEVKITPRO)/libctru
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
PYTHON ?= python3
# The game's version (../Makefile), shown in the description and on the forwarder's own screen.
GAME_VERSION ?= $(shell sed -n 's/^TWIN_EMBERS_VERSION := //p' ../Makefile)
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := $(ARCH) -O2 -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -D__3DS__ \
	-I$(CTRULIB)/include -I../include -DTWIN_EMBERS_VERSION='"$(GAME_VERSION)"'
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -L$(CTRULIB)/lib
LIBS := -lctru -lm

TOOLS_BIN := $(abspath ../tools-bin)
EXE := $(if $(filter Windows_NT,$(OS)),.exe,)
MAKEROM ?= $(firstword $(wildcard $(TOOLS_BIN)/makerom$(EXE)) makerom)
BANNERTOOL ?= $(firstword $(wildcard $(TOOLS_BIN)/bannertool$(EXE)) bannertool)

APP_TITLE := Twin Embers
APP_DESCRIPTION := Twin Embers v$(GAME_VERSION), a FireRed port for the 3DS (shortcut)
APP_AUTHOR := alex61194
# The CIA's own version, shown by FBI; bump with the forwarder, not with the game.
APP_VERSION ?= 1.1.0
# Fraction cut from each side of ICON_SRC (0 for a picture that already is the 48x48 icon).
ICON_CROP ?= 0.09
CIA := dist/TwinEmbers-Forwarder.cia
ART := build/art

all: $(CIA)

build/forwarder.elf: source/main.c ../include/3ds_forwarder.h Makefile ../Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(LIBS) -o $@

ifneq ($(and $(ICON_SRC),$(BANNER_SRC)),)
$(ART)/icon.png $(ART)/banner.png $(ART)/banner.wav &: prepare_art.py make_art.py $(ICON_SRC) $(BANNER_SRC)
	$(PYTHON) prepare_art.py --icon "$(ICON_SRC)" --icon-crop $(ICON_CROP) --banner "$(BANNER_SRC)" $(ART)
else
$(ART)/icon.png $(ART)/banner.png $(ART)/banner.wav &: make_art.py
	$(PYTHON) make_art.py $(ART)
endif

build/icon.smdh: $(ART)/icon.png Makefile ../Makefile
	$(BANNERTOOL) makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i $(ART)/icon.png -o $@

build/banner.bnr: $(ART)/banner.png $(ART)/banner.wav
	$(BANNERTOOL) makebanner -i $(ART)/banner.png -a $(ART)/banner.wav -o $@

$(CIA): build/forwarder.elf build/banner.bnr build/icon.smdh forwarder-rsf.yml
	@mkdir -p dist
	$(MAKEROM) -f cia -o $@ -target t -exefslogo -rsf forwarder-rsf.yml -elf build/forwarder.elf \
		-banner build/banner.bnr -icon build/icon.smdh \
		-major $(word 1,$(subst ., ,$(APP_VERSION))) -minor $(word 2,$(subst ., ,$(APP_VERSION))) \
		-micro $(word 3,$(subst ., ,$(APP_VERSION)))

clean:
	rm -rf build dist

.PHONY: all clean
