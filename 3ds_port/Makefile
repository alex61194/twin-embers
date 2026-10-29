.SUFFIXES:
SHELL := bash
.SHELLFLAGS := -o pipefail -c
.DEFAULT_GOAL := clean-game-3dsx

DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CTRULIB := $(DEVKITPRO)/libctru
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
# pret's tools are built with .exe only on Windows (tools/*/Makefile).
HOST_EXE := $(if $(filter Windows_NT,$(OS)),.exe,)
PREPROC := $(abspath ../tools/preproc/preproc$(HOST_EXE))
PYTHON ?= $(if $(filter Windows_NT,$(OS)),py -3,python3)

TARGET := twinembers_bootstrap
ARCH := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := $(ARCH) -O2 -g -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -DCTR_VOXEL_ENABLED=0 -DCTR_PERF_ENABLED=1 -Iinclude -Ibuild -I$(CTRULIB)/include
CFLAGS += -DCTR_EXTERNAL_ASSETS=1
# 0 keeps the sound engine on the game thread (A/B testing on hardware).
CTR_AUDIO_WORKER ?= 1
CFLAGS += -DCTR_AUDIO_WORKER=$(CTR_AUDIO_WORKER)
# 0 keeps NDSP's linear interpolation instead of polyphase.
CTR_AUDIO_POLYPHASE ?= 1
CFLAGS += -DCTR_AUDIO_POLYPHASE=$(CTR_AUDIO_POLYPHASE)
# 0 keeps the PSG (Game Boy channels) point-sampled instead of band-limited.
CTR_AUDIO_PSG_BLEP ?= 1
CFLAGS += -DCTR_AUDIO_PSG_BLEP=$(CTR_AUDIO_PSG_BLEP)
# 1 makes a New 3DS play as an Old 3DS (268 MHz, no L2, no core 2, Old 3DS
# settings) and writes port.log without debug.txt: a build to measure Old 3DS
# speed. The file sdmc:/3ds/twinembers/force_old3ds does the same at run time.
CTR_FORCE_OLD3DS ?= 0
CFLAGS += -DCTR_FORCE_OLD3DS=$(CTR_FORCE_OLD3DS)
LDFLAGS := $(ARCH) -specs=3dsx.specs -Wl,--gc-sections -L$(CTRULIB)/lib
# Every link that has src/3ds_video.c and src/3ds_gpu_flush.c, which routes the GPU data cache flushes past the GSP service.
VIDEO_LDFLAGS := -Wl,--wrap=GSPGPU_FlushDataCache

.PHONY: all clean verify verify-host verify-game-core verify-title-units verify-game-source verify-game-data verify-sound-data verify-sound-songs verify-m4a-arm11 verify-naming-exit verify-map-load-wait verify-battle-transition-exit link-game-probe verify-game-relocs romfs-bundles game-3dsx verify-game-3dsx
all: $(TARGET).3dsx

build/%.o: src/%.c Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# The version this checkout builds, shown on the title screen as v$(TWIN_EMBERS_VERSION). Change it here when a release starts.
TWIN_EMBERS_VERSION := 0.3.0
.PHONY: force-git-version
build/git_version.h: force-git-version ../tools/gen_git_version.py
	$(PYTHON) ../tools/gen_git_version.py ../../.. $@ --version $(TWIN_EMBERS_VERSION)
build/main_game_full.o: build/git_version.h

VOXEL := 0
ifneq ($(VOXEL),0)
$(error Voxel is disabled for FireRed 3DS)
endif

$(TARGET).elf: build/main.o build/gba_shadow.o build/3ds_video.o build/3ds_video_decode.o build/3ds_input.o build/3ds_input_decode.o build/3ds_platform_min.o build/3ds_perf.o build/3ds_gpu_flush.o build/3ds_fast_cells.o
	$(CC) $(LDFLAGS) $(VIDEO_LDFLAGS) $^ -lcitro2d -lcitro3d -lctru -lm -o $@

$(TARGET).smdh: Makefile
	$(DEVKITPRO)/tools/bin/smdhtool --create "FireRed 3DS port" "ARM11 bootstrap diagnostic" "FireRed 3DS contributors" $(CTRULIB)/default_icon.png $@

$(TARGET).3dsx: $(TARGET).elf $(TARGET).smdh
	$(DEVKITPRO)/tools/bin/3dsxtool $< $@ --smdh=$(TARGET).smdh

verify: all verify-host verify-game-core build/gpu_regs_probe.o build/main_game_probe.o build/game_bridge_probe.o build/3ds_bios.o build/3ds_dma.o
	$(DEVKITARM)/bin/arm-none-eabi-readelf -h $(TARGET).elf | grep -q 'Machine:.*ARM'
	@test -s $(TARGET).3dsx

