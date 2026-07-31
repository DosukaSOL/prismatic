/* SPDX-License-Identifier: GPL-3.0-or-later
 * PRISMATIC — C shim over the SameBoy core (compiled as C; includes gb.h). */
#include "sameboy_shim.h"

#include <stdlib.h>
#include <string.h>

#include "gb.h"

#define SB_W 160
#define SB_H 144

struct SBGb {
    GB_gameboy_t* gb;
    uint32_t pixels[SB_W * SB_H];
};

static uint32_t sb_rgb_encode(GB_gameboy_t* gb, uint8_t r, uint8_t g, uint8_t b) {
    (void)gb;
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

SBGb* sb_new(void) {
    SBGb* h = (SBGb*)calloc(1, sizeof(SBGb));
    if (!h) return NULL;
    h->gb = GB_alloc();
    if (!h->gb) {
        free(h);
        return NULL;
    }
    GB_init(h->gb, GB_MODEL_CGB_E);
    GB_set_border_mode(h->gb, GB_BORDER_NEVER);
    GB_set_rgb_encode_callback(h->gb, sb_rgb_encode);
    GB_set_color_correction_mode(h->gb, GB_COLOR_CORRECTION_MODERN_BALANCED);
    GB_set_pixels_output(h->gb, h->pixels);
    return h;
}

void sb_free(SBGb* h) {
    if (!h) return;
    if (h->gb) {
        GB_free(h->gb);
        GB_dealloc(h->gb);
    }
    free(h);
}

int sb_load_boot_rom(SBGb* h, const char* path) {
    return GB_load_boot_rom(h->gb, path);
}

void sb_load_boot_rom_mem(SBGb* h, const uint8_t* data, size_t size) {
    GB_load_boot_rom_from_buffer(h->gb, data, size);
}

int sb_load_rom(SBGb* h, const char* path) {
    return GB_load_rom(h->gb, path);
}

void sb_load_rom_mem(SBGb* h, const uint8_t* data, size_t size) {
    GB_load_rom_from_buffer(h->gb, data, size);
}

void sb_reset(SBGb* h) { GB_reset(h->gb); }

void sb_set_key(SBGb* h, int key, int pressed) {
    if (key < 0 || key > 7) return;
    GB_set_key_state(h->gb, (GB_key_t)key, pressed ? true : false);
}

void sb_run_frame(SBGb* h) { GB_run_frame(h->gb); }

const uint32_t* sb_pixels(SBGb* h) { return h->pixels; }
int sb_width(SBGb* h) { (void)h; return SB_W; }
int sb_height(SBGb* h) { (void)h; return SB_H; }

uint8_t sb_read(SBGb* h, uint16_t addr) { return GB_safe_read_memory(h->gb, addr); }

const uint8_t* sb_vram(SBGb* h, size_t* size) {
    uint16_t bank = 0;
    size_t s = 0;
    void* p = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_VRAM, &s, &bank);
    if (size) *size = s;
    return (const uint8_t*)p;
}

const uint8_t* sb_bg_palettes(SBGb* h, size_t* size) {
    uint16_t bank = 0;
    size_t s = 0;
    void* p = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_BGP, &s, &bank);
    if (size) *size = s;
    return (const uint8_t*)p;
}

const uint8_t* sb_obj_palettes(SBGb* h, size_t* size) {
    uint16_t bank = 0;
    size_t s = 0;
    void* p = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_OBP, &s, &bank);
    if (size) *size = s;
    return (const uint8_t*)p;
}

const uint8_t* sb_oam(SBGb* h, size_t* size) {
    uint16_t bank = 0;
    size_t s = 0;
    void* p = GB_get_direct_access(h->gb, GB_DIRECT_ACCESS_OAM, &s, &bank);
    if (size) *size = s;
    return (const uint8_t*)p;
}

int sb_is_cgb(SBGb* h) { return GB_is_cgb(h->gb) ? 1 : 0; }

void sb_rom_title(SBGb* h, char* out) {
    memset(out, 0, 17);
    GB_get_rom_title(h->gb, out);
}

uint32_t sb_rom_crc32(SBGb* h) { return GB_get_rom_crc32(h->gb); }

int sb_save_battery(SBGb* h, const char* path) { return GB_save_battery(h->gb, path); }
int sb_load_battery(SBGb* h, const char* path) { return GB_load_battery(h->gb, path); }

int sb_save_state(SBGb* h, const char* path) { return GB_save_state(h->gb, path); }
int sb_load_state(SBGb* h, const char* path) { return GB_load_state(h->gb, path); }
