/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ENTROPY HELIOS contributors
 *
 * ENTROPY HELIOS: Standalone Dual-Core 8-Voice Ambient Drone & Microtonal Synthesizer
 * for the M-VAVE FM-1.
 */
#pragma once
#include <stdint.h>
#include "plat.h"

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);

#define APP_NAME "ENTROPY HELIOS"
#define APP_SITE "ENTROPY HELIOS FM-1"
#define APP_SITE_NOTE "AMBIENT SYNTH"
#define APP_TAGLINE "DUAL-CORE AMBIENT DRONE SYNTHESIZER"
#define APP_AUTHOR "ENTROPY HELIOS contributors"
#define APP_PALETTE 1                    /* AMBER/GOLD default palette */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */
#define APP_OVERLOAD
#ifdef PLAT_TICKS_PER_US
#define APP_LOG
void app_log(void (*line)(const char *t));
#else
#define APP_LOG_LINES
#define APP_LOG_WORD
#endif
int app_spk_muted(void);
#define APP_GAIN(m) (app_spk_muted() ? 0u : ((m) * (m) >> 12))
#define APP_TOUCH                        /* capacitive velocity and continuous pressure from the FM-1's 27 keys */
#define APP_SPLASH                       /* replace early white/orange boot splash with Helios CRT oscilloscope splash */
void app_splash(void);
#define APP_SYSEX                        /* WebMIDI Studio & Patch Librarian SysEx protocol */
#define APP_USB_AUDIO_IN                 /* Live 44.1 kHz stereo USB Audio In -> Entropy Helios FX Engine */
int app_sysex(const uint8_t *p, uint32_t n);
int app_usb_audio_wants_in(void);
void app_usb_audio_in(const int32_t *lr, uint32_t n);

enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_PURE_A, OBJ_PURE_B, OBJ_USER_BANK, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xD0000u, 0xCE000u, 0xD4000u}

#ifdef PLAT_HOST
#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
#define APP_HOST_CMD
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d);
#endif

/* 240 x 48 strip canvas (5 strips cover the full 240 x 240 RGB565 IPS LCD) */
#define APP_CANVAS_PX (240u * 48u)