verify-host: build/gba_shadow_test.exe build/video_decode_test.exe build/gba_alias_test.exe build/input_test.exe build/bios_test.exe build/bios_semantics_test.exe build/bios_math_test.exe build/dma_test.exe build/flash_test.exe build/rex_test.exe build/obj_window_test.exe build/map_edge_test.exe build/poketouch_options_test.exe build/forwarder_test.exe build/type_chart_test.exe build/leafgreen_test.exe build/night_wild_test.exe build/audio_cadence_test.exe build/audio_catchup_test.exe build/vram_track_test.exe build/fast_forward_test.exe build/settings_test.exe build/palette_fade_test.exe build/day_night_test.exe build/data_error_text_test.exe build/cgb_audio_test.exe build/cgb_blep_test.exe build/audio_mix_test.exe build/battle_gen4_test.exe
	./build/gba_shadow_test.exe
	./build/video_decode_test.exe
	./build/gba_alias_test.exe
	./build/input_test.exe
	./build/bios_test.exe
	./build/bios_semantics_test.exe
	./build/bios_math_test.exe
	./build/dma_test.exe
	./build/flash_test.exe
	./build/rex_test.exe
	./build/obj_window_test.exe
	./build/map_edge_test.exe
	./build/poketouch_options_test.exe
	./build/forwarder_test.exe
	./build/type_chart_test.exe
	./build/leafgreen_test.exe
	./build/battle_gen4_test.exe
	./build/night_wild_test.exe
	./build/audio_cadence_test.exe
	./build/audio_catchup_test.exe
	./build/vram_track_test.exe
	./build/fast_forward_test.exe
	./build/settings_test.exe
	./build/palette_fade_test.exe
	./build/day_night_test.exe
	./build/data_error_text_test.exe
	./build/cgb_audio_test.exe
	./build/cgb_blep_test.exe
	./build/audio_mix_test.exe

verify-game-core: build/game/task.o build/game/bg.o build/game/sprite.o

# This extra gate builds graphics locally from pret's source, then checks that
# the real intro/title translation units can target ARM11. It is not a ROM.
verify-title-units: build/title_assets.stamp
	$(MAKE) build/game/intro.o build/game/title_screen.o

