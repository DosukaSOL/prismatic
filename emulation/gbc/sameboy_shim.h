/* SPDX-License-Identifier: GPL-3.0-or-later
 * PRISMATIC — minimal C shim over the SameBoy core.
 *
 * This header is pure C with NO dependency on SameBoy's gb.h. It exists so the
 * C++ adapter (sameboy_backend.cpp) never has to include gb.h — gb.h defines
 * the full internal GB_gameboy_s struct in the header (anonymous unions, section
 * macros) which is C, not guaranteed C++-clean. sameboy_shim.c includes gb.h and
 * is compiled as C; the C++ side talks only to these POD-returning functions.
 */
#ifndef PRISMATIC_SAMEBOY_SHIM_H
#define PRISMATIC_SAMEBOY_SHIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SBGb SBGb; /* opaque handle wrapping a GB_gameboy_t + framebuffer */

/* Key indices, matching SameBoy's GB_key_t order. */
enum {
    SB_KEY_RIGHT = 0,
    SB_KEY_LEFT = 1,
    SB_KEY_UP = 2,
    SB_KEY_DOWN = 3,
    SB_KEY_A = 4,
    SB_KEY_B = 5,
    SB_KEY_SELECT = 6,
    SB_KEY_START = 7
};

/* Create a CGB (Game Boy Color, model CGB-E) instance with the framebuffer,
 * rgb-encode callback and modern-balanced color correction wired up. Border is
 * forced off so the screen is always 160x144. Returns NULL on allocation
 * failure. */
SBGb* sb_new(void);
void sb_free(SBGb* h);

/* Boot ROM (SameBoy's open-source CGB boot ROM). Optional: without it the core
 * still runs, just skipping the boot animation. Returns 0 on success. */
int sb_load_boot_rom(SBGb* h, const char* path);
void sb_load_boot_rom_mem(SBGb* h, const uint8_t* data, size_t size);

/* Game ROM (the user's own dump). Returns 0 on success. */
int sb_load_rom(SBGb* h, const char* path);
void sb_load_rom_mem(SBGb* h, const uint8_t* data, size_t size);

void sb_reset(SBGb* h);
void sb_set_key(SBGb* h, int key, int pressed);
void sb_run_frame(SBGb* h);

/* 160*144 pixels, packed 0xFFRRGGBB (already color-corrected). */
const uint32_t* sb_pixels(SBGb* h);
int sb_width(SBGb* h);
int sb_height(SBGb* h);

/* IO register read (e.g. 0xFF40 LCDC, 0xFF42 SCY, 0xFF43 SCX). */
uint8_t sb_read(SBGb* h, uint16_t addr);

/* Direct hardware state. Pointers stay valid for the lifetime of the handle;
 * contents update every frame. *size (optional) receives the region size. */
const uint8_t* sb_vram(SBGb* h, size_t* size);         /* 0x4000: bank0 [0,0x2000), bank1 [0x2000,0x4000) */
const uint8_t* sb_bg_palettes(SBGb* h, size_t* size);  /* 64: 8 palettes * 4 colors * RGB555 LE */
const uint8_t* sb_obj_palettes(SBGb* h, size_t* size); /* 64 */
const uint8_t* sb_oam(SBGb* h, size_t* size);          /* 0xA0: 40 sprites * (y,x,tile,attr) */

int sb_is_cgb(SBGb* h);
void sb_rom_title(SBGb* h, char* out /* >=17 bytes */);
uint32_t sb_rom_crc32(SBGb* h);

int sb_save_battery(SBGb* h, const char* path);
int sb_load_battery(SBGb* h, const char* path);

/* Full save state (BESS). Returns 0 on success. */
int sb_save_state(SBGb* h, const char* path);
int sb_load_state(SBGb* h, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* PRISMATIC_SAMEBOY_SHIM_H */
