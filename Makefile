V=1
SOURCE_DIR=src
BUILD_DIR=build
include $(N64_INST)/include/n64.mk

all: b64.z64
.PHONY: all

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/game.o $(BUILD_DIR)/draw.o $(BUILD_DIR)/maps.o

# ---- Asset pipeline ----
SPRITES = filesystem/sprites/Tree_Wall_Texture.sprite \
          filesystem/sprites/Deer_Enemy_Sprite.sprite \
          filesystem/sprites/Deer_Enemy_Sprite_Death.sprite \
          filesystem/sprites/Rifle_GUI_Sprite.sprite \
          filesystem/sprites/Rifle_GUI_Sprite_Firing.sprite

filesystem/sprites/%.sprite: assets/sprites/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 -o "$(dir $@)" "$<"

# Death sprite source is pre-resized to 256x256 (original is 1024x1024 which
# exceeds the billboard texH cap of 256 rows, hiding the deer body).
# mksprite names the output after the input file, so rename after conversion.
filesystem/sprites/Deer_Enemy_Sprite_Death.sprite: assets/sprites/Deer_Enemy_Sprite_Death_256.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	$(N64_MKSPRITE) --format RGBA16 -o "$(dir $@)" "$<"
	mv $(dir $@)Deer_Enemy_Sprite_Death_256.sprite $@

SOUNDS = filesystem/sounds/button_press.wav64 \
         filesystem/sounds/shoot.wav64 \
         filesystem/sounds/death.wav64 \
         filesystem/sounds/music.wav64

$(SOUNDS): tools/gen_sounds.py assets/sounds/NESquik\ Beat\ 95\ BPM.mp3
	@mkdir -p filesystem/sounds
	@echo "    [WAV64] generating sounds"
	python3 tools/gen_sounds.py

$(BUILD_DIR)/b64.dfs: $(SPRITES) $(SOUNDS)

b64.z64: N64_ROM_TITLE="Blekenstein 64"
b64.z64: $(BUILD_DIR)/b64.dfs

$(BUILD_DIR)/b64.elf: $(OBJS)

clean:
	rm -f $(BUILD_DIR)/* *.z64
	rm -rf filesystem/
.PHONY: clean

-include $(wildcard $(BUILD_DIR)/*.d)