# Compile the actual FireRed translation units, without linking them yet.
# This exposes ARM11 compatibility gaps across the whole game instead of only
# the intro/title probes. The .inc.c fragments are included by their owners.
# isagbprn writes to emulator-only GBA debug addresses and is disabled by
# NDEBUG in FireRed's main loop.
GAME_C_SRCS := $(filter-out %.inc.c ../src/isagbprn.c ../src/agb_flash%.c,$(wildcard ../src/*.c ../src/*/*.c ../src/*/*/*.c))
GAME_C_OBJS := $(patsubst ../src/%.c,build/game/%.o,$(GAME_C_SRCS))
# Serialized layout changes must reach every game unit; this compile pipeline
# preprocesses to stdin and does not otherwise emit header dependency files.
$(GAME_C_OBJS): ../include/global.h ../include/load_save.h ../include/gba/defines.h ../include/fieldmap.h $(wildcard compat/*.h)
build/game/main.o build/game/task.o build/game/sprite.o build/game_bridge_probe.o: include/3ds_perf.h
verify-game-source: build/game_sources.stamp
	$(MAKE) $(GAME_C_OBJS)

GAME_DATA_SRCS := $(filter-out ../data/multiboot_%.s ../data/sound_data.s,$(wildcard ../data/*.s))
GAME_DATA_OBJS := $(patsubst ../data/%.s,build/game_data/%.o,$(GAME_DATA_SRCS))
verify-game-data: build/game_sources.stamp
	$(MAKE) $(GAME_DATA_OBJS)

verify-sound-data: build/sound_assets.stamp
	$(MAKE) build/game_data/sound_data.o

GAME_SONG_MIDIS := $(wildcard ../sound/songs/midi/*.mid)
GAME_SONG_OBJS := $(patsubst ../sound/songs/midi/%.mid,build/game_song/%.o,$(GAME_SONG_MIDIS))
GAME_SOUND_OBJS := build/game_data/sound_data.o $(GAME_SONG_OBJS)
# Real FireRed sound engine (Thumb/ARM mixer, sequencer, VSync). This is the
# actual game code from ../src/m4a_1.s, not a stub; m4a.c calls into it.
GAME_M4A_OBJS := build/game/m4a_1.o
verify-sound-songs: build/sound_assets.stamp
	$(MAKE) $(GAME_SONG_OBJS)

# Real AgbMain link attempt. The target is separate from the runnable 2D
# diagnostic until every game/platform dependency is resolved.
# GAME_PLATFORM_OBJS is shared; each image adds its own main entry.
GAME_PLATFORM_OBJS := build/gba_shadow.o build/3ds_bios.o build/3ds_bios_math.o build/3ds_game_memory.o build/3ds_dma.o build/game_bridge_probe.o
GAME_PLATFORM_OBJS += build/3ds_flash.o build/3ds_rng.o build/3ds_rng_platform.o
GAME_PLATFORM_OBJS += build/3ds_link_peripherals.o
GAME_PLATFORM_OBJS += build/3ds_cgb_audio.o build/3ds_audio_mix.o build/3ds_audio.o
GAME_PLATFORM_OBJS += build/3ds_video.o build/3ds_gpu_flush.o build/3ds_fast_cells.o build/3ds_video_decode.o build/3ds_input.o build/3ds_input_decode.o build/3ds_platform_min.o
GAME_PLATFORM_OBJS += build/3ds_perf.o
# LeafGreen's version exclusives in the wild encounters (wild_encounter.c calls it).
GAME_PLATFORM_OBJS += build/3ds_leafgreen.o
build/3ds_leafgreen.o: CFLAGS += -iquote ../include
build/3ds_leafgreen.o: include/3ds_leafgreen.h
# The touch MENU's own text and the lower screen's Party message (field_message_box.c, help_system.c, party_menu.c).
GAME_PLATFORM_OBJS += build/3ds_touch_text.o
build/3ds_touch_text.o: CFLAGS += -iquote ../include
build/3ds_touch_text.o: include/3ds_touch_text.h
# Night encounters (wild_encounter.c); the map constants are generated with the game sources.
# The turbo and auto-run that are chosen, kept on the SD card (3ds_game_bridge.c, ctr_poketouch.c).
GAME_PLATFORM_OBJS += build/3ds_settings.o
build/3ds_settings.o: include/3ds_settings.h
GAME_PLATFORM_OBJS += build/3ds_night_wild.o
build/3ds_night_wild.o: CFLAGS += -iquote ../include
build/3ds_night_wild.o: include/3ds_night_wild.h build/game_sources.stamp
GAME_PLATFORM_OBJS += build/3ds_bottom.o build/3ds_bottom_screen.o build/3ds_poketouch.o build/3ds_poketouch_options.o
GAME_NATIVE_OBJS := build/main_game_native.o $(GAME_PLATFORM_OBJS)
GAME_NATIVE_OBJS += build/3ds_assets.o build/3ds_asset_memory.o build/3ds_data.o build/3ds_pak.o build/3ds_data_error.o
# HeartGold's battle sprites from the pack's optional entries, drawn by the compositor (battle_gfx_sfx_util.c).
GAME_NATIVE_OBJS += build/3ds_battle_gen4.o
build/3ds_battle_gen4.o: include/3ds_battle_gen4.h include/3ds_data.h
verify-m4a-arm11: build/game/m4a_1.o ../tools/verify_m4a_arm11.py
	$(PYTHON) ../tools/verify_m4a_arm11.py --object build/game/m4a_1.o --objcopy $(DEVKITARM)/bin/arm-none-eabi-objcopy --readelf $(DEVKITARM)/bin/arm-none-eabi-readelf

verify-naming-exit:
	$(PYTHON) ../tools/verify_naming_exit.py

verify-map-load-wait:
	$(PYTHON) ../tools/verify_map_load_wait.py

verify-battle-transition-exit:
	$(PYTHON) ../tools/verify_battle_transition_exit.py

link-game-probe: verify-game-source verify-game-data verify-sound-data verify-sound-songs verify-m4a-arm11 verify-naming-exit verify-map-load-wait verify-battle-transition-exit $(GAME_NATIVE_OBJS)
	$(MAKE) build/firered_game_probe.elf

# NUM_MUSIC_PLAYERS (= 4) and MAX_LINES (= 0) come from
# patches/pokefirered/0004-m4a-3ds-constants.patch as plain PLATFORM_3DS
# constants, so no --defsym absolute symbols are needed in the 3DS link.
build/firered_game_probe.elf: $(GAME_C_OBJS) $(GAME_DATA_OBJS) $(GAME_SOUND_OBJS) $(GAME_M4A_OBJS) $(GAME_NATIVE_OBJS)
	$(file >build/game-link.rsp,$(GAME_C_OBJS) $(GAME_DATA_OBJS) $(GAME_SOUND_OBJS) $(GAME_M4A_OBJS) $(GAME_NATIVE_OBJS))
	$(CC) $(LDFLAGS) $(VIDEO_LDFLAGS) -Wl,-Map,build/firered_game_probe.map @build/game-link.rsp -lcitro2d -lcitro3d -lctru -lm -o $@

# CHECKPOINT B: drop only the verified vestigial R_ARM_THM_CALL branch
# relocations (statically resolved, PC-relative, ignored by 3dsxtool's own
# type switch) that trip its blanket alignment check. The diagnostic ELF is
# never modified; the tool aborts unless every condition holds and the set
# is exactly the expected one, so future deviations fail the build loudly.
# The link carries 1 dead ARM->Thumb veneer instead of 2: m4aSoundMain calls
# SoundMain via direct BLX, so ld emits no __SoundMain_from_arm stub.
# The 14 unaligned THM_CALL relocs are unchanged and still verified.
build/firered_game_probe_3dsx.elf: build/firered_game_probe.elf ../tools/filter_3dsx_relocs.py
	$(PYTHON) ../tools/filter_3dsx_relocs.py --expect-thunks 1 build/firered_game_probe.elf build/firered_game_probe_3dsx.elf

# C2: the 3dsx-bound game image. Every game object is linked; the tables listed in
# the recipe are NOBITS (tools/rex_objects.py) and arrive from the pack at start-up.
GAME_FULL_OBJS := build/main_game_full.o build/3ds_rex.o build/3ds_rex_core.o build/3ds_data.o build/3ds_pak.o build/3ds_data_error.o $(GAME_PLATFORM_OBJS)
GAME_FULL_OBJS += build/3ds_assets.o
GAME_FULL_OBJS += build/3ds_battle_gen4.o
# m4a on a worker core: the game's calls into the engine go through
# src/3ds_sound.c, which holds the worker's lock (keep this list in sync).
GAME_FULL_OBJS += build/3ds_sound.o
# FireRed's DirectSound mixer in C with a 16-bit output (src/3ds_m4a_mixer.c).

SOUND_WRAPPED := m4aSoundVSync m4aSoundMain m4aSoundInit m4aSoundVSyncOn m4aSoundVSyncOff \
    m4aSongNumStart m4aSongNumStop m4aMPlayAllStop m4aMPlayStop m4aMPlayContinue \
    m4aMPlayFadeOut m4aMPlayFadeOutTemporarily m4aMPlayFadeIn m4aMPlayImmInit \
    m4aMPlayVolumeControl m4aMPlayPanpotControl \
    SetPokemonCryVolume SetPokemonCryPanpot SetPokemonCryPitch SetPokemonCryLength \
    SetPokemonCryRelease SetPokemonCryProgress SetPokemonCryChorus SetPokemonCryStereo \
    SetPokemonCryPriority SetPokemonCryTone IsPokemonCryPlaying
GAME_FULL_OBJS += build/3ds_asset_memory.o
GAME_LDFLAGS := $(foreach f,$(SOUND_WRAPPED),-Wl,--wrap=$(f))
# The game's own variables in their own section, for the soft reset: gcc finds
# this copy of devkitARM's 3dsx.ld (../tools/game_ram_ld.py) before its own.
GAME_LDFLAGS += -Bbuild/ld/
# Pointer targets of the NOBITS tables: their relocations are gone, so the
# linker needs them as roots (build/rex-roots.rsp) or it would collect the
# handlers they name.
build/firered_game.elf: verify-game-source verify-game-data verify-sound-data verify-sound-songs $(GAME_FULL_OBJS) ../tools/rex_link.py
	$(PYTHON) ../tools/rex_link.py --roots build/rex-roots.rsp
	$(file >build/game-full-link.rsp,$(GAME_C_OBJS) $(GAME_DATA_OBJS) $(GAME_SOUND_OBJS) $(GAME_M4A_OBJS) $(GAME_FULL_OBJS))
	$(CC) $(LDFLAGS) $(VIDEO_LDFLAGS) $(GAME_LDFLAGS) -Wl,-Map,build/firered_game.map @build/game-full-link.rsp @build/rex-roots.rsp -lcitro2d -lcitro3d -lctru -lm -o $@

# A fresh tree must build the real mixer before the full-game link.
build/firered_game.elf: $(GAME_M4A_OBJS)
build/firered_game.elf: build/ld/3dsx.ld
build/ld/3dsx.ld: $(DEVKITARM)/arm-none-eabi/lib/3dsx.ld ../tools/game_ram_ld.py
	$(PYTHON) ../tools/game_ram_ld.py $< $@

build/firered_game_3dsx.elf: build/firered_game.elf ../tools/filter_3dsx_relocs.py
	$(PYTHON) ../tools/filter_3dsx_relocs.py --expect-thunks 1 build/firered_game.elf build/firered_game_3dsx.elf

# The Homebrew Launcher's name and 48x48 icon, the same as the forwarder's
# (forwarder/Makefile). No artwork is stored in the repository:
#   ICON_SRC=icon.jpg   the owner's picture, kept outside the repository, fitted
#                       into 48x48 by forwarder/prepare_art.py (Pillow)
#   (none)              the two flames drawn by forwarder/make_art.py
APP_TITLE := Twin Embers
APP_DESCRIPTION := A FireRed port for the 3DS
APP_AUTHOR := alex61194
ICON_SRC ?=
ICON_ART := build/icon-art
ifneq ($(ICON_SRC),)
$(ICON_ART)/icon.png: forwarder/prepare_art.py forwarder/make_art.py $(ICON_SRC)
	$(PYTHON) forwarder/prepare_art.py --icon "$(ICON_SRC)" $(ICON_ART)
else
$(ICON_ART)/icon.png: forwarder/make_art.py
	$(PYTHON) forwarder/make_art.py $(ICON_ART)
endif

build/firered_game.smdh: $(ICON_ART)/icon.png Makefile
	$(DEVKITPRO)/tools/bin/smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(ICON_ART)/icon.png $@

# Engine metadata (what the executable expects from the pack) is always in the
# RomFS. The game bytes only travel with the executable in a PRIVATE development
# image (EMBED_DATA=1, needs FIRERED_ROM). CLEAN_RELEASE=1 is the public image:
# it forces EMBED_DATA=0, ignores FIRERED_ROM, writes its own RomFS and output
# name, and marks itself clean so the runtime refuses every development backend.
FIRERED_ROM ?=
# A private image embeds its data only when a ROM is given; without one it is an
# engine-only image that needs the pack (or loose data) like the public one.
override EMBED_DATA := 0
CLEAN_RELEASE ?= 1
ifeq ($(CLEAN_RELEASE),1)
override EMBED_DATA := 0
override FIRERED_ROM :=
RUNTIME_ROMFS := build/runtime-romfs-clean
IMAGE_3DSX := twinembers.3dsx
STAGE_PROFILE := clean
else
RUNTIME_ROMFS := build/runtime-romfs
IMAGE_3DSX := twinembers.3dsx
STAGE_PROFILE := development
endif
.PHONY: runtime-data private-pack
romfs/engine/assets.bin: build/firered_game.elf ../tools/asset_bundle.py
	$(PYTHON) ../tools/asset_bundle.py --index build/firered_game.elf
romfs/engine/rex.bin: build/firered_game.elf $(REX_TOOLS) ../tools/rex_link.py
	$(PYTHON) ../tools/rex_link.py --recipe $(REX_RECIPE)
runtime-data: romfs/engine/assets.bin romfs/engine/rex.bin
	$(PYTHON) ../tools/port_common/staging.py --embed $(EMBED_DATA) --dest $(RUNTIME_ROMFS) --profile $(STAGE_PROFILE) $(if $(FIRERED_ROM),--rom $(FIRERED_ROM))
private-pack: romfs/engine/assets.bin romfs/engine/rex.bin
	$(PYTHON) ../tools/port_common/staging.py --embed $(EMBED_DATA) --pack --dest $(RUNTIME_ROMFS) --profile $(STAGE_PROFILE) $(if $(FIRERED_ROM),--rom $(FIRERED_ROM))

$(IMAGE_3DSX): build/firered_game_3dsx.elf build/firered_game.smdh runtime-data
	$(DEVKITPRO)/tools/bin/3dsxtool build/firered_game_3dsx.elf $@ --smdh=build/firered_game.smdh --romfs=$(RUNTIME_ROMFS)
	$(DEVKITARM)/bin/arm-none-eabi-readelf -h build/firered_game.elf | grep -q 'Machine:.*ARM'
	@test -s $@

# Full Azahar-loader simulation gate: the produced 3dsx must parse exactly
# like Azahar's Load3DSXFile (header, segments, every skip/patch entry).
# 3dsxtool exit 0 is NOT enough: it accepts files Azahar rejects.
verify-game-3dsx: $(IMAGE_3DSX)
	$(PYTHON) ../tools/validate_3dsx.py $(IMAGE_3DSX)
	$(PYTHON) ../tools/verify_m4a_arm11.py --object build/game/m4a_1.o --elf build/firered_game.elf --objcopy $(DEVKITARM)/bin/arm-none-eabi-objcopy --readelf $(DEVKITARM)/bin/arm-none-eabi-readelf

game-3dsx: verify-game-3dsx

# PUBLIC release image: no FireRed data inside, hard-gated. The image is only
# declared built when tools/audit_clean_release.py --release accepts it.
.PHONY: clean-game-3dsx verify-clean-release
clean-game-3dsx: verify-clean-release
verify-clean-release:
	@echo "Binary publication remains blocked by distribution policy"
	$(MAKE) CLEAN_RELEASE=1 verify-game-3dsx
	$(PYTHON) ../tools/audit_clean_release.py --tree .. --elf build/firered_game.elf --map build/firered_game.map \
		--3dsx twinembers.3dsx --release --coverage $(if $(FIRERED_ROM_REFERENCE),--rom $(FIRERED_ROM_REFERENCE))
	$(PYTHON) ../tools/source_audit.py --release

# Execute the real optimized intro consumers from cold asset storage.
# Optional execution-test dependencies: unicorn and pyelftools.
.PHONY: verify-intro-assets-arm11
verify-intro-assets-arm11: build/firered_game.elf romfs/engine/assets.bin romfs/engine/rex.bin
	$(PYTHON) ../tools/verify_intro_assets_arm11.py --tree ..

.PHONY: verify-menu-exit-assets-arm11
verify-menu-exit-assets-arm11: build/firered_game.elf romfs/engine/assets.bin romfs/engine/rex.bin private-pack
	$(PYTHON) ../tools/verify_menu_exit_assets_arm11.py --tree ..

# The only readelf -r difference between the two probe ELFs must be the
# removed unaligned THM_CALL entries: 14 blocking ones in the original
# .rel.text (12 aligned THM_CALL stay in both), 0 unaligned in the filtered
# one. This guards CHECKPOINT B and never touches the game image.
verify-game-relocs: build/firered_game_probe_3dsx.elf
	@test "$$( $(DEVKITARM)/bin/arm-none-eabi-readelf -r -W build/firered_game_probe.elf | awk '/\.rel\.text/{f=1;next} /Relocation section/{f=0} f' | grep THM_CALL | awk '{o=strtonum("0x"$$1); if (o % 4) c++} END {print c+0}' )" = "14"
	@test "$$( $(DEVKITARM)/bin/arm-none-eabi-readelf -r -W build/firered_game_probe_3dsx.elf | awk '/\.rel\.text/{f=1;next} /Relocation section/{f=0} f' | grep THM_CALL | awk '{o=strtonum("0x"$$1); if (o % 4) c++} END {print c+0}' )" = "0"

build/game_sources.stamp: ../tools/prepare_game_sources.py
	@mkdir -p build
	$(PYTHON) ../tools/prepare_game_sources.py
	@touch $@

build/sound_assets.stamp: ../tools/prepare_sound_assets.py
	@mkdir -p build
	$(PYTHON) ../tools/prepare_sound_assets.py
	@touch $@


build/gba_shadow_test.exe: tests/gba_shadow_test.c src/gba_shadow.c include/gba_shadow.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/gba_shadow_test.c src/gba_shadow.c -o $@

build/audio_cadence_test.exe: tests/audio_cadence_test.c src/gba_shadow.c include/gba_shadow.h include/3ds_audio_cadence.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/audio_cadence_test.c src/gba_shadow.c -o $@

build/audio_catchup_test.exe: tests/audio_catchup_test.c include/3ds_audio_catchup.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/audio_catchup_test.c -o $@

build/vram_track_test.exe: tests/vram_track_test.c include/3ds_vram_track.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/vram_track_test.c -o $@

build/fast_forward_test.exe: tests/fast_forward_test.c include/3ds_fast_forward.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/fast_forward_test.c -o $@

build/palette_fade_test.exe: tests/palette_fade_test.c include/3ds_palette_fade.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/palette_fade_test.c -o $@

build/day_night_test.exe: tests/day_night_test.c include/3ds_day_night.h include/3ds_day_night_lights.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/day_night_test.c -o $@

build/data_error_text_test.exe: tests/data_error_text_test.c include/3ds_data_error_text.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/data_error_text_test.c -o $@

build/cgb_audio_test.exe: tests/cgb_audio_test.c src/3ds_cgb_audio.c include/3ds_cgb_audio.h include/3ds_cgb_blep.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/cgb_audio_test.c src/3ds_cgb_audio.c -o $@

build/cgb_blep_test.exe: tests/cgb_blep_test.c src/3ds_cgb_audio.c include/3ds_cgb_audio.h include/3ds_cgb_blep.h
	@mkdir -p build
	gcc -std=gnu11 -O2 -Wall -Wextra -Werror -Iinclude tests/cgb_blep_test.c src/3ds_cgb_audio.c -lm -o $@

build/audio_mix_test.exe: tests/audio_mix_test.c src/3ds_audio_mix.c include/3ds_audio_mix.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/audio_mix_test.c src/3ds_audio_mix.c -o $@

build/bios_test.exe: tests/bios_test.c src/3ds_bios.c src/gba_shadow.c include/gba_shadow.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/bios_test.c src/3ds_bios.c src/gba_shadow.c -o $@

build/bios_semantics_test.exe: tests/bios_semantics_test.c src/3ds_bios.c src/gba_shadow.c include/gba_shadow.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/bios_semantics_test.c src/3ds_bios.c src/gba_shadow.c -o $@

build/bios_math_test.exe: tests/bios_math_test.c src/3ds_bios_math.c
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror tests/bios_math_test.c src/3ds_bios_math.c -lm -o $@

build/dma_test.exe: tests/dma_test.c src/3ds_dma.c src/gba_shadow.c include/3ds_dma.h include/gba_shadow.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/dma_test.c src/3ds_dma.c src/gba_shadow.c -o $@

build/flash_test.exe: tests/flash_test.c src/3ds_flash.c include/3ds_flash.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DPLATFORM_3DS -DMODERN=1 -DFIRERED -DENGLISH -DREVISION=0 -iquote include -iquote ../include -iquote .. tests/flash_test.c src/3ds_flash.c -o $@

build/settings_test.exe: tests/settings_test.c src/3ds_settings.c include/3ds_settings.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DCTR_SETTINGS_PATH='"build/settings_test.cfg"' -Iinclude tests/settings_test.c src/3ds_settings.c -o $@

build/obj_window_test.exe: tests/obj_window_test.c include/3ds_obj_window.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/obj_window_test.c -o $@

build/map_edge_test.exe: tests/map_edge_test.c include/3ds_map_edge.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DCTR_MAP_EDGE_FILL=1 -Iinclude tests/map_edge_test.c -o $@

build/forwarder_test.exe: tests/forwarder_test.c include/3ds_forwarder.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/forwarder_test.c -o $@

build/type_chart_test.exe: tests/type_chart_test.c include/3ds_type_chart.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/type_chart_test.c -o $@

build/battle_gen4_test.exe: tests/battle_gen4_test.c src/3ds_battle_gen4.c include/3ds_battle_gen4.h include/3ds_data.h include/3ds_log.h include/3ds_video.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/battle_gen4_test.c src/3ds_battle_gen4.c -o $@

build/leafgreen_test.exe: tests/leafgreen_test.c src/3ds_leafgreen.c include/3ds_leafgreen.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude -iquote ../include tests/leafgreen_test.c src/3ds_leafgreen.c -o $@

build/night_wild_test.exe: tests/night_wild_test.c src/3ds_night_wild.c include/3ds_night_wild.h build/game_sources.stamp
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DCTR_HOST_TEST -Iinclude -iquote ../include tests/night_wild_test.c src/3ds_night_wild.c -o $@

build/poketouch_options_test.exe: tests/poketouch_options_test.c src/3ds_poketouch_options.c src/3ds_settings.c include/3ds_poketouch_options.h include/3ds_poketouch.h include/3ds_settings.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DCTR_SETTINGS_PATH='"build/poketouch_options_test.cfg"' -Iinclude tests/poketouch_options_test.c src/3ds_poketouch_options.c src/3ds_settings.c -o $@

build/rex_test.exe: tests/rex_test.c src/3ds_rex_core.c src/3ds_pak.c include/3ds_rex.h include/3ds_pak.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/rex_test.c src/3ds_rex_core.c src/3ds_pak.c -o $@

build/video_decode_test.exe: tests/video_decode_test.c src/3ds_video_decode.c include/3ds_video.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/video_decode_test.c src/3ds_video_decode.c -o $@

build/gba_alias_test.exe: tests/gba_alias_test.c src/gba_shadow.c include/gba_shadow.h ../include/gba/defines.h ../include/gba/io_reg.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -DPLATFORM_3DS -Iinclude -iquote ../include tests/gba_alias_test.c src/gba_shadow.c -o $@

# Compile one real FireRed game unit with the ARM11 header mapping. -iquote
# keeps the game's include/strings.h from shadowing newlib's <strings.h>.
build/gpu_regs_probe.o: ../src/gpu_regs.c ../include/gba/defines.h ../include/gba/io_reg.h include/gba_shadow.h
	@mkdir -p build
	$(CC) $(ARCH) -std=gnu11 -DPLATFORM_3DS -DMODERN=1 -DFIRERED -DENGLISH -DREVISION=0 -iquote include -iquote ../include -iquote ../include/constants -iquote .. -c $< -o $@

build/main_game_probe.o: ../src/main.c ../include/gba/defines.h ../include/gba/io_reg.h include/gba_shadow.h
	@mkdir -p build
	$(CC) $(ARCH) -std=gnu11 -DPLATFORM_3DS -DMODERN=1 -DFIRERED -DENGLISH -DREVISION=0 -iquote include -iquote ../include -iquote ../include/constants -iquote .. -c $< -o $@

build/game_bridge_probe.o: src/3ds_game_bridge.c include/3ds_fast_forward.h ../include/gba/defines.h include/gba_shadow.h include/3ds_input.h include/3ds_audio.h include/3ds_cgb_audio.h
	@mkdir -p build
	$(CC) $(ARCH) -std=gnu11 -DPLATFORM_3DS -DMODERN=1 -DFIRERED -DENGLISH -DREVISION=0 -I$(CTRULIB)/include -iquote include -iquote ../include -iquote ../include/constants -iquote .. -c $< -o $@

# FireRed translation units need the same charmap preprocessing as the GBA
# build. The pipe runs from the repository root so source-relative includes
# and the charmap resolve identically. pipefail rejects truncated objects.
GAMEFLAGS := $(ARCH) -O2 -std=gnu11 -fsigned-char -fno-strict-aliasing -ffunction-sections -fdata-sections -Wno-error=int-conversion -DPLATFORM_3DS -DMODERN=1 -DFIRERED -DENGLISH -DREVISION=0 -DNDEBUG
GAMEFLAGS += -DCTR_EXTERNAL_ASSETS=1
# ROM-extracted data (REX): every game table listed in the public recipe leaves
# the objects as NOBITS storage and loads from the pack (docs/CLEAN_RELEASE_AUDIT.md).
REX_RECIPE := ../builder/firered3ds_builder/recipes/firered-41cb23d8.json
REX_TOOLS := ../tools/rex.py ../tools/rex_objects.py $(REX_RECIPE) $(wildcard ../builder/firered3ds_builder/recipes/firered-41cb23d8/*.json)
GAMEFLAGS += -iquote $(abspath include) -iquote $(abspath ../include) -iquote $(abspath ../include/constants) -iquote $(abspath ..)

# Authored full-screen pictures retain GBA coordinates. Field/camera/object
# units deliberately do not belong here. All retain the common GAMEFLAGS.
CTR_GBA_STAGE_OBJS := build/game/credits.o
# Oak's Controls Guide and speech are stages, but their BG0 is foreground text.
# Use a separate callback class so only the scenery can fill stage margins.
CTR_GBA_STAGE_UI_OBJS := build/game/oak_speech.o
CTR_GBA_INTRO_OBJS := build/game/intro.o build/game/title_screen.o
CTR_GBA_CENTRED_NAMES := region_map main_menu naming_screen pokemon_summary_screen \
    pokemon_storage_system pokemon_storage_system_data pokemon_storage_system_graphics \
    pokemon_storage_system_tasks pokemon_storage_system_misc item_pc \
    pokedex pokedex_screen pokedex_area_markers pc_screen_effect \
    trainer_card option_menu learn_move help_system mail shop teachy_tv fame_checker \
    berry_pouch pokemon_special_anim hall_of_fame diploma seagallop slot_machine \
    trade trade_scene battle_records \
    party_menu item_menu tm_case evolution_scene
# evolution_scene: a 240x160 battle-style screen (battle BGs, text box, the mon at
# 120,64) that registered no callback class and was drawn as the 400x240 field.
CTR_GBA_CENTRED_OBJS := $(addprefix build/game/,$(addsuffix .o,$(CTR_GBA_CENTRED_NAMES)))
# The native 400x240 TOP Party/Bag/TM attempt (d88c94b) is abandoned: those
# menus use the stable centred 240x160 fallback while the bottom screen owns
# the Party UI. NativeUi remains available for future use but has no objects.
CTR_NATIVE_UI_OBJS :=
# Controllers, animation and interface code share GBA windows, entry positions,
# scanline tables and battle text geometry; transition owns a separate view.
CTR_GBA_BATTLE_OBJS := $(filter build/game/battle_%.o,$(GAME_C_OBJS)) build/game/reshow_battle_screen.o
CTR_GBA_BATTLE_OBJS := $(filter-out build/game/battle_transition.o build/game/battle_records.o,$(CTR_GBA_BATTLE_OBJS))
CTR_GBA_TRANSITION_OBJS := build/game/battle_transition.o
$(CTR_GBA_STAGE_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_stage.h)
$(CTR_GBA_STAGE_UI_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_stage_ui.h)
$(CTR_GBA_INTRO_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_intro.h)
$(CTR_GBA_CENTRED_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_centred.h)
$(CTR_NATIVE_UI_OBJS): GAMEFLAGS += -include $(abspath compat/ctr_native_ui.h)
$(CTR_GBA_BATTLE_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_battle.h)
$(CTR_GBA_TRANSITION_OBJS): GAMEFLAGS += -DCTR_GBA_STAGE -include $(abspath compat/ctr_gba_transition.h)
# These field menus keep GBA UI geometry but restore/retain field VBlank ownership.
# Both preprocessing passes must omit the callback remapping include.
build/game/quest_log.o build/game/pokemon_storage_system_menu.o: GAMEFLAGS += -DCTR_GBA_STAGE


build/3ds_flash.o: src/3ds_flash.c include/3ds_flash.h Makefile
	@mkdir -p build
	$(CC) $(GAMEFLAGS) -c $< -o $@

build/3ds_link_peripherals.o: src/3ds_link_peripherals.c include/3ds_flash.h Makefile
	@mkdir -p build
	$(CC) $(GAMEFLAGS) -c $< -o $@

# No sibling calls: a tail call to the Thumb m4aSoundVSync would need one more
# ARM->Thumb veneer than filter_3dsx_relocs.py --expect-thunks allows.
build/3ds_sound.o: src/3ds_sound.c include/3ds_audio.h Makefile build/game_sources.stamp
	@mkdir -p build
	$(CC) $(GAMEFLAGS) -fno-optimize-sibling-calls -c $< -o $@

$(PREPROC):
	$(MAKE) -C ../tools/preproc

build/game/%.o: ../src/%.c $(PREPROC) Makefile ../tools/asset_bundle.py include/3ds_assets.h $(REX_TOOLS)
	@mkdir -p $(dir $@)
	cd .. && $(CC) $(GAMEFLAGS) -E -x c src/$*.c \
		| tee $(CURDIR)/build/game/$*.incbin.i \
		| tools/preproc/preproc$(HOST_EXE) -i src/$*.c charmap.txt \
		| $(CC) $(GAMEFLAGS) -x c -c -o $(CURDIR)/$@ -
	$(PYTHON) ../tools/asset_bundle.py --object $@ --preprocessed build/game/$*.incbin.i
	$(PYTHON) ../tools/rex_objects.py --manifest $(REX_RECIPE) --object $@

# FireRed's src assembly uses the same preproc + cpp + preproc flow as the
# GBA build (Makefile: $(C_BUILDDIR)/%.o). Only m4a_1.s is linked; crt0,
# rom_header, libagbsyscall and libgcnmultiboot stay out (native 3DS entry
# and 3ds_link_peripherals provide those roles).
build/game/m4a_1.o: ../src/m4a_1.s $(PREPROC) Makefile $(REX_TOOLS)
	@mkdir -p $(dir $@)
	cd .. && $(PREPROC) src/m4a_1.s charmap.txt \
		| $(DEVKITARM)/bin/arm-none-eabi-cpp -Iinclude -I. - \
		| sed 's/\r$$//' \
		| $(PREPROC) -ie src/m4a_1.s charmap.txt \
		| sed -e 's/\.4byte/\.int/g;s/\.2byte/\.short/g' \
		| $(DEVKITARM)/bin/arm-none-eabi-as -mcpu=mpcore --defsym PLATFORM_3DS=1 --defsym MODERN=1 --defsym FIRERED=1 --defsym ENGLISH=1 --defsym REVISION=0 -I. -o $(CURDIR)/$@ -
	$(PYTHON) ../tools/rex_objects.py --manifest $(REX_RECIPE) --object $@

# FireRed's data scripts use preproc + cpp + preproc for charmap and macros.
# Assemble as ARM11 data, translating GBA-specific .4byte/.2byte spellings.
build/game_data/%.o: ../data/%.s $(PREPROC) Makefile $(REX_TOOLS)
	@mkdir -p $(dir $@)
	cd .. && $(PREPROC) data/$*.s charmap.txt \
		| $(DEVKITARM)/bin/arm-none-eabi-cpp -Iinclude -I. - \
		| sed 's/\r$$//' \
		| $(PREPROC) -ie data/$*.s charmap.txt \
		| sed -e 's/\.4byte/\.int/g;s/\.2byte/\.short/g' \
		| $(DEVKITARM)/bin/arm-none-eabi-as -mcpu=mpcore --defsym MODERN=1 --defsym FIRERED=1 --defsym ENGLISH=1 --defsym REVISION=0 -I. -o $(CURDIR)/$@ -
	$(PYTHON) ../tools/rex_objects.py --manifest $(REX_RECIPE) --object $@

build/game_data/sound_data.o: build/sound_assets.stamp

build/game_song/%.o: ../sound/songs/midi/%.mid build/sound_assets.stamp Makefile $(REX_TOOLS)
	@mkdir -p $(dir $@)
	cd .. && $(DEVKITARM)/bin/arm-none-eabi-as -mcpu=mpcore -I sound -o $(CURDIR)/$@ sound/songs/midi/$*.s
	$(PYTHON) ../tools/rex_objects.py --manifest $(REX_RECIPE) --object $@


build/title_assets.stamp: ../tools/prepare_title_assets.py ../src/intro.c ../src/title_screen.c
	@mkdir -p build
	$(PYTHON) ../tools/prepare_title_assets.py
	@touch $@

build/input_test.exe: tests/input_test.c src/3ds_input_decode.c include/3ds_input.h
	@mkdir -p build
	gcc -std=c11 -O2 -Wall -Wextra -Werror -Iinclude tests/input_test.c src/3ds_input_decode.c -o $@

clean:
	rm -rf build $(TARGET).elf $(TARGET).smdh $(TARGET).3dsx

# Every port unit must see profiling structure/enum updates; stale BIOS/DMA
# objects otherwise write counters at the old CtrPerfFrame offsets.
-include $(wildcard build/*.d)

# Generated includes/assets must finish before any unit can assemble/compile.
$(GAME_C_OBJS) $(GAME_DATA_OBJS): build/game_sources.stamp
$(GAME_DATA_OBJS) $(GAME_M4A_OBJS) $(GAME_SOUND_OBJS): ../asm/macros/music_voice.inc

build/3ds_video.o: include/3ds_fast_cells.h include/3ds_vram_track.h src/3ds_intro_fallback.h include/3ds_intro_native.h src/3ds_field_trail_impl.h src/3ds_line_brightness_impl.h src/3ds_obj_window_impl.h
build/3ds_fast_cells.o: include/3ds_fast_cells.h
build/3ds_poketouch.o: include/poketouch_assets.h include/3ds_poketouch.h
build/3ds_bottom.o: include/3ds_storage_touch.h include/3ds_storage_bridge.h
build/game/pokemon_storage_system_tasks.o build/game/pokemon_storage_system_menu.o build/game/pokemon_storage_system_data.o build/game/naming_screen.o: include/3ds_storage_bridge.h

# The game pipeline preprocesses through stdin, so it cannot infer this header dependency.
build/game/sprite.o: include/3ds_effect_oam.h include/3ds_video.h
build/3ds_video.o: include/3ds_effect_oam.h include/3ds_battle_gen4.h

build/game/field_effect.o: include/3ds_fly_phase.h

# Asset-free entropy diagnostic. Never upload generated binaries.
.PHONY: rng-test-3dsx
rng-test-3dsx: build/rng-test.3dsx
build/rng_test_main.o: tests/rng_test_main.c include/3ds_rng.h
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@
build/3ds_rng.o build/3ds_rng_platform.o: include/3ds_rng.h
build/game/main.o: include/3ds_rng.h
build/rng-test.elf: build/rng_test_main.o build/3ds_rng.o build/3ds_rng_platform.o
	$(CC) $(LDFLAGS) $^ -lctru -lm -o $@
build/rng-test.3dsx: build/rng-test.elf
	$(DEVKITPRO)/tools/bin/3dsxtool $< $@
