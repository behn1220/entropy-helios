/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ENTROPY HELIOS contributors
 *
 * ENTROPY HELIOS: Dual-Core Component-Modeled HELIOS Music Entropy Helios Polyphonic Ambient Synthesizer
 * for the M-VAVE FM-1 (Core 0 + Core 1 Asymmetric SMP).
 *
 * Strictly single-precision FPU clean (no double, no stdlib/string headers) for JieLi AC79 pi32v2.
 */
#include "dsp.h"
#include "helios.h"

static float hl_fs = 44100.0f;
static float hl_inv_fs = 1.0f / 44100.0f;
static uint32_t (*hl_prof_clk)(void);

static void hl_zero(void *p, uint32_t n)
{
    uint8_t *z = (uint8_t *)p;
    uint32_t i;
    for (i = 0; i < n; i++) z[i] = 0;
}

/* 7 Musical Scales for the 12 Copper Touch Plates (semitone offsets from root across 2 octaves) */
static const uint8_t HL_SCALES[7][12] = {
    {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19}, /* 0 NATURAL MINOR */
    {0, 1, 3, 5, 7, 8, 10, 12, 13, 15, 17, 19}, /* 1 PHRYGIAN (Eastern) */
    {0, 2, 3, 5, 7, 9, 10, 12, 14, 15, 17, 19}, /* 2 DORIAN (Cinematic) */
    {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19}, /* 3 LYDIAN (Cosmic Cinema) */
    {0, 7, 12, 14, 19, 24, 26, 31, 36, 38, 43, 48}, /* 4 STACKED FIFTHS */
    {0, 2, 3, 5, 7, 8, 11, 12, 14, 15, 17, 19}, /* 5 HARMONIC MINOR */
    {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19}  /* 6 MAJOR (Luminous Brian Eno / Aphex Twin) */
};

static const char *const HL_SCALE_NAMES[7] = {
    "MINOR", "PHRYG", "DORIAN", "LYDIAN", "FIFTHS", "HARMIN", "MAJOR"
};

static const char *const HL_WAVE_NAMES[6] = {
    "SAW+SUB", "PWM PLS", "FLD TRI", "FM BELL", "FORMANT", "HARMON"
};

static const char *const HL_CART_NAMES[HL_NCART] = {
    "SHIMMER HALL", "SPACE ECHO", "BLACKHOLE", "GRANULAR", "PHASER+REV", "SUB VOID",
    "REVERSE TAPE", "RINGMOD HALL", "LO-FI CRUSH", "CHORUS+PITCH", "SPECTRAL RES", "INF FREEZE",
    "VIBROTREM", "AUTO-WAH", "BELL PITCH", "TAPE WASH"
};

static const char *const HL_KEYMODE_NAMES[3] = {
    "SPLIT 6+6", "DUAL 12", "POLY 2V"
};

static const char *const HL_PERFMODE_NAMES[3] = {
    "GEN DRONE", "HYBRID", "PLAY 8V"
};

static const char *const HL_ARPMODE_NAMES[6] = {
    "ARP OFF", "ARP UP", "ARP DN", "UP/DOWN", "RANDOM", "STRUM"
};

static const char *const HL_MOD_SRC_NAMES[HL_NMOD_SRC] = {
    "LFO 1", "LFO 2", "S & H", "SLEW", "ENV FOL", "JOY X", "JOY Y", "PLATE P"
};

static const char *const HL_MOD_DST_NAMES[HL_NMOD_DST] = {
    "DRN L", "DRN R", "V4 V/O", "V5 V/O", "VCF L", "VCF R", "VCA L/R", "CART FX"
};

static const char *const HL_EVOLVE_NAMES[4] = {
    "MANUAL", "DRIFT", "CINEMA", "WILD"
};

typedef struct {
    const char *name;
    uint8_t vibe; /* 1=CALM, 2=LUSH, 3=EDGE */
    uint8_t root, scale, bpm, key_mode;
    float d_note[HL_NDRONE];
    float d_spread[HL_NDRONE];
    uint8_t d_sub[HL_NDRONE];
    int8_t d_pan[HL_NDRONE];
    float d_lvl[HL_NDRONE];
    uint8_t m_wave[2];
    int8_t m_oct[2];
    float m_glide[2], m_lvl[2];
    uint8_t m_seq[2], m_div[2];
    uint8_t m_steps[2][5];
    float vcf_cut[2], vcf_res[2];
    uint8_t vcf_mode[2];
    float vcf_drive, vcf_lfo;
    float noise_lvl;
    uint8_t noise_mode;
    float chorus_mix;
    uint8_t cart_type;
    float cart_mix, cart_decay, cart_shimmer, cart_tone;
    float ext_in_lvl, v45_xmod, sh_rate, slew_amt;
    uint8_t pin_preset;
    float micro_detune;
} hl_patch_t;

/* 36 Curated Entropy Helios Factory Patches — Grouped by Mood:
 *   CALM (0..19: P01..P08 Upgraded Calm + P09..P16 Aphex/Eno/BoC Classics + P17..P20 Ultra-Warm Sub-Heavy Bliss)
 *   LUSH (20..27: P21..P28 Cinematic)
 *   EDGE (28..35: P29..P36 Gritty / Experimental) */
static const hl_patch_t HL_PATCHES[HL_NPATCHES] = {
    /* ================= [CALM] (0 .. 7: Upgraded Pure Warmth & Open Harmony) ================= */
    {"WARM ANALOG SEA", 1, 0, 6, 56, 0,
     {36, 43, 48, 52, 55, 64}, {0.14f, 0.16f, 0.18f, 0.15f, 0.18f, 0.20f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.76f, 0.70f, 0.66f, 0.62f, 0.54f},
     {2, 3}, {0, 1}, {0.04f, 0.03f}, {0.54f, 0.48f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {7, 9, 11, 9, 7}},
     {0.28f, 0.32f}, {0.14f, 0.16f}, {0, 0}, 0.07f, 0.20f,
     0.05f, 0, 0.72f, 0, 0.66f, 0.88f, 0.36f, 0.44f,
     0.0f, 0.0f, 0.15f, 0.82f, 0, 0.02f},

    {"VELVET SUNSET", 1, 5, 6, 60, 0,
     {41, 48, 53, 57, 60, 64}, {0.13f, 0.15f, 0.17f, 0.16f, 0.18f, 0.19f}, {1, 0, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.74f, 0.70f, 0.64f, 0.60f, 0.52f},
     {2, 5}, {0, 1}, {0.04f, 0.04f}, {0.54f, 0.48f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {9, 7, 4, 7, 9}},
     {0.30f, 0.34f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.20f,
     0.05f, 0, 0.74f, 1, 0.62f, 0.84f, 0.34f, 0.42f,
     0.0f, 0.0f, 0.14f, 0.84f, 0, 0.02f},

    {"TAPE ARCHIVE", 1, 2, 2, 54, 1,
     {38, 45, 50, 53, 57, 62}, {0.16f, 0.18f, 0.18f, 0.16f, 0.20f, 0.20f}, {1, 1, 0, 0, 0, 0}, {-1, 0, -1, 1, 0, 1},
     {0.84f, 0.78f, 0.70f, 0.64f, 0.58f, 0.52f},
     {2, 3}, {0, 0}, {0.05f, 0.04f}, {0.52f, 0.46f}, {1, 1}, {1, 1}, {{0, 2, 4, 7, 4}, {7, 9, 7, 4, 2}},
     {0.26f, 0.30f}, {0.16f, 0.16f}, {0, 0}, 0.08f, 0.20f,
     0.09f, 0, 0.68f, 1, 0.64f, 0.86f, 0.28f, 0.38f,
     0.0f, 0.0f, 0.14f, 0.84f, 0, 0.03f},

    {"TIBETAN PLATEAU", 1, 7, 6, 46, 0,
     {31, 38, 43, 47, 50, 59}, {0.12f, 0.14f, 0.16f, 0.14f, 0.18f, 0.18f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.86f, 0.80f, 0.74f, 0.68f, 0.62f, 0.54f},
     {5, 3}, {0, 1}, {0.05f, 0.04f}, {0.50f, 0.44f}, {1, 1}, {0, 1}, {{0, 2, 4, 7, 4}, {7, 9, 11, 7, 4}},
     {0.26f, 0.30f}, {0.16f, 0.18f}, {0, 0}, 0.06f, 0.18f,
     0.05f, 0, 0.64f, 2, 0.72f, 0.92f, 0.40f, 0.40f,
     0.0f, 0.0f, 0.12f, 0.88f, 0, 0.02f},

    {"ETERNAL DRONE", 1, 9, 0, 44, 1,
     {33, 40, 45, 48, 52, 60}, {0.14f, 0.16f, 0.18f, 0.16f, 0.18f, 0.20f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.88f, 0.82f, 0.76f, 0.68f, 0.62f, 0.56f},
     {5, 2}, {0, 0}, {0.06f, 0.06f}, {0.42f, 0.38f}, {0, 0}, {0, 1}, {{0, 2, 4, 7, 4}, {7, 5, 4, 2, 0}},
     {0.24f, 0.28f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.18f,
     0.04f, 0, 0.72f, 2, 0.72f, 0.94f, 0.36f, 0.38f,
     0.0f, 0.0f, 0.12f, 0.88f, 0, 0.02f},

    {"SUBHARMONIC VOID", 1, 3, 6, 46, 0,
     {27, 34, 39, 43, 46, 55}, {0.13f, 0.15f, 0.17f, 0.16f, 0.19f, 0.20f}, {1, 1, 1, 1, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.92f, 0.84f, 0.76f, 0.66f, 0.60f, 0.52f},
     {2, 3}, {0, 1}, {0.04f, 0.04f}, {0.48f, 0.42f}, {1, 1}, {0, 1}, {{0, 4, 2, 7, 4}, {7, 9, 7, 4, 2}},
     {0.22f, 0.26f}, {0.16f, 0.16f}, {0, 0}, 0.06f, 0.18f,
     0.05f, 0, 0.68f, 2, 0.72f, 0.92f, 0.42f, 0.36f,
     0.0f, 0.0f, 0.14f, 0.86f, 0, 0.02f},

    {"JUST INTONATION", 1, 0, 6, 52, 2,
     {36, 43, 48, 52, 55, 64}, {0.10f, 0.12f, 0.14f, 0.12f, 0.15f, 0.16f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.85f, 0.78f, 0.72f, 0.66f, 0.60f, 0.54f},
     {5, 3}, {0, 1}, {0.03f, 0.03f}, {0.54f, 0.48f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {7, 9, 11, 9, 7}},
     {0.30f, 0.34f}, {0.12f, 0.14f}, {0, 0}, 0.05f, 0.20f,
     0.04f, 0, 0.68f, 0, 0.68f, 0.90f, 0.40f, 0.44f,
     0.0f, 0.0f, 0.14f, 0.84f, 0, 0.02f},

    {"ENTROPY SYMBIOSIS", 1, 0, 2, 56, 0,
     {36, 43, 48, 51, 55, 62}, {0.14f, 0.16f, 0.18f, 0.16f, 0.19f, 0.20f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.76f, 0.70f, 0.64f, 0.60f, 0.56f, 0.50f},
     {2, 3}, {0, 1}, {0.04f, 0.04f}, {0.50f, 0.46f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {7, 9, 7, 4, 2}},
     {0.28f, 0.32f}, {0.16f, 0.18f}, {0, 0}, 0.08f, 0.22f,
     0.04f, 0, 0.70f, 0, 0.68f, 0.88f, 0.38f, 0.44f,
     0.55f, 0.0f, 0.16f, 0.80f, 0, 0.02f},

    /* ================= [CALM] (8 .. 15: Aphex Twin / Brian Eno / Boards of Canada Classics) ================= */
    {"RHUBARB TAPE", 1, 9, 6, 62, 0,
     {33, 40, 45, 49, 52, 61}, {0.14f, 0.16f, 0.18f, 0.15f, 0.18f, 0.20f}, {1, 0, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.74f, 0.70f, 0.66f, 0.62f, 0.54f},
     {3, 5}, {0, 1}, {0.03f, 0.04f}, {0.56f, 0.50f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {7, 9, 11, 9, 7}},
     {0.34f, 0.38f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.20f,
     0.04f, 0, 0.72f, 0, 0.68f, 0.90f, 0.44f, 0.46f,
     0.0f, 0.0f, 0.14f, 0.85f, 0, 0.02f},

    {"STONE IN FOCUS", 1, 5, 6, 54, 0,
     {29, 36, 41, 45, 48, 57}, {0.16f, 0.18f, 0.20f, 0.16f, 0.20f, 0.22f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.86f, 0.78f, 0.72f, 0.68f, 0.60f, 0.52f},
     {2, 3}, {0, 1}, {0.05f, 0.04f}, {0.48f, 0.44f}, {1, 1}, {0, 1}, {{0, 4, 2, 7, 4}, {7, 11, 9, 7, 9}},
     {0.28f, 0.32f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.16f,
     0.05f, 0, 0.68f, 2, 0.72f, 0.92f, 0.34f, 0.40f,
     0.0f, 0.0f, 0.12f, 0.88f, 0, 0.02f},

    {"AIRPORT TAPE 1/1", 1, 5, 6, 68, 2,
     {41, 48, 53, 57, 60, 64}, {0.10f, 0.12f, 0.14f, 0.12f, 0.15f, 0.16f}, {1, 0, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.66f, 0.60f, 0.56f, 0.54f, 0.50f, 0.46f},
     {3, 5}, {0, 1}, {0.02f, 0.02f}, {0.64f, 0.58f}, {1, 1}, {2, 1}, {{0, 2, 4, 7, 9}, {7, 11, 9, 7, 4}},
     {0.38f, 0.40f}, {0.10f, 0.12f}, {0, 0}, 0.05f, 0.18f,
     0.03f, 0, 0.64f, 0, 0.72f, 0.90f, 0.50f, 0.52f,
     0.0f, 0.0f, 0.14f, 0.84f, 0, 0.02f},

    {"APOLLO AN ENDING", 1, 2, 6, 58, 0,
     {38, 45, 50, 54, 57, 66}, {0.16f, 0.18f, 0.20f, 0.18f, 0.20f, 0.22f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.78f, 0.74f, 0.70f, 0.66f, 0.60f},
     {5, 3}, {0, 1}, {0.04f, 0.03f}, {0.56f, 0.52f}, {1, 1}, {1, 2}, {{0, 4, 7, 9, 7}, {7, 9, 11, 9, 7}},
     {0.34f, 0.38f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.20f,
     0.04f, 0, 0.76f, 0, 0.76f, 0.94f, 0.56f, 0.48f,
     0.0f, 0.0f, 0.14f, 0.86f, 0, 0.02f},

    {"OLSON SUNSET", 1, 7, 6, 72, 0,
     {31, 38, 43, 47, 50, 59}, {0.18f, 0.20f, 0.22f, 0.18f, 0.22f, 0.24f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.76f, 0.72f, 0.68f, 0.62f, 0.56f},
     {2, 3}, {0, 1}, {0.03f, 0.03f}, {0.58f, 0.52f}, {1, 1}, {2, 2}, {{0, 2, 4, 6, 7}, {7, 9, 7, 4, 2}},
     {0.32f, 0.36f}, {0.16f, 0.18f}, {0, 0}, 0.08f, 0.22f,
     0.08f, 0, 0.78f, 9, 0.64f, 0.84f, 0.36f, 0.40f,
     0.0f, 0.0f, 0.18f, 0.78f, 0, 0.03f},

    {"CAMPFIRE TAPE", 1, 2, 2, 74, 0,
     {38, 45, 50, 53, 57, 62}, {0.16f, 0.18f, 0.20f, 0.18f, 0.20f, 0.22f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.80f, 0.74f, 0.70f, 0.66f, 0.60f, 0.54f},
     {3, 2}, {0, 1}, {0.03f, 0.04f}, {0.60f, 0.54f}, {1, 1}, {2, 3}, {{0, 2, 4, 7, 4}, {7, 9, 11, 7, 4}},
     {0.34f, 0.38f}, {0.16f, 0.18f}, {0, 0}, 0.08f, 0.20f,
     0.07f, 0, 0.70f, 1, 0.66f, 0.86f, 0.34f, 0.44f,
     0.0f, 0.0f, 0.18f, 0.80f, 0, 0.03f},

    {"LICHEN CATHEDRAL", 1, 0, 6, 48, 0,
     {36, 43, 48, 52, 55, 64}, {0.12f, 0.14f, 0.16f, 0.14f, 0.16f, 0.18f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.78f, 0.74f, 0.70f, 0.64f, 0.58f},
     {5, 2}, {0, 0}, {0.08f, 0.08f}, {0.36f, 0.32f}, {0, 0}, {0, 0}, {{0, 2, 4, 7, 4}, {7, 9, 11, 9, 7}},
     {0.30f, 0.34f}, {0.12f, 0.14f}, {0, 0}, 0.05f, 0.16f,
     0.03f, 0, 0.66f, 2, 0.76f, 0.94f, 0.38f, 0.40f,
     0.0f, 0.0f, 0.10f, 0.90f, 0, 0.02f},

    {"DISCREET LOOP", 1, 7, 6, 66, 2,
     {31, 38, 43, 47, 50, 59}, {0.13f, 0.15f, 0.17f, 0.14f, 0.18f, 0.20f}, {1, 0, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.74f, 0.70f, 0.66f, 0.62f, 0.58f, 0.52f},
     {3, 5}, {0, 1}, {0.02f, 0.03f}, {0.62f, 0.56f}, {1, 1}, {2, 5}, {{0, 2, 4, 7, 9}, {11, 9, 7, 4, 2}},
     {0.34f, 0.38f}, {0.14f, 0.16f}, {0, 0}, 0.06f, 0.20f,
     0.04f, 0, 0.72f, 3, 0.70f, 0.90f, 0.46f, 0.46f,
     0.0f, 0.0f, 0.14f, 0.86f, 0, 0.02f},

    /* ================= [CALM] (16 .. 19: ULTRA-WARM SUB-HEAVY PURE ANALOG BLISS — Zero Brassy Highs!) ================= */
    {"VELVET SUB OCEAN", 1, 5, 6, 48, 0,
     {29, 36, 41, 45, 48, 53}, {0.12f, 0.14f, 0.16f, 0.14f, 0.16f, 0.18f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.92f, 0.84f, 0.76f, 0.66f, 0.58f, 0.48f},
     {2, 5}, {-1, 0}, {0.04f, 0.04f}, {0.46f, 0.42f}, {1, 1}, {0, 1}, {{0, 2, 4, 7, 4}, {7, 4, 2, 4, 7}},
     {0.19f, 0.21f}, {0.10f, 0.12f}, {0, 0}, 0.04f, 0.14f,
     0.03f, 0, 0.76f, 2, 0.70f, 0.92f, 0.22f, 0.26f,
     0.0f, 0.0f, 0.10f, 0.90f, 0, 0.02f},

    {"MIDNIGHT TAPE WRM", 1, 2, 6, 54, 0,
     {26, 33, 38, 42, 45, 54}, {0.14f, 0.16f, 0.18f, 0.15f, 0.18f, 0.20f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.90f, 0.82f, 0.74f, 0.66f, 0.58f, 0.48f},
     {2, 3}, {-1, 0}, {0.03f, 0.04f}, {0.50f, 0.44f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 4}, {7, 9, 7, 4, 2}},
     {0.21f, 0.23f}, {0.12f, 0.12f}, {0, 0}, 0.05f, 0.16f,
     0.05f, 0, 0.78f, 1, 0.66f, 0.88f, 0.24f, 0.28f,
     0.0f, 0.0f, 0.12f, 0.88f, 0, 0.02f},

    {"DEEP WOOL DRONE", 1, 0, 6, 42, 0,
     {24, 31, 36, 40, 43, 52}, {0.12f, 0.14f, 0.16f, 0.14f, 0.16f, 0.18f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.94f, 0.86f, 0.78f, 0.68f, 0.58f, 0.46f},
     {2, 5}, {-1, 0}, {0.06f, 0.06f}, {0.34f, 0.30f}, {0, 0}, {0, 0}, {{0, 2, 4, 7, 4}, {7, 9, 7, 4, 2}},
     {0.18f, 0.20f}, {0.08f, 0.10f}, {0, 0}, 0.04f, 0.12f,
     0.02f, 0, 0.74f, 2, 0.74f, 0.94f, 0.18f, 0.22f,
     0.0f, 0.0f, 0.10f, 0.92f, 0, 0.02f},

    {"SILK HORIZON", 1, 7, 6, 60, 0,
     {31, 38, 43, 47, 50, 55}, {0.13f, 0.15f, 0.17f, 0.14f, 0.17f, 0.19f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.88f, 0.80f, 0.74f, 0.66f, 0.58f, 0.50f},
     {2, 3}, {-1, 0}, {0.03f, 0.03f}, {0.54f, 0.48f}, {1, 1}, {1, 2}, {{0, 2, 4, 7, 9}, {7, 4, 2, 4, 7}},
     {0.22f, 0.24f}, {0.10f, 0.12f}, {0, 0}, 0.05f, 0.16f,
     0.03f, 0, 0.76f, 0, 0.68f, 0.90f, 0.28f, 0.30f,
     0.0f, 0.0f, 0.12f, 0.88f, 0, 0.02f},

    /* ================= [LUSH] (8 .. 15) ================= */
    {"NEON RAIN 82", 2, 2, 2, 58, 0,
     {38, 45, 50, 57, 60, 65}, {0.26f, 0.30f, 0.32f, 0.28f, 0.34f, 0.36f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.80f, 0.76f, 0.72f, 0.68f, 0.64f},
     {0, 1}, {0, 1}, {0.42f, 0.48f}, {0.64f, 0.58f}, {1, 1}, {1, 2}, {{0, 3, 4, 5, 4}, {7, 9, 10, 7, 6}},
     {0.48f, 0.54f}, {0.36f, 0.38f}, {0, 3}, 0.42f, 0.45f,
     0.12f, 0, 0.74f, 0, 0.68f, 0.88f, 0.62f, 0.64f,
     0.0f, 0.12f, 0.32f, 0.55f, 1, 0.0f},

    {"DESERT WIND", 2, 4, 1, 54, 0,
     {28, 35, 40, 47, 52, 56}, {0.30f, 0.34f, 0.38f, 0.32f, 0.36f, 0.40f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.88f, 0.84f, 0.78f, 0.74f, 0.70f, 0.66f},
     {4, 0}, {-1, 0}, {0.50f, 0.55f}, {0.66f, 0.60f}, {1, 1}, {1, 4}, {{0, 1, 4, 5, 1}, {7, 8, 7, 6, 8}},
     {0.44f, 0.50f}, {0.52f, 0.56f}, {3, 1}, 0.56f, 0.48f,
     0.24f, 0, 0.62f, 2, 0.70f, 0.90f, 0.48f, 0.52f,
     0.0f, 0.18f, 0.28f, 0.50f, 1, 0.16f},

    {"HELIOS CORONA", 2, 9, 3, 68, 2,
     {33, 40, 45, 52, 59, 64}, {0.28f, 0.32f, 0.35f, 0.30f, 0.36f, 0.38f}, {1, 0, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.78f, 0.74f, 0.72f, 0.68f, 0.64f},
     {3, 5}, {0, 1}, {0.25f, 0.30f}, {0.62f, 0.58f}, {1, 1}, {2, 3}, {{0, 2, 4, 5, 4}, {11, 9, 7, 6, 8}},
     {0.56f, 0.62f}, {0.32f, 0.35f}, {0, 0}, 0.36f, 0.42f,
     0.10f, 0, 0.76f, 0, 0.72f, 0.90f, 0.74f, 0.70f,
     0.0f, 0.10f, 0.42f, 0.45f, 1, 0.0f},

    {"CATHEDRAL VCF", 2, 0, 5, 50, 0,
     {36, 43, 48, 55, 56, 63}, {0.24f, 0.28f, 0.32f, 0.30f, 0.34f, 0.36f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.80f, 0.76f, 0.72f, 0.68f, 0.64f},
     {4, 1}, {0, 0}, {0.48f, 0.52f}, {0.60f, 0.56f}, {1, 1}, {1, 2}, {{0, 2, 3, 5, 4}, {7, 6, 8, 9, 7}},
     {0.42f, 0.48f}, {0.58f, 0.62f}, {3, 3}, 0.52f, 0.50f,
     0.14f, 0, 0.68f, 4, 0.70f, 0.88f, 0.56f, 0.58f,
     0.0f, 0.15f, 0.30f, 0.60f, 1, 0.0f},

    {"BERLIN SEQUENCER", 2, 6, 0, 92, 0,
     {30, 37, 42, 49, 54, 61}, {0.22f, 0.26f, 0.28f, 0.24f, 0.30f, 0.32f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.80f, 0.76f, 0.72f, 0.68f, 0.64f, 0.60f},
     {0, 1}, {0, 1}, {0.12f, 0.15f}, {0.68f, 0.64f}, {1, 1}, {2, 3}, {{0, 3, 4, 5, 4}, {7, 10, 9, 7, 6}},
     {0.52f, 0.58f}, {0.42f, 0.45f}, {0, 0}, 0.44f, 0.38f,
     0.10f, 0, 0.66f, 1, 0.64f, 0.76f, 0.45f, 0.60f,
     0.0f, 0.14f, 0.58f, 0.22f, 2, 0.0f},

    {"GLASS NEBULA", 2, 10, 3, 56, 2,
     {34, 41, 46, 53, 58, 65}, {0.25f, 0.28f, 0.32f, 0.35f, 0.38f, 0.42f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.76f, 0.72f, 0.68f, 0.64f, 0.60f},
     {3, 5}, {0, 1}, {0.35f, 0.40f}, {0.60f, 0.56f}, {1, 1}, {2, 5}, {{0, 4, 5, 2, 4}, {11, 7, 9, 6, 8}},
     {0.50f, 0.56f}, {0.30f, 0.34f}, {0, 3}, 0.32f, 0.44f,
     0.12f, 0, 0.72f, 3, 0.74f, 0.88f, 0.72f, 0.68f,
     0.0f, 0.22f, 0.36f, 0.48f, 1, 0.0f},

    {"MATRIX VOYAGER", 2, 7, 2, 66, 0,
     {31, 38, 43, 50, 55, 60}, {0.24f, 0.28f, 0.30f, 0.26f, 0.32f, 0.35f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.82f, 0.78f, 0.74f, 0.70f, 0.66f, 0.62f},
     {1, 2}, {0, 1}, {0.28f, 0.32f}, {0.64f, 0.60f}, {1, 1}, {2, 4}, {{0, 2, 4, 5, 3}, {7, 9, 11, 8, 6}},
     {0.46f, 0.52f}, {0.48f, 0.50f}, {3, 3}, 0.46f, 0.42f,
     0.14f, 0, 0.70f, 4, 0.68f, 0.86f, 0.58f, 0.62f,
     0.0f, 0.20f, 0.52f, 0.35f, 2, 0.0f},

    {"BIOME EXT FILTER", 2, 2, 2, 60, 0,
     {38, 45, 50, 57, 62, 69}, {0.22f, 0.26f, 0.30f, 0.24f, 0.28f, 0.32f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.76f, 0.72f, 0.68f, 0.64f, 0.60f, 0.56f},
     {2, 3}, {0, 1}, {0.40f, 0.45f}, {0.58f, 0.54f}, {1, 1}, {1, 2}, {{0, 2, 3, 5, 4}, {7, 8, 10, 9, 7}},
     {0.44f, 0.50f}, {0.46f, 0.48f}, {3, 0}, 0.45f, 0.40f,
     0.10f, 0, 0.68f, 1, 0.66f, 0.84f, 0.52f, 0.58f,
     0.80f, 0.12f, 0.35f, 0.50f, 3, 0.0f},

    /* ================= [EDGE] (16 .. 23) ================= */
    {"LYRA MONOLITH", 3, 1, 1, 60, 0,
     {25, 32, 37, 44, 49, 56}, {0.58f, 0.64f, 0.68f, 0.62f, 0.70f, 0.74f}, {1, 1, 1, 1, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.88f, 0.84f, 0.80f, 0.76f, 0.72f, 0.68f},
     {2, 4}, {-1, 0}, {0.65f, 0.70f}, {0.68f, 0.64f}, {1, 1}, {1, 5}, {{0, 1, 4, 5, 1}, {8, 7, 10, 6, 7}},
     {0.46f, 0.52f}, {0.72f, 0.76f}, {3, 1}, 0.78f, 0.62f,
     0.32f, 1, 0.58f, 2, 0.72f, 0.92f, 0.58f, 0.54f,
     0.0f, 0.55f, 0.48f, 0.30f, 4, 0.38f},

    {"CHERNOBYL REACTOR", 3, 5, 1, 64, 0,
     {29, 36, 41, 42, 48, 55}, {0.66f, 0.72f, 0.76f, 0.70f, 0.78f, 0.82f}, {1, 1, 1, 1, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.86f, 0.82f, 0.78f, 0.74f, 0.70f, 0.66f},
     {3, 2}, {-1, 0}, {0.50f, 0.45f}, {0.66f, 0.62f}, {1, 1}, {5, 3}, {{0, 1, 3, 5, 1}, {7, 8, 11, 9, 6}},
     {0.40f, 0.48f}, {0.78f, 0.82f}, {1, 3}, 0.84f, 0.68f,
     0.52f, 1, 0.54f, 2, 0.76f, 0.94f, 0.64f, 0.50f,
     0.0f, 0.62f, 0.65f, 0.15f, 2, 0.42f},

    {"COSMIC HORIZON", 3, 7, 5, 74, 2,
     {31, 38, 43, 50, 55, 62}, {0.52f, 0.58f, 0.62f, 0.56f, 0.64f, 0.68f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.86f, 0.82f, 0.78f, 0.74f, 0.70f, 0.66f},
     {1, 3}, {0, 1}, {0.22f, 0.28f}, {0.68f, 0.64f}, {1, 1}, {3, 5}, {{0, 2, 3, 5, 4}, {11, 8, 7, 9, 6}},
     {0.54f, 0.60f}, {0.68f, 0.72f}, {3, 3}, 0.72f, 0.58f,
     0.28f, 2, 0.68f, 3, 0.74f, 0.92f, 0.76f, 0.62f,
     0.0f, 0.48f, 0.55f, 0.25f, 4, 0.25f},

    {"CYBERPUNK RAIN", 3, 11, 2, 78, 0,
     {35, 42, 47, 54, 59, 66}, {0.46f, 0.52f, 0.56f, 0.50f, 0.58f, 0.62f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.80f, 0.76f, 0.72f, 0.68f, 0.64f},
     {0, 3}, {0, 1}, {0.30f, 0.25f}, {0.70f, 0.66f}, {1, 1}, {2, 4}, {{0, 3, 5, 4, 2}, {10, 7, 8, 9, 6}},
     {0.50f, 0.58f}, {0.64f, 0.68f}, {0, 1}, 0.68f, 0.54f,
     0.34f, 0, 0.72f, 0, 0.70f, 0.88f, 0.68f, 0.64f,
     0.0f, 0.40f, 0.50f, 0.35f, 2, 0.0f},

    {"K140UD12 SCREAM", 3, 2, 1, 84, 0,
     {26, 33, 38, 45, 50, 57}, {0.50f, 0.56f, 0.60f, 0.54f, 0.62f, 0.66f}, {1, 1, 1, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.86f, 0.82f, 0.78f, 0.74f, 0.70f, 0.66f},
     {2, 3}, {0, 1}, {0.15f, 0.18f}, {0.72f, 0.68f}, {1, 1}, {3, 5}, {{0, 1, 4, 5, 3}, {7, 8, 10, 11, 6}},
     {0.42f, 0.50f}, {0.86f, 0.88f}, {1, 3}, 0.88f, 0.64f,
     0.30f, 1, 0.56f, 4, 0.68f, 0.88f, 0.62f, 0.58f,
     0.0f, 0.78f, 0.72f, 0.18f, 4, 0.30f},

    {"S&H ROBOTICS", 3, 9, 5, 108, 0,
     {33, 40, 45, 52, 57, 64}, {0.42f, 0.48f, 0.52f, 0.46f, 0.54f, 0.58f}, {1, 1, 0, 0, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.80f, 0.76f, 0.72f, 0.68f, 0.64f, 0.60f},
     {1, 3}, {0, 1}, {0.05f, 0.08f}, {0.72f, 0.68f}, {1, 1}, {3, 5}, {{0, 3, 4, 5, 2}, {7, 10, 11, 8, 6}},
     {0.48f, 0.54f}, {0.74f, 0.76f}, {3, 2}, 0.72f, 0.58f,
     0.26f, 1, 0.60f, 1, 0.66f, 0.82f, 0.54f, 0.66f,
     0.0f, 0.65f, 0.85f, 0.06f, 2, 0.0f},

    {"CORONA FLARE XMOD", 3, 4, 1, 72, 1,
     {28, 35, 40, 47, 52, 59}, {0.62f, 0.68f, 0.72f, 0.66f, 0.74f, 0.78f}, {1, 1, 1, 1, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.88f, 0.84f, 0.80f, 0.76f, 0.72f, 0.68f},
     {3, 4}, {-1, 1}, {0.35f, 0.40f}, {0.70f, 0.66f}, {1, 1}, {2, 5}, {{0, 1, 3, 5, 4}, {7, 8, 10, 11, 9}},
     {0.46f, 0.54f}, {0.76f, 0.80f}, {3, 1}, 0.82f, 0.66f,
     0.38f, 2, 0.62f, 5, 0.76f, 0.94f, 0.72f, 0.52f,
     0.0f, 0.88f, 0.60f, 0.25f, 4, 0.48f},

    {"ENTROPY OVERDRIVE", 3, 0, 1, 80, 0,
     {24, 31, 36, 43, 48, 55}, {0.54f, 0.60f, 0.64f, 0.58f, 0.66f, 0.70f}, {1, 1, 1, 1, 0, 0}, {-1, -1, 0, 0, 1, 1},
     {0.84f, 0.80f, 0.76f, 0.72f, 0.68f, 0.64f},
     {2, 3}, {-1, 0}, {0.25f, 0.30f}, {0.68f, 0.64f}, {1, 1}, {2, 3}, {{0, 1, 3, 4, 1}, {6, 7, 9, 10, 7}},
     {0.42f, 0.48f}, {0.80f, 0.84f}, {3, 1}, 0.86f, 0.60f,
     0.25f, 1, 0.58f, 5, 0.74f, 0.92f, 0.68f, 0.54f,
     0.95f, 0.50f, 0.55f, 0.28f, 3, 0.22f}
};

/* Large DSP Delay/Reverb/Dual-Core Buffers in .pool SRAM (336 KiB region on M-VAVE FM-1) */
static BR_POOL float bbd_l[2048], bbd_r[2048];
static BR_POOL float fdn_buf[8][2048];
static BR_POOL float sh_buf[2048];
static BR_POOL float dly_l[16384], dly_r[16384];
static BR_POOL float spr_buf[3][1024];   /* Physical-modeled 3-spring analog reverb tank (12 KiB in .pool) */
static BR_POOL float c0_flt_l[HL_MAXN], c2_flt_r[HL_MAXN];
static BR_POOL float c2_in_l[HL_MAXN],  c2_in_r[HL_MAXN];
static BR_POOL float c2_wet_l[HL_MAXN], c2_wet_r[HL_MAXN];

/* Lock-free Dual-Core Synchronization State (Core 0 <-> Core 1) */
static volatile uint32_t hl_cs_go;      /* 0 = idle, 2 = Right Voices + Right Analog VCF, 1 = BBD Chorus + Cartridge FDN FX */
static int hl_cs_mode;                  /* 0 = single-core, 1 = hardware Core 1, 2 = host dual-core simulator */
static uint32_t hl_cs_have;
static volatile uint32_t hl_v_jobs, hl_fx_jobs, hl_cs_late, hl_cs_lost;
static volatile uint32_t hl_c1_busy, hl_c0_samples;
static int (*hl_c2_run)(void (*)(uint32_t), uint32_t);
static int (*hl_c2_wait)(void);

/* Shared Per-Block Parameters for Core 0 & Core 1 Parallel Render */
static struct {
    uint32_t n;
    float d_inc[HL_NDRONE][HL_NSAW];
    float d_gain_l[HL_NDRONE], d_gain_r[HL_NDRONE];
    float m_inc[HL_NMEL], m_g[HL_NMEL];
    float g_cut[2], res_eff[2];
    float sh_eff, fb_rev, k_rtone;
    uint32_t dly_len_l, dly_len_r;
} B;

/* Internal DSP Runtime State */
static helios_state_t S;
static struct {
    /* 6 Drone voices x 5 saws */
    float d_ph[HL_NDRONE][HL_NSAW];
    float d_drift[HL_NDRONE][HL_NSAW];
    float d_lp[HL_NDRONE];

    /* 2 Melodic voices */
    float m_ph1[HL_NMEL], m_ph2[HL_NMEL], m_sub[HL_NMEL], m_vib_ph[HL_NMEL];
    float m_last_sig[HL_NMEL];
    float m_seq_ph[HL_NMEL];
    int8_t m_pend_dir[HL_NMEL];
    uint8_t duo_turn;

    /* Dual Russian Analog VCF states (0 = Left Core 0, 1 = Right Core 1) */
    float pv_bp[2], pv_lp[2], pv_slew[2];
    float vcf_lfo_ph;

    /* Modular CV Generators: LFO 2, Sample & Hold, Analog Slew Limiter */
    float lfo2_ph;
    float sh_ph, sh_val, slew_val;
    uint8_t prev_comp;

    /* Entropy Symbiosis Preamp Sub-Generator + Noise / Geiger (0 = Left Core 0, 1 = Right Core 1) */
    uint32_t rng[2];
    float pink[2][2], geiger[2], sub_ph[2];
    float sym_ph[2][3], sym_lfo[2];

    /* BBD Tri-Phase Stereo Ensemble Chorus & Cartridge FX State (Run on Core 1 in Stage B) */
    uint32_t bbd_w;
    float bbd_ph;
    uint32_t fdn_w[8];
    float fdn_lp[8], fdn_dc_x[8], fdn_dc_y[8];
    uint32_t sh_w;
    float sh_ph_fx[2], sub_oct_ph;
    uint32_t dly_w;
    float dly_lp[2];
    float phaser_ap[2][4];

    /* Physical-Modeled 3-Spring Analog Reverb Tank + Piezo Contact-Mic Kick (Core 1) */
    uint32_t spr_w[3];
    float spr_ap[3][4];
    float spr_lp[3];
    float spr_kick_imp;

    /* New Cartridges (Reverse Tape, RingMod, Lo-Fi Crush, Spectral Resonator) on Core 1 */
    float rm_ph, rev_ph;
    float lofi_cnt, lofi_hl, lofi_hr;
    float spec_y1[4], spec_y2[4];

    /* 8-Second 2D Vector Joystick Motion Recorder (64 keyframes @ 8 Hz) */
    float joy_rec_x[64], joy_rec_y[64];
    float joy_rec_ph;
    uint8_t joy_rec_len;

    /* Pultec Low-End Foundation & Lookahead Limiter (Run on Core 0) */
    float xf_lo_l, xf_lo_r, dc_xl, dc_yl, dc_xr, dc_yr;
    float lim_env;
    uint32_t scope_w;

    /* Generative Helios Evolve: Entropy Harmonic Composer + Rarity-Weighted Timbral Evolution */
    float    ev_timer, ev_joy_ph, ev_chord_t, ev_freeze_t, ev_mel_rest_t;
    float    d_cur_note[HL_NDRONE]; /* Smooth voice-led drone pitch glide */
    uint8_t  ev_home_root, ev_home_scale, ev_prog_step, ev_journey_rem, ev_last_deg;
    uint8_t  ev_tgt_ready;
    float    ev_tgt_cut[2], ev_tgt_res[2], ev_tgt_drive;
    float    ev_tgt_spr, ev_tgt_cmix, ev_tgt_shim, ev_tgt_chor;
    float    ev_tgt_morph[HL_NDRONE], ev_tgt_spread[HL_NDRONE];

    /* Per-Voice Polyphonic Instrument & Velocity / Polyphonic-Aftertouch State (HYBRID & PLAY 8V) */
    int8_t   d_key[HL_NDRONE];
    uint8_t  d_gate[HL_NDRONE];
    float    d_vel[HL_NDRONE];
    float    d_at[HL_NDRONE];
    float    d_pluck[HL_NDRONE];
    uint32_t d_age[HL_NDRONE];

    int8_t   m_key[HL_NMEL];
    uint8_t  m_gate[HL_NMEL];
    float    m_vel[HL_NMEL];
    float    m_at[HL_NMEL];
    float    m_pluck[HL_NMEL];
    uint32_t m_age[HL_NMEL];

    uint32_t voice_clock;
    float    arp_ph;
    int8_t   arp_idx, arp_dir;
    uint8_t  arp_gate_on;
    float    xmod_pluck;

    /* Studio v2.0: Live USB Audio In, MIDI Clock, Channel AT, Sustain Pedal, Master Bus & 16-Band Spectrum */
    float    usb_in[2][HL_MAXN];
    uint8_t  usb_in_ready;
    uint32_t clk_last_ms;
    float    clk_dt_avg;
    uint32_t clk_age_blocks;
    uint8_t  midi_sustain;
    float    chan_at;
    float    hpf_xl, hpf_yl, hpf_xr, hpf_yr;
    float    tilt_l, tilt_r;
    float    spec_bp[16], spec_lp[16];
    uint8_t  spec_en;
    /* v3.2: VIBROTREM, AUTO-WAH, TAPE WASH cartridge state (28 B) */
    float    vt_ph, wah_env, wah_bp[2], wah_lp[2], wash_lp[2];
} D;

/* Studio v2.0 Scene A/B Morphing Snapshot */
typedef struct {
    float vcf_cut[2], vcf_res[2], vcf_drive;
    float joy_x, joy_y;
    float spring_mix, spring_decay;
    float cart_mix, cart_decay, cart_shimmer;
    float chorus_mix, ext_in_lvl, v45_xmod;
    float d_spread[HL_NDRONE], d_morph[HL_NDRONE], d_level[HL_NDRONE];
} hl_scene_t;

static hl_scene_t hl_scene_a, hl_scene_b, hl_compare_bak;
static void hl_snap_scene(hl_scene_t *sc);

static const char *const HL_TUNING_NAMES[5] = {"12-TET", "JUST INT", "PYTHAG", "BOHLEN-P", "MAQAM"};
static const char *const HL_CHORD_NAMES[6]  = {"OFF", "5TH PWR", "MIN 7", "MAJ 9", "SUS 2", "CLUSTER"};
static const char *const HL_EXTSRC_NAMES[3] = {"SYMBIOSIS", "USB AUDIO", "SYM + USB"};

/* Apply one of 5 curated 8x8 Modular Pin Matrix templates */
static void hl_apply_pin_preset(uint8_t preset)
{
    hl_zero(S.pin_matrix, sizeof S.pin_matrix);
    S.pin_preset = preset % 5u;
    switch (S.pin_preset) {
    case 0: /* 0: CLASSIC NORMALED JOYSTICK + LFO + AFTERTOUCH */
        S.pin_matrix[HL_SRC_LFO1][HL_DST_VCFL]  = 1;
        S.pin_matrix[HL_SRC_LFO1][HL_DST_VCFR]  = 1;
        S.pin_matrix[HL_SRC_JOYX][HL_DST_VCFL]  = -1;
        S.pin_matrix[HL_SRC_JOYX][HL_DST_VCFR]  = 2;
        S.pin_matrix[HL_SRC_JOYY][HL_DST_DRNL]  = 1;
        S.pin_matrix[HL_SRC_JOYY][HL_DST_DRNR]  = 1;
        S.pin_matrix[HL_SRC_PRESS][HL_DST_VCFL] = 1;
        S.pin_matrix[HL_SRC_PRESS][HL_DST_VCFR] = 1;
        break;
    case 1: /* 1: ORGANIC DRIFT MATRIX (Slow Slewed S&H + LFO1/2 Stereo Morphing) */
        S.pin_matrix[HL_SRC_LFO1][HL_DST_VCFL]  = 1;
        S.pin_matrix[HL_SRC_LFO2][HL_DST_VCFR]  = 1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_DRNL]  = 1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_DRNR]  = -1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_CART]  = 2;
        S.pin_matrix[HL_SRC_JOYX][HL_DST_VCFL]  = -1;
        S.pin_matrix[HL_SRC_JOYX][HL_DST_VCFR]  = 2;
        S.pin_matrix[HL_SRC_PRESS][HL_DST_V4VO] = 1;
        break;
    case 2: /* 2: S&H DUAL VCF STEPPER (Sample & Hold -> Dual Analog VCFs & Melodic V4/V5) */
        S.pin_matrix[HL_SRC_SH][HL_DST_VCFL]    = 2;
        S.pin_matrix[HL_SRC_SH][HL_DST_VCFR]    = -1;
        S.pin_matrix[HL_SRC_SH][HL_DST_V4VO]    = 1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_V5VO]  = 1;
        S.pin_matrix[HL_SRC_LFO2][HL_DST_VCA]   = 1;
        S.pin_matrix[HL_SRC_JOYY][HL_DST_CART]  = 2;
        break;
    case 3: /* 3: ENTROPY ENV-FOLLOWER SYMBIOSIS (Entropy Preamp drives Analog Filters, VCA & FX) */
        S.pin_matrix[HL_SRC_ENVFOL][HL_DST_VCFL] = 2;
        S.pin_matrix[HL_SRC_ENVFOL][HL_DST_VCFR] = 2;
        S.pin_matrix[HL_SRC_ENVFOL][HL_DST_CART] = 1;
        S.pin_matrix[HL_SRC_ENVFOL][HL_DST_DRNL] = 1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_VCFL]   = 1;
        S.pin_matrix[HL_SRC_JOYX][HL_DST_VCFR]   = 2;
        break;
    case 4: /* 4: LYRA X-MOD & CHAOS MATRIX (Cross-LFO + S&H + Pressure into all oscillators & filters) */
        S.pin_matrix[HL_SRC_LFO2][HL_DST_V4VO]  = 2;
        S.pin_matrix[HL_SRC_LFO1][HL_DST_V5VO]  = -1;
        S.pin_matrix[HL_SRC_SH][HL_DST_DRNL]    = 2;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_DRNR]  = 2;
        S.pin_matrix[HL_SRC_SH][HL_DST_VCFL]    = 2;
        S.pin_matrix[HL_SRC_LFO2][HL_DST_VCFR]  = 2;
        S.pin_matrix[HL_SRC_PRESS][HL_DST_CART] = 2;
        break;
    }
}

/* 5-saw spread ratios: from tight unison (spread=0) -> super-cluster (0.5) -> fifths/octaves (1.0) */
static inline float hl_saw_ratio(uint32_t k, float spread, uint8_t sub)
{
    static const float DET[5] = {0.0f, -0.011f, +0.012f, -0.024f, +0.026f};
    static const float CHORD[5] = {1.0f, 0.500f, 1.4983f, 2.000f, 1.250f};
    if (k == 0u) return sub ? 0.5f : 1.0f;
    if (spread <= 0.55f) {
        float u = spread * (1.0f / 0.55f);
        return 1.0f + DET[k] * u * (1.0f + 1.4f * u);
    } else {
        float u = (spread - 0.55f) * (1.0f / 0.45f);
        float r_det = 1.0f + DET[k] * 2.4f;
        return r_det + (CHORD[k] - r_det) * u;
    }
}

/* Band-limited PolyBLEP analog relaxation sawtooth with subtle CMOS 40106 RC charge curvature */
static inline float hl_poly_saw(float ph, float dt)
{
    float s = 2.0f * ph - 1.0f;
    s += 0.14f * (s * s - 0.333f);
    if (ph < dt) {
        float t = ph / dt;
        s -= t + t - t * t - 1.0f;
    } else if (ph > 1.0f - dt) {
        float t = (ph - 1.0f) / dt;
        s -= t * t + t + t + 1.0f;
    }
    return s;
}

/* Russian Analog K140UD12 Programmable-Op-Amp Slew-Limited Filter (bank 0=Left Core 0, bank 1=Right Core 1) */
static inline float hl_analog_vcf(unsigned bank, float x, float g_cut, float res, float drive, uint8_t mode)
{
    float bp = D.pv_bp[bank], lp = D.pv_lp[bank], slew = D.pv_slew[bank];
    float fb = (0.35f + 3.45f * res * res) * (bp + 0.08f * bp * bp);
    float in_s = (x * (1.0f + 1.8f * drive) - fb);
    float hp = br_soft(in_s * 0.85f) - lp;
    float dbp = g_cut * hp;
    float max_slew = 0.28f + 0.72f * (1.0f - 0.55f * drive);
    if (dbp > max_slew) dbp = max_slew;
    else if (dbp < -max_slew) dbp = -max_slew;
    bp += dbp;
    bp = br_soft(bp * 0.92f) * (1.0f / 0.92f);
    lp += g_cut * bp;
    slew += 0.15f * (lp - slew);
    D.pv_bp[bank] = fm_flush(bp);
    D.pv_lp[bank] = fm_flush(lp);
    D.pv_slew[bank] = fm_flush(slew);

    if (mode == 1u) return bp * 1.35f;               /* BANDPASS (Vocal throat growl) */
    if (mode == 2u) return hp * 0.90f;               /* HIGHPASS */
    if (mode == 3u) return 0.65f * lp + 0.55f * bp;  /* LP + BP Formant Morph */
    return lp * 1.12f;                               /* LOWPASS 12dB */
}

/* Synthesize one half-bank (bank=0: Left Bank V1..V3 + Melodic V4 + Left Analog VCF on Core 0;
 *                          bank=1: Right Bank V6..V8 + Melodic V5 + Right Analog VCF on Core 1) */
static void hl_render_bank(unsigned bank, float *out_flt, uint32_t n)
{
    uint32_t i, v, k;
    uint32_t v0 = bank == 0u ? 0u : 3u;
    uint32_t v1 = bank == 0u ? 3u : 6u;
    float g_cut = B.g_cut[bank], res = B.res_eff[bank], drive = S.vcf_drive;
    uint8_t vmode = S.vcf_mode[bank];
    float ext_pk = 0.0f;

    for (i = 0; i < n; i++) {
        float sig_bank = 0.0f;

        /* 1. External Preamp Stage: Entropy Symbiosis Generator, Live 44.1 kHz USB Audio In, or BOTH */
        if (S.ext_in_lvl > 0.005f) {
            float ext_raw = 0.0f;
            if (S.ext_src != 1u) {
                float dt0 = B.d_inc[v0][0] * 0.5f;
                float p0 = D.sym_ph[bank][0] + dt0;
                float p1 = D.sym_ph[bank][1] + dt0 * 1.4983f;
                float p2 = D.sym_ph[bank][2] + dt0 * 2.002f;
                if (p0 >= 1.0f) p0 -= 1.0f;
                if (p1 >= 1.0f) p1 -= 1.0f;
                if (p2 >= 1.0f) p2 -= 1.0f;
                D.sym_ph[bank][0] = p0; D.sym_ph[bank][1] = p1; D.sym_ph[bank][2] = p2;
                D.sym_lfo[bank] += 0.00004f;
                if (D.sym_lfo[bank] >= 1.0f) D.sym_lfo[bank] -= 1.0f;
                float pulse = 0.5f + 0.5f * fm_sin_turns(D.sym_lfo[bank] + (bank ? 0.35f : 0.0f));
                ext_raw += (0.45f * fm_sin_turns(p0 + 0.22f * fm_sin_turns(p1)) +
                            0.35f * hl_poly_saw(p1, dt0) +
                            0.25f * fm_sin_turns(p2)) * pulse;
            }
            if (S.ext_src != 0u && D.usb_in_ready) {
                ext_raw += D.usb_in[bank][i] * 1.45f;
            }
            float pre = br_soft(ext_raw * (0.9f + 2.4f * S.ext_in_lvl)) * S.ext_in_lvl * 0.48f;
            sig_bank += pre;
            if (bank == 0u) {
                float a = fm_fabsf(pre);
                if (a > ext_pk) ext_pk = a;
            }
        }

        /* 2. 3 Drone Voices x 5 Analog Relaxation Oscillators with Wave Morph (SAW -> TRI -> PULSE -> BELL) */
        for (v = v0; v < v1; v++) {
            float g_side = (bank == 0u) ? B.d_gain_l[v] : B.d_gain_r[v];
            float wm = S.drone[v].wave_morph;
            float v_sum = 0.0f, p0;
            if (g_side <= 1e-5f) continue;
            p0 = D.d_ph[v][0];
            for (k = 0; k < HL_NSAW; k++) {
                float ph = D.d_ph[v][k], dt = B.d_inc[v][k];
                float osc;
                if (wm <= 0.02f) {
                    osc = hl_poly_saw(ph, dt);
                } else if (wm <= 0.34f) {
                    float t = wm * 3.0f;
                    float saw = hl_poly_saw(ph, dt);
                    float tri = ph < 0.5f ? (4.0f * ph - 1.0f) : (3.0f - 4.0f * ph);
                    osc = saw + (tri - saw) * t;
                } else if (wm <= 0.67f) {
                    float t = (wm - 0.34f) * 3.0f;
                    float tri = ph < 0.5f ? (4.0f * ph - 1.0f) : (3.0f - 4.0f * ph);
                    float p2 = ph + 0.32f; if (p2 >= 1.0f) p2 -= 1.0f;
                    float pls = 0.7f * (hl_poly_saw(ph, dt) - hl_poly_saw(p2, dt));
                    osc = tri + (pls - tri) * t;
                } else {
                    float t = (wm - 0.67f) * 3.0f;
                    float p2 = ph + 0.32f; if (p2 >= 1.0f) p2 -= 1.0f;
                    float pls = 0.7f * (hl_poly_saw(ph, dt) - hl_poly_saw(p2, dt));
                    float bell = fm_sin_turns(ph + 0.25f * fm_sin_turns(ph * 2.0f));
                    osc = pls + (bell - pls) * t;
                }
                v_sum += osc;
                ph += dt + (k > 0u ? 0.00012f * fm_sin_turns(p0 - ph) : 0.0f);
                if (ph >= 1.0f) ph -= 1.0f;
                else if (ph < 0.0f) ph += 1.0f;
                D.d_ph[v][k] = ph;
            }
            D.d_lp[v] += 0.28f * (v_sum * 0.22f - D.d_lp[v]);
            sig_bank += D.d_lp[v] * g_side;
        }

        /* 3. Melodic Multi-Mode Voice (V4 on Core 0, V5 on Core 1) with V4<->V5 Cross-FM & Hard-Sync Bite */
        {
            float ph1 = D.m_ph1[bank], ph2 = D.m_ph2[bank], sub = D.m_sub[bank], dt = B.m_inc[bank], sig = 0.0f;
            float other_sig = D.m_last_sig[bank ^ 1u];
            float xmod_eff = br_clampf(S.v45_xmod + D.xmod_pluck * 0.45f, 0.0f, 1.0f);
            float xfm = xmod_eff * other_sig * 0.28f;
            float ph_eff = ph1 + xfm;
            while (ph_eff >= 1.0f) ph_eff -= 1.0f;
            while (ph_eff < 0.0f)  ph_eff += 1.0f;

            uint8_t wv = S.mel[bank].wave;
            if (wv == 0u) {        /* SAW + SUB */
                sig = 0.65f * hl_poly_saw(ph_eff, dt) + 0.40f * fm_sin_turns(sub);
            } else if (wv == 1u) { /* PWM PULSE */
                float pw = 0.5f + 0.32f * fm_sin_turns(D.vcf_lfo_ph + (float)bank * 0.3f) + 0.18f * xfm;
                float p2 = ph_eff + pw; if (p2 >= 1.0f) p2 -= 1.0f;
                sig = 0.55f * (hl_poly_saw(ph_eff, dt) - hl_poly_saw(p2, dt));
            } else if (wv == 2u) { /* FOLDED TRIANGLE */
                float tri = ph_eff < 0.5f ? 4.0f * ph_eff - 1.0f : 3.0f - 4.0f * ph_eff;
                sig = fm_sin_turns(tri * (0.45f + 0.35f * drive + 0.45f * S.v45_xmod));
            } else if (wv == 3u) { /* 2-OP FM BELL */
                float mod = fm_sin_turns(ph2);
                sig = fm_sin_turns(ph_eff + (0.35f + 0.45f * drive) * mod);
            } else if (wv == 4u) { /* VOWEL FORMANT */
                sig = 0.55f * fm_sin_turns(ph_eff) + 0.35f * fm_sin_turns(ph_eff * 3.0f) + 0.25f * fm_sin_turns(ph_eff * 5.0f);
            } else {               /* HARMONIC OCTAVES + FIFTH */
                sig = 0.50f * fm_sin_turns(ph_eff) + 0.30f * fm_sin_turns(ph2) + 0.30f * fm_sin_turns(sub);
            }
            ph1 += dt;
            if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += dt * 1.4983f; if (ph2 >= 1.0f) ph2 -= 1.0f;
            sub += dt * 0.5f; if (sub >= 1.0f) sub -= 1.0f;
            D.m_ph1[bank] = ph1; D.m_ph2[bank] = ph2; D.m_sub[bank] = sub;
            D.m_last_sig[bank] = sig;
            sig_bank += sig * B.m_g[bank];
        }

        /* 4. Per-Bank Analog Pink / Geiger / Sub-Rumble Noise */
        if (S.noise_lvl > 0.002f) {
            float w = br_randf(&D.rng[bank]);
            float ns, ng = S.noise_lvl * 0.22f;
            D.pink[bank][0] = 0.995f * D.pink[bank][0] + w * 0.035f;
            D.pink[bank][1] = 0.960f * D.pink[bank][1] + w * 0.095f;
            ns = D.pink[bank][0] + D.pink[bank][1];
            if (S.noise_mode == 1u) {
                D.geiger[bank] *= 0.92f;
                if (w > 0.996f) D.geiger[bank] = 1.2f;
                ns = ns * 0.4f + D.geiger[bank];
            } else if (S.noise_mode == 2u) {
                D.sub_ph[bank] += 42.0f * hl_inv_fs;
                if (D.sub_ph[bank] >= 1.0f) D.sub_ph[bank] -= 1.0f;
                ns = D.pink[bank][0] * 1.6f + fm_sin_turns(D.sub_ph[bank]) * 0.55f;
            }
            sig_bank += ns * ng;
        }

        /* 5. Russian Analog K140UD12 Slew-Limited VCF */
        out_flt[i] = hl_analog_vcf(bank, sig_bank, g_cut, res, drive, vmode);
    }

    if (bank == 0u) {
        if (S.ext_in_lvl > 0.005f) {
            ext_pk = br_clampf(ext_pk * 2.8f, 0.0f, 1.0f);
            if (ext_pk > S.ext_env) S.ext_env += 0.45f * (ext_pk - S.ext_env);
            else                    S.ext_env += 0.12f * (ext_pk - S.ext_env);
            S.ext_comp = (S.ext_env > 0.32f) ? 1u : (S.ext_env < 0.22f ? 0u : S.ext_comp);
        } else {
            S.ext_env *= 0.85f;
            S.ext_comp = 0u;
        }
    }
}

/* Stage B: Stereo BBD Tri-Phase Ensemble Chorus + Physical 3-Spring Analog Reverb Tank +
 * 12-Slot Magnetic Stereo Cartridge Multi-FX + Infinite Freeze
 * (Executed on Core 1 in parallel with Core 0's Mastering & 240x240 UI!) */
static void hl_render_fx_core(const float *in_l, const float *in_r, float *out_l, float *out_r, uint32_t n)
{
    static const uint32_t FDN_LEN[8] = {683u, 809u, 967u, 1151u, 1327u, 1523u, 1733u, 1951u};
    static const uint32_t SPR_LEN[3] = {541u, 719u, 907u}; /* 3 staggered physical spring lengths */
    static const float SPEC_RATIO[4] = {1.0f, 1.4983f, 2.0f, 2.5198f};
    uint32_t i, k, s;
    float sh_eff = B.sh_eff, fb_rev = B.fb_rev, k_rtone = B.k_rtone;
    uint32_t dly_len_l = B.dly_len_l, dly_len_r = B.dly_len_r;
    float ch_mix = S.chorus_mix, c_mix = S.cart_mix, c_dec = S.cart_decay;
    float sp_mix = S.spring_mix, sp_fb = 0.56f + 0.36f * S.spring_decay;
    uint16_t cm = S.cart_mask;
    uint8_t frozen = (S.cart_freeze != 0u || (cm & (1u << 11))) ? 1u : 0u;
    float rm_inc = (28.0f + 420.0f * S.cart_param[7] * S.cart_param[7]) * hl_inv_fs;
    float spec_base_hz = 110.0f * fm_exp2f((float)(S.root % 12u) * (1.0f / 12.0f));
    int has_shimmer = ((cm & ((1u << 0) | (1u << 2) | (1u << 5) | (1u << 6) | (1u << 9) | (1u << 14))) != 0u);
    int has_sub_oct = ((cm & ((1u << 2) | (1u << 5) | (1u << 9))) != 0u);
    int has_rev     = ((cm & (1u << 6)) != 0u);
    int has_echo    = ((cm & ((1u << 1) | (1u << 3) | (1u << 6) | (1u << 14) | (1u << 15))) != 0u);
    int has_chorus  = (ch_mix > 0.005f || (cm & ((1u << 9) | (1u << 12))) != 0u);

    if (frozen) fb_rev = 0.996f;

    /* Hoist block-constant FX coefficients (using independent per-cartridge cart_param[i]!) */
    uint32_t bbd_off1 = 220u, bbd_off2 = 240u, bbd_off3 = 260u;
    float phaser_ap_k = 0.25f + 0.55f * fm_sin_turns(D.bbd_ph * (0.35f + 0.85f * S.cart_param[4]));
    float lofi_sr_div = 2.0f + 14.0f * (1.0f - S.cart_param[8]);
    float lofi_q      = 14.0f + 24.0f * S.cart_param[8];
    float lofi_inv_q  = 1.0f / lofi_q;
    float vt_gl = 1.0f, vt_gr = 1.0f;
    float spec_c1[4], spec_r2 = 0.992f + 0.006f * c_dec;
    spec_r2 = spec_r2 * spec_r2;
    if (cm & (1u << 10)) {
        float rad = 0.992f + 0.006f * c_dec;
        for (k = 0; k < 4u; k++) {
            float f_r = spec_base_hz * SPEC_RATIO[k] * (1.0f + 0.5f * S.cart_param[10]);
            spec_c1[k] = 2.0f * rad * fm_sin_turns(0.25f - f_r * hl_inv_fs);
        }
    }
    if (cm & (1u << 12)) {
        /* 12: VIBROTREM — Optical Stereo Tremolo + Jet Flanger LFO (independent cart_param[12] rate!) */
        D.vt_ph += (1.2f + 6.2f * S.cart_param[12]) * (float)n * hl_inv_fs;
        while (D.vt_ph >= 1.0f) D.vt_ph -= 1.0f;
        vt_gl = 0.52f + 0.48f * fm_sin_turns(D.vt_ph);
        vt_gr = 0.52f + 0.48f * fm_sin_turns(D.vt_ph + 0.25f);
    }
    if (has_chorus) {
        D.bbd_ph += ((cm & (1u << 12)) ? 1.45f : 0.62f) * (float)n * hl_inv_fs;
        while (D.bbd_ph >= 1.0f) D.bbd_ph -= 1.0f;
        if (cm & (1u << 12)) {
            /* Tight resonant jet-flanger taps when VIBROTREM is active */
            bbd_off1 = (uint32_t)(48.0f + 36.0f * fm_sin_turns(D.bbd_ph));
            bbd_off2 = (uint32_t)(64.0f + 42.0f * fm_sin_turns(D.bbd_ph + 0.333f));
            bbd_off3 = (uint32_t)(82.0f + 48.0f * fm_sin_turns(D.bbd_ph + 0.667f));
        } else {
            bbd_off1 = (uint32_t)(220.0f + 110.0f * fm_sin_turns(D.bbd_ph));
            bbd_off2 = (uint32_t)(240.0f + 110.0f * fm_sin_turns(D.bbd_ph + 0.333f));
            bbd_off3 = (uint32_t)(260.0f + 110.0f * fm_sin_turns(D.bbd_ph + 0.667f));
        }
    }

    for (i = 0; i < n; i++) {
        float flt_l = in_l[i], flt_r = in_r[i];

        /* 1. Stereo BBD Tri-Phase Analog Ensemble Chorus / VIBROTREM Jet Flanger */
        if (has_chorus) {
            float eff_ch = ((cm & ((1u << 9) | (1u << 12))) && ch_mix < 0.72f) ? 0.72f : ch_mix;
            uint32_t w = D.bbd_w;
            bbd_l[w] = flt_l;
            bbd_r[w] = flt_r;
            D.bbd_w = (w + 1u) & 2047u;
            uint32_t i1 = (w + 2048u - bbd_off1) & 2047u;
            uint32_t i2 = (w + 2048u - bbd_off2) & 2047u;
            uint32_t i3 = (w + 2048u - bbd_off3) & 2047u;
            float cl = 0.5f * (bbd_l[i1] + bbd_r[i2]);
            float cr = 0.5f * (bbd_r[i3] + bbd_l[i2]);
            flt_l = flt_l * (1.0f - 0.45f * eff_ch) + cl * 0.65f * eff_ch;
            flt_r = flt_r * (1.0f - 0.45f * eff_ch) + cr * 0.65f * eff_ch;
        }

        /* 2. Physical-Modeled 3-Spring Analog Reverb Tank + Piezo Contact-Mic Kick */
        if (sp_mix > 0.005f || D.spr_kick_imp > 0.001f) {
            float mono_in = (flt_l + flt_r) * 0.42f;
            float kick_n = 0.0f;
            float spr_l = 0.0f, spr_r = 0.0f;
            if (D.spr_kick_imp > 0.001f) {
                kick_n = br_randf(&D.rng[1]) * D.spr_kick_imp * 1.45f;
                D.spr_kick_imp *= 0.9965f;
            }
            for (s = 0; s < 3u; s++) {
                float x = spr_buf[s][D.spr_w[s]];
                /* 4-stage cascaded dispersive allpass produces authentic metallic spring chirp/boing */
                float ap_g = 0.58f + 0.06f * (float)s;
                for (k = 0; k < 4u; k++) {
                    float y = -ap_g * x + D.spr_ap[s][k];
                    D.spr_ap[s][k] = x + ap_g * y;
                    x = y;
                }
                D.spr_lp[s] += (0.24f + 0.14f * (float)s) * (x - D.spr_lp[s]);
                spr_buf[s][D.spr_w[s]] = br_soft((mono_in + kick_n) * 0.55f + D.spr_lp[s] * sp_fb);
                if (++D.spr_w[s] >= SPR_LEN[s]) D.spr_w[s] = 0u;
                if (s == 0u)      spr_l += D.spr_lp[s];
                else if (s == 1u) { spr_l += D.spr_lp[s] * 0.55f; spr_r += D.spr_lp[s] * 0.55f; }
                else              spr_r += D.spr_lp[s];
            }
            {
                float sm = sp_mix > 0.005f ? sp_mix : 0.45f;
                flt_l = flt_l + spr_l * sm * 0.58f;
                flt_r = flt_r + spr_r * sm * 0.58f;
            }
        }

        /* 3. 16-Slot Stackable Magnetic Stereo Cartridge Multi-FX (any combination of bits 0..15!) */
        if (cm == 0u && !frozen) {
            out_l[i] = flt_l;
            out_r[i] = flt_r;
        } else {
            float rd[8], sum = 0.0f, wet_l = 0.0f, wet_r = 0.0f;
            float sh_in = (flt_l + flt_r) * 0.5f, sh_sig = 0.0f;

            sh_buf[D.sh_w] = sh_in;
            if (has_shimmer) {
                for (k = 0; k < 2u; k++) {
                    float ph = D.sh_ph_fx[k];
                    float d_age = has_rev ? (ph * 1800.0f) : ((1.0f - ph) * 1024.0f);
                    uint32_t r_idx = (D.sh_w + 2048u - (uint32_t)d_age) & 2047u;
                    float win = fm_sin_turns(ph * 0.5f);
                    sh_sig += sh_buf[r_idx] * win * win;
                    /* Cart 9: +7st fifth; Cart 14 (BELL PITCH): +12st crisp bell; Cart 6: reverse grain */
                    ph += (cm & (1u << 9)) ? 0.000365f : has_rev ? 0.00072f : (cm & (1u << 14)) ? 0.00062f : 0.000488f;
                    if (ph >= 1.0f) ph -= 1.0f;
                    D.sh_ph_fx[k] = ph;
                }
            }
            if (has_sub_oct) {
                D.sub_oct_ph += 0.000244f;
                if (D.sub_oct_ph >= 1.0f) D.sub_oct_ph -= 1.0f;
                uint32_t r_sub = (D.sh_w + 2048u - (uint32_t)(D.sub_oct_ph * 1024.0f)) & 2047u;
                float w_sub = fm_sin_turns(D.sub_oct_ph * 0.5f);
                sh_sig = 0.45f * sh_sig + 0.85f * sh_buf[r_sub] * w_sub * w_sub;
            }
            D.sh_w = (D.sh_w + 1u) & 2047u;

            /* Stereo Space Tape Echo / Reverse Tape / Magic Bell Pitch Delay / Tape Wash Tape Loop */
            float echo_l, echo_r;
            if (has_rev) {
                D.rev_ph += (0.45f + 1.15f * (1.0f - c_dec)) * hl_inv_fs;
                if (D.rev_ph >= 1.0f) D.rev_ph -= 1.0f;
                uint32_t rev_off_l = (uint32_t)(D.rev_ph * (float)dly_len_l) & 16383u;
                uint32_t rev_off_r = (uint32_t)(D.rev_ph * (float)dly_len_r) & 16383u;
                float rwin = fm_sin_turns(D.rev_ph * 0.5f);
                echo_l = dly_l[(D.dly_w + 16384u - rev_off_l) & 16383u] * rwin;
                echo_r = dly_r[(D.dly_w + 16384u - rev_off_r) & 16383u] * rwin;
            } else {
                echo_l = dly_l[(D.dly_w + 16384u - dly_len_l) & 16383u];
                echo_r = dly_r[(D.dly_w + 16384u - dly_len_r) & 16383u];
            }
            D.dly_lp[0] += k_rtone * (echo_r - D.dly_lp[0]);
            D.dly_lp[1] += k_rtone * (echo_l - D.dly_lp[1]);
            {
                float bell_fb = (cm & (1u << 14)) ? (sh_sig * 0.55f) : 0.0f;
                float echo_fb = (cm & (1u << 15)) ? (0.58f + 0.34f * c_dec) : (0.45f + 0.38f * c_dec);
                dly_l[D.dly_w] = br_soft((frozen ? 0.0f : (flt_l + bell_fb)) + D.dly_lp[0] * echo_fb);
                dly_r[D.dly_w] = br_soft((frozen ? 0.0f : (flt_r + bell_fb)) + D.dly_lp[1] * echo_fb);
            }
            D.dly_w = (D.dly_w + 1u) & 16383u;

            /* 8-Line Householder FDN Cathedral Hall (with Freeze support!) */
            for (k = 0; k < 8u; k++) {
                rd[k] = fdn_buf[k][D.fdn_w[k]];
                sum += rd[k];
                if (k & 1u) wet_r += rd[k];
                else        wet_l += rd[k];
            }
            sum *= 0.25f;
            {
                float in_gate = frozen ? 0.015f : 1.0f;
                float fdn_in_l = (flt_l + sh_sig * sh_eff * 0.52f + (has_echo ? echo_l * 0.55f : 0.0f)) * in_gate;
                float fdn_in_r = (flt_r + sh_sig * sh_eff * 0.52f + (has_echo ? echo_r * 0.55f : 0.0f)) * in_gate;
                for (k = 0; k < 8u; k++) {
                    float fb = (sum - rd[k]) * fb_rev + ((k & 1u) ? fdn_in_r : fdn_in_l) * 0.35f;
                    D.fdn_lp[k] += (frozen ? 0.85f : k_rtone) * (fb - D.fdn_lp[k]);
                    float dcy = D.fdn_lp[k] - D.fdn_dc_x[k] + 0.995f * D.fdn_dc_y[k];
                    D.fdn_dc_x[k] = D.fdn_lp[k];
                    D.fdn_dc_y[k] = dcy;
                    fdn_buf[k][D.fdn_w[k]] = br_soft(dcy);
                    if (++D.fdn_w[k] >= FDN_LEN[k]) D.fdn_w[k] = 0u;
                }
            }
            wet_l = wet_l * 0.38f + (has_echo ? echo_l * 0.65f : echo_l * 0.22f);
            wet_r = wet_r * 0.38f + (has_echo ? echo_r * 0.65f : echo_r * 0.22f);

            /* STACKABLE CARTRIDGE INSERT CHAIN (every active bit uses its own independent cart_lvl[i] & cart_param[i]!) */
            if (cm & (1u << 4)) {
                /* 4: PHASER+REV — 4-Stage Stereo Krautrock Phaser */
                float lv = S.cart_lvl[4];
                float pl = wet_l, pr = wet_r;
                for (k = 0; k < 4u; k++) {
                    float yl = -phaser_ap_k * pl + D.phaser_ap[0][k];
                    D.phaser_ap[0][k] = pl + phaser_ap_k * yl; pl = yl;
                    float yr = -phaser_ap_k * pr + D.phaser_ap[1][k];
                    D.phaser_ap[1][k] = pr + phaser_ap_k * yr; pr = yr;
                }
                wet_l = wet_l * (1.0f - 0.45f * lv) + pl * (0.45f * lv);
                wet_r = wet_r * (1.0f - 0.45f * lv) + pr * (0.45f * lv);
            }
            if (cm & (1u << 7)) {
                /* 7: RINGMOD HALL — Bode-style stereo carrier ring modulation */
                float lv = S.cart_lvl[7];
                D.rm_ph += rm_inc;
                if (D.rm_ph >= 1.0f) D.rm_ph -= 1.0f;
                float cl = fm_sin_turns(D.rm_ph);
                float cr = fm_sin_turns(D.rm_ph + 0.25f);
                wet_l = wet_l * (1.0f - 0.50f * lv) + (flt_l * cl) * (0.78f * lv);
                wet_r = wet_r * (1.0f - 0.50f * lv) + (flt_r * cr) * (0.78f * lv);
            }
            if (cm & (1u << 8)) {
                /* 8: LO-FI CRUSH — Zero-order sample-rate reduction + warm tape bit-crush */
                float lv = S.cart_lvl[8];
                D.lofi_cnt += 1.0f;
                if (D.lofi_cnt >= lofi_sr_div) {
                    D.lofi_cnt -= lofi_sr_div;
                    D.lofi_hl = (float)((int)(wet_l * lofi_q)) * lofi_inv_q;
                    D.lofi_hr = (float)((int)(wet_r * lofi_q)) * lofi_inv_q;
                }
                wet_l = wet_l * (1.0f - 0.65f * lv) + D.lofi_hl * (0.65f * lv);
                wet_r = wet_r * (1.0f - 0.65f * lv) + D.lofi_hr * (0.65f * lv);
            }
            if (cm & (1u << 10)) {
                /* 10: SPECTRAL RES — 4-band tuned resonant chord body bank */
                float lv = S.cart_lvl[10];
                float r_in = (flt_l + flt_r) * 0.25f;
                float sl = 0.0f, sr = 0.0f;
                for (k = 0; k < 4u; k++) {
                    float y = r_in * 0.08f + spec_c1[k] * D.spec_y1[k] - spec_r2 * D.spec_y2[k];
                    D.spec_y2[k] = D.spec_y1[k];
                    D.spec_y1[k] = br_soft(y);
                    if (k & 1u) sr += D.spec_y1[k];
                    else        sl += D.spec_y1[k];
                }
                wet_l = wet_l * (1.0f - 0.50f * lv) + sl * (0.65f * lv);
                wet_r = wet_r * (1.0f - 0.50f * lv) + sr * (0.65f * lv);
            }
            if (cm & (1u << 12)) {
                /* 12: VIBROTREM — Optical Stereo Tremolo + Jet Flanger */
                float lv = S.cart_lvl[12];
                wet_l = wet_l * (1.0f - 0.60f * lv) + (flt_l * vt_gl) * (0.78f * lv);
                wet_r = wet_r * (1.0f - 0.60f * lv) + (flt_r * vt_gr) * (0.78f * lv);
            }
            if (cm & (1u << 13)) {
                /* 13: AUTO-WAH — Envelope-following resonant State-Variable Auto-Wah + Stereo Notch */
                float lv = S.cart_lvl[13];
                float ein = fm_fabsf(flt_l) + fm_fabsf(flt_r);
                D.wah_env += (ein > D.wah_env ? 0.008f : 0.00045f) * (ein - D.wah_env);
                float wf = br_clampf(0.045f + 0.34f * D.wah_env + 0.14f * S.cart_param[13], 0.03f, 0.58f);
                float q_inv = 0.28f;
                float hp_l = flt_l - D.wah_lp[0] - q_inv * D.wah_bp[0];
                D.wah_bp[0] += wf * hp_l; D.wah_lp[0] += wf * D.wah_bp[0];
                float hp_r = flt_r - D.wah_lp[1] - q_inv * D.wah_bp[1];
                D.wah_bp[1] += wf * hp_r; D.wah_lp[1] += wf * D.wah_bp[1];
                wet_l = wet_l * (1.0f - 0.55f * lv) + (D.wah_bp[0] + 0.35f * (hp_l + D.wah_lp[0])) * (0.75f * lv);
                wet_r = wet_r * (1.0f - 0.55f * lv) + (D.wah_bp[1] + 0.35f * (hp_r + D.wah_lp[1])) * (0.75f * lv);
            }
            if (cm & (1u << 14)) {
                /* 14: BELL PITCH — Magic Octave-Up / Bell Pitch-Shifted Delay */
                float lv = S.cart_lvl[14];
                wet_l = wet_l * (1.0f - 0.45f * lv) + (echo_l + sh_sig * (0.30f + 0.50f * S.cart_param[14])) * (0.62f * lv);
                wet_r = wet_r * (1.0f - 0.45f * lv) + (echo_r + sh_sig * (0.30f + 0.50f * S.cart_param[14])) * (0.62f * lv);
            }
            if (cm & (1u << 15)) {
                /* 15: TAPE WASH — Ambient Textural Lo-Fi Tape-Loop & Warm Filtered Drone Wash */
                float lv = S.cart_lvl[15];
                float ok = 0.05f + 0.18f * S.cart_param[15];
                D.wash_lp[0] += ok * (br_soft((wet_l + echo_l * 0.65f) * 1.15f) - D.wash_lp[0]);
                D.wash_lp[1] += ok * (br_soft((wet_r + echo_r * 0.65f) * 1.15f) - D.wash_lp[1]);
                wet_l = wet_l * (1.0f - 0.62f * lv) + D.wash_lp[0] * (0.88f * lv);
                wet_r = wet_r * (1.0f - 0.62f * lv) + D.wash_lp[1] * (0.88f * lv);
            }

            out_l[i] = flt_l * (1.0f - 0.42f * c_mix) + wet_l * c_mix;
            out_r[i] = flt_r * (1.0f - 0.42f * c_mix) + wet_r * c_mix;
        }
    }
    /* Single block-end denormal flush (replaces >5,000 per-sample flushes!) */
    for (s = 0; s < 3u; s++) {
        D.spr_lp[s] = fm_flush(D.spr_lp[s]);
        for (k = 0; k < 4u; k++) D.spr_ap[s][k] = fm_flush(D.spr_ap[s][k]);
    }
    for (k = 0; k < 8u; k++) D.fdn_dc_y[k] = fm_flush(D.fdn_dc_y[k]);
    for (k = 0; k < 4u; k++) {
        D.phaser_ap[0][k] = fm_flush(D.phaser_ap[0][k]);
        D.phaser_ap[1][k] = fm_flush(D.phaser_ap[1][k]);
        D.spec_y1[k] = fm_flush(D.spec_y1[k]);
    }
    D.wah_bp[0] = fm_flush(D.wah_bp[0]); D.wah_bp[1] = fm_flush(D.wah_bp[1]);
    D.wah_lp[0] = fm_flush(D.wah_lp[0]); D.wah_lp[1] = fm_flush(D.wah_lp[1]);
    D.wash_lp[0] = fm_flush(D.wash_lp[0]); D.wash_lp[1] = fm_flush(D.wash_lp[1]);
    S.spring_kick_env = D.spr_kick_imp;
}

/* Polled on Core 1 (via plat_cpu2_job or plat_cpu2_run) or run inline in Mode 2 */
void helios_core2_work(void)
{
    uint32_t g = hl_cs_go, t0 = 0;
    if (!g) return;
    if (hl_prof_clk) t0 = hl_prof_clk();

    if (g == 2u) {
        /* Stage A on Core 1: Right Drone Bank V6..V8 (15 saws) + Melodic V5 + Right Analog VCF */
        hl_render_bank(1u, c2_flt_r, B.n);
    } else if (g == 1u) {
        /* Stage B on Core 1: Stereo BBD Tri-Phase Chorus + 8-Line FDN Cartridge Multi-FX */
        hl_render_fx_core(c2_in_l, c2_in_r, c2_wet_l, c2_wet_r, B.n);
    }

    if (hl_prof_clk) {
        hl_c1_busy += hl_prof_clk() - t0;
    }
    __asm__ volatile("" ::: "memory");
    hl_cs_go = 0u;
}

static void hl_c2_tramp(uint32_t arg)
{
    hl_cs_go = arg;
    helios_core2_work();
}

void helios_core2_hooks(int (*run)(void (*)(uint32_t), uint32_t), int (*wait)(void))
{
    hl_c2_run  = run;
    hl_c2_wait = wait;
}

static int hl_cs_idle(void)
{
    uint32_t t = 0;
    if (!hl_cs_go || hl_cs_lost)
        return !hl_cs_go;
    hl_cs_late++;
    while (hl_cs_go && ++t < 20000000u)
        ;
    if (hl_cs_go) {
        hl_cs_mode = 0;
        hl_cs_have = 0;
        hl_cs_lost++;
        return 0;
    }
    return 1;
}

void helios_core2_mode(int mode)
{
    hl_cs_idle();
    hl_cs_have = 0;
    hl_cs_mode = mode;
}

void helios_profile_clock(uint32_t (*clk)(void))
{
    hl_prof_clk = clk;
}

void helios_core2_stats(uint32_t *v_jobs, uint32_t *fx_jobs, uint32_t *late, uint32_t *lost,
                         uint32_t *c1_pct, int *mode)
{
    *v_jobs  = hl_v_jobs;
    *fx_jobs = hl_fx_jobs;
    *late    = hl_cs_late;
    *lost    = hl_cs_lost;
    *mode    = hl_cs_mode;
    if (hl_c0_samples > 0u && hl_prof_clk) {
        /* 24 ticks per microsecond, 44100 samples/sec => ~544.2 ticks per sample budget */
        uint32_t pct = (uint32_t)((float)hl_c1_busy * 100.0f / ((float)hl_c0_samples * 544.217f));
        *c1_pct = pct > 99u ? 99u : pct;
        if (hl_c0_samples > 44100u) {
            hl_c1_busy >>= 1;
            hl_c0_samples >>= 1;
        }
    } else {
        *c1_pct = hl_cs_mode ? 24u : 0u;
    }
}

static float hl_tuning_offset(uint8_t tuning, int semi_from_root)
{
    /* Microtonal pitch offsets in semitones relative to 12-TET */
    static const float JUST_CENTS[12]   = {0.0f, +0.117f, +0.039f, +0.156f, -0.137f, -0.020f, -0.098f, +0.020f, +0.137f, -0.156f, +0.176f, -0.117f};
    static const float PYTH_CENTS[12]   = {0.0f, -0.098f, +0.039f, -0.059f, +0.078f, -0.020f, +0.117f, +0.020f, -0.078f, +0.059f, -0.039f, +0.098f};
    static const float BOHLEN_CENTS[12] = {0.0f, +0.463f, +0.926f, +0.389f, +0.852f, +0.315f, +0.778f, +0.241f, +0.704f, +0.167f, +0.630f, +0.093f};
    static const float MAQAM_CENTS[12]  = {0.0f, -0.15f, -0.50f, +0.05f, -0.50f, 0.0f, -0.15f, +0.02f, -0.15f, -0.50f, +0.05f, -0.45f};
    int pc = semi_from_root % 12;
    if (pc < 0) pc += 12;
    if (tuning == 1u) return JUST_CENTS[pc];
    if (tuning == 2u) return PYTH_CENTS[pc];
    if (tuning == 3u) return BOHLEN_CENTS[pc];
    if (tuning == 4u) return MAQAM_CENTS[pc];
    return 0.0f;
}

float hl_plate_to_note(uint8_t root, uint8_t scale, uint8_t plate, int8_t oct)
{
    uint8_t p_idx = plate % 12u;
    uint8_t deg = HL_SCALES[scale % 7u][p_idx];
    float n = (float)(48 + (int)root + (int)deg + (int)oct * 12) + S.plate_trim[p_idx] + hl_tuning_offset(S.tuning, (int)deg);
    if (n < 24.0f) n = 24.0f;
    if (n > 96.0f) n = 96.0f;
    return n;
}

void helios_load_patch(uint32_t idx)
{
    static const float JUST_OFF[12] = {
        0.0f, +0.12f, +0.04f, +0.16f, -0.02f, +0.02f, -0.10f, +0.02f, +0.14f, -0.16f, +0.18f, -0.12f
    };
    const hl_patch_t *p = &HL_PATCHES[idx % HL_NPATCHES];
    uint32_t i, s;
    S.patch    = (uint8_t)(idx % HL_NPATCHES);
    S.root     = p->root;
    S.scale    = p->scale;
    S.bpm      = p->bpm;
    S.key_mode = p->key_mode % 3u;

    for (i = 0; i < HL_NPLATES; i++) {
        float lim = (p->vibe == 1u) ? 0.04f : 1.0f;
        S.plate_trim[i] = br_clampf(JUST_OFF[i] * p->micro_detune * (p->vibe == 1u ? 0.35f : 2.5f), -lim, lim);
    }

    for (i = 0; i < HL_NDRONE; i++) {
        S.drone[i].note       = p->d_note[i];
        D.d_cur_note[i]       = p->d_note[i];
        S.drone[i].spread     = p->d_spread[i];
        S.drone[i].wave_morph = 0.0f;
        S.drone[i].sub        = p->d_sub[i];
        S.drone[i].hold       = 1;
        S.drone[i].pan        = p->d_pan[i];
        S.drone[i].attack     = 0.45f;
        S.drone[i].release    = 0.65f;
        S.drone[i].glide      = (p->vibe == 1u) ? 0.68f : 0.42f;
        S.drone[i].level      = p->d_lvl[i];
    }
    for (i = 0; i < HL_NMEL; i++) {
        S.mel[i].wave      = p->m_wave[i];
        S.mel[i].octave    = p->m_oct[i];
        S.mel[i].glide     = p->m_glide[i];
        S.mel[i].vibrato   = 0.28f;
        S.mel[i].attack    = 0.20f;
        S.mel[i].release   = 0.55f;
        S.mel[i].level     = p->m_lvl[i];
        S.mel[i].hold      = 1;
        S.mel[i].seq_run   = p->m_seq[i];
        S.mel[i].seq_div   = p->m_div[i];
        S.mel[i].seq_len   = HL_NSTEPS;
        for (s = 0; s < HL_NSTEPS; s++) {
            S.mel[i].steps[s] = p->m_steps[i][s];
            S.mel[i].step_gate[s] = 1u;
            S.mel[i].step_prob[s] = 10u; /* 100% default probability */
            S.mel[i].step_acc[s]  = (s == 0u) ? 1u : 0u;
        }
        S.mel[i].cur_step  = 0;
        S.mel[i].cur_plate = p->m_steps[i][0];
        S.mel[i].press     = 0.35f;
        S.mel[i].cur_note  = hl_plate_to_note(S.root, S.scale, S.mel[i].cur_plate, S.mel[i].octave);
    }
    for (i = 0; i < 2u; i++) {
        S.vcf_cut[i]  = p->vcf_cut[i];
        S.vcf_res[i]  = p->vcf_res[i];
        S.vcf_mode[i] = p->vcf_mode[i];
    }
    S.vcf_drive    = p->vcf_drive;
    S.vcf_lfo      = p->vcf_lfo;
    S.vcf_link     = (p->vcf_mode[0] == p->vcf_mode[1]) ? 1u : 0u;
    S.spring_mix   = (p->vibe == 3u) ? 0.34f : (S.patch == 13u) ? 0.26f : (p->vibe == 1u) ? 0.14f : 0.22f;
    S.spring_decay = 0.55f;
    /* Pure Analog Warmth Studio Finish applied to ALL 20 CALM presets (P01..P20) — zero brassiness! */
    if (p->vibe == 1u) {
        int is_sub_bliss = (S.patch >= 16u && S.patch <= 19u); /* P17..P20 Ultra-Warm Sub-Heavy Bliss */
        for (i = 0; i < HL_NDRONE; i++) {
            S.drone[i].wave_morph = is_sub_bliss ? 0.44f : 0.36f; /* Pure velvet TRI/SAW blend — never harsh! */
            S.drone[i].attack     = 0.74f;
            S.drone[i].release    = 0.86f;
        }
        S.master_tape = is_sub_bliss ? 0.40f : 0.34f;
        S.master_tilt = is_sub_bliss ? -0.44f : -0.26f; /* Heavy warm low-end, silky rolled-off highs */
        S.master_hpf  = 0.0f;
        S.tuning      = (S.patch == 6u || S.patch == 8u || S.patch == 14u) ? 1u : 0u; /* Just Intonation */
        if (S.patch == 10u) { S.mel[0].seq_len = 3u; S.mel[1].seq_len = 5u; } /* AIRPORT TAPE 1/1 polymetric 3 vs 5 */
        if (S.patch == 15u) { S.mel[0].seq_len = 4u; S.mel[1].seq_len = 5u; } /* DISCREET LOOP polymetric 4 vs 5 */
    } else {
        S.master_tape = 0.0f;
        S.master_tilt = 0.0f;
        S.master_hpf  = 0.0f;
        S.tuning      = 0u;
    }
    S.noise_lvl    = p->noise_lvl;
    S.noise_mode   = p->noise_mode;
    S.chorus_mix   = p->chorus_mix;
    S.joy_x        = 0.0f;
    S.joy_y        = 0.0f;
    S.joy_rec_state = 0u;
    S.cart_type    = p->cart_type % HL_NCART;
    S.cart_mask    = (uint16_t)(1u << (S.cart_type & 15u));
    S.cart_freeze  = 0u;
    S.lfo_fast     = 0u;
    S.cart_mix     = p->cart_mix;
    S.cart_decay   = p->cart_decay;
    S.cart_shimmer = p->cart_shimmer;
    S.cart_tone    = p->cart_tone;
    for (i = 0; i < HL_NCART; i++) {
        S.cart_lvl[i]   = (i == (uint32_t)(S.cart_type & 15u)) ? 1.0f : 0.65f;
        S.cart_param[i] = p->cart_tone;
    }
    S.ext_in_lvl   = p->ext_in_lvl;
    S.v45_xmod     = p->v45_xmod;
    S.sh_rate      = p->sh_rate;
    S.slew_amt     = p->slew_amt;
    if (S.vel_sens <= 0.01f) S.vel_sens = 0.78f;
    if (S.at_sens  <= 0.01f) S.at_sens  = 0.85f;
    hl_apply_pin_preset(p->pin_preset);
    /* For CALM patches, remove pitch-wobbling oscillator pins so chords stay lush & dead-in-tune! */
    if (p->vibe == 1u) {
        uint32_t sr;
        for (sr = 0; sr < HL_NMOD_SRC; sr++) {
            S.pin_matrix[sr][HL_DST_DRNL] = 0;
            S.pin_matrix[sr][HL_DST_DRNR] = 0;
            S.pin_matrix[sr][HL_DST_V4VO] = 0;
            S.pin_matrix[sr][HL_DST_V5VO] = 0;
        }
        S.pin_matrix[HL_SRC_LFO1][HL_DST_VCFL] = 1;
        S.pin_matrix[HL_SRC_LFO2][HL_DST_VCFR] = 1;
        S.pin_matrix[HL_SRC_SLEW][HL_DST_CART] = 1;
        S.pin_matrix[HL_SRC_LFO1][HL_DST_VCA]  = 1;
    }
    S.scene_b_set = 0u;
    S.compare_on  = 0u;
    S.morph_pos   = 0.0f;
    hl_snap_scene(&hl_scene_a);
    hl_scene_b = hl_scene_a;
    D.ev_home_root   = S.root;
    D.ev_home_scale  = S.scale;
    D.ev_prog_step   = 0u;
    D.ev_journey_rem = 0u;
    D.ev_last_deg    = 0u;
    D.ev_chord_t     = 0.0f;
    D.ev_timer       = 0.0f;
    D.ev_freeze_t    = 0.0f;
    D.ev_mel_rest_t  = 0.0f;
    D.ev_tgt_ready   = 0u;

    /* If loading a patch while in PLAY 8V or HYBRID mode, keep performance mode clean */
    if (S.perf_mode == HL_PERF_PLAY) {
        for (i = 0; i < HL_NDRONE; i++) {
            S.drone[i].hold = 0u;
            S.drone[i].env  = 0.0f;
            D.d_gate[i]     = 0u;
            D.d_key[i]      = -1;
        }
        for (i = 0; i < HL_NMEL; i++) {
            S.mel[i].hold    = 0u;
            S.mel[i].seq_run = 0u;
            S.mel[i].env     = 0.0f;
            D.m_gate[i]      = 0u;
            D.m_key[i]       = -1;
        }
    } else if (S.perf_mode == HL_PERF_HYBRID) {
        for (i = 0; i < HL_NMEL; i++) {
            S.mel[i].hold    = 0u;
            S.mel[i].seq_run = 0u;
            S.mel[i].env     = 0.0f;
            D.m_gate[i]      = 0u;
            D.m_key[i]       = -1;
        }
    }
}

void helios_load_init(uint8_t pure)
{
    static const float init_notes[HL_NDRONE] = { 48.0f, 55.0f, 60.0f, 36.0f, 43.0f, 52.0f };
    uint32_t i, j;
    S.pure_mode     = pure ? 1u : 0u;
    S.evolve        = pure ? 0u : 1u;
    S.root          = 0u; /* C */
    S.scale         = 0u; /* MINOR */
    S.bpm           = 64u;
    S.perf_mode     = HL_PERF_GEN;
    S.arp_mode      = HL_ARP_OFF;
    S.tuning        = 0u;
    S.chord_mode    = 0u;
    S.sustain_latch = 0u;
    S.play_oct      = 0;
    S.playing       = 1u;
    S.vcf_link      = 1u;
    S.vcf_mode[0]   = 0u;
    S.vcf_mode[1]   = 0u;
    S.vcf_cut[0]    = 0.88f;
    S.vcf_cut[1]    = 0.88f;
    S.vcf_res[0]    = 0.06f;
    S.vcf_res[1]    = 0.06f;
    S.vcf_drive     = 0.0f;
    S.vcf_lfo       = 0.0f;
    S.spring_mix    = 0.0f;
    S.spring_decay  = 0.45f;
    S.cart_type     = 0u;
    S.cart_mask     = 0u; /* All 16 FX Cartridges bypassed */
    S.cart_freeze   = 0u;
    S.lfo_fast      = 0u;
    S.cart_mix      = 0.0f;
    S.cart_decay    = 0.50f;
    S.cart_shimmer  = 0.0f;
    S.cart_tone     = 0.50f;
    for (i = 0; i < HL_NCART; i++) {
        S.cart_lvl[i]   = 0.75f;
        S.cart_param[i] = 0.50f;
    }
    S.chorus_mix    = 0.0f;
    S.noise_lvl     = 0.0f;
    S.noise_mode    = 0u;
    S.ext_src       = 0u;
    S.ext_in_lvl    = 0.0f;
    S.v45_xmod      = 0.0f;
    S.sh_rate       = 0.25f;
    S.slew_amt      = 0.50f;
    S.master_hpf    = 0.0f;
    S.master_tilt   = 0.0f;
    S.master_tape   = 0.12f;
    S.joy_x         = 0.0f;
    S.joy_y         = 0.0f;
    S.joy_rec_state = 0u;
    S.morph_pos     = 0.0f;
    S.pin_preset    = 0u;
    for (i = 0; i < HL_NMOD_SRC; i++)
        for (j = 0; j < HL_NMOD_DST; j++)
            S.pin_matrix[i][j] = 0;
    for (i = 0; i < HL_NPLATES; i++)
        S.plate_trim[i] = 0.0f;
    for (i = 0; i < HL_NDRONE; i++) {
        S.drone[i].note       = init_notes[i];
        S.drone[i].spread     = 0.12f;
        S.drone[i].level      = 0.62f;
        S.drone[i].wave_morph = 0.25f;
        S.drone[i].attack     = 0.25f;
        S.drone[i].release    = 0.55f;
        S.drone[i].glide      = pure ? 0.0f : 0.45f; /* 0 = instant raw hardware pitch in PURE mode */
        S.drone[i].sub        = 0u;
        S.drone[i].pan        = (i < 3u) ? -1 : 1;
        S.drone[i].hold       = 0u; /* All 6 drones silent on INIT until enabled with DRN HOLD */
        S.drone[i].env        = 0.0f;
        D.d_cur_note[i]       = init_notes[i];
        D.d_gate[i]           = 0u;
        D.d_key[i]            = -1;
    }
    S.drone[0].pan = 0; /* V1 centered on INIT */
    for (i = 0; i < HL_NMEL; i++) {
        S.mel[i].wave    = 1u;
        S.mel[i].level   = 0.72f;
        S.mel[i].glide   = 0.15f;
        S.mel[i].attack  = 0.12f;
        S.mel[i].release = 0.50f;
        S.mel[i].vibrato = 0.0f;
        S.mel[i].seq_run = 0u;
        S.mel[i].seq_len = HL_NSTEPS;
        S.mel[i].hold    = 0u;
        S.mel[i].env     = 0.0f;
        D.m_gate[i]      = 0u;
        D.m_key[i]       = -1;
    }
    D.ev_timer      = 0.0f;
    D.ev_freeze_t   = 0.0f;
    D.ev_mel_rest_t = 0.0f;
    D.ev_tgt_ready  = 0u;
    helios_panic(); /* Flush reverb/delay/spring tails so blank INIT starts dead silent */
}

void helios_set_pure_mode(uint8_t on)
{
    S.pure_mode = on ? 1u : 0u;
    if (S.pure_mode) {
        S.evolve = 0u;
        D.ev_tgt_ready = 0u;
    } else if (S.evolve == 0u) {
        S.evolve = 1u;
    }
}

void helios_panic(void)
{
    hl_cs_idle();
    hl_cs_have = 0;
    hl_zero(bbd_l, sizeof bbd_l);
    hl_zero(bbd_r, sizeof bbd_r);
    hl_zero(fdn_buf, sizeof fdn_buf);
    hl_zero(sh_buf, sizeof sh_buf);
    hl_zero(dly_l, sizeof dly_l);
    hl_zero(dly_r, sizeof dly_r);
    hl_zero(spr_buf, sizeof spr_buf);
    hl_zero(c0_flt_l, sizeof c0_flt_l);
    hl_zero(c2_flt_r, sizeof c2_flt_r);
    hl_zero(c2_in_l, sizeof c2_in_l);
    hl_zero(c2_in_r, sizeof c2_in_r);
    hl_zero(c2_wet_l, sizeof c2_wet_l);
    hl_zero(c2_wet_r, sizeof c2_wet_r);
}

void helios_init(float fs)
{
    uint32_t i, k;
    hl_zero(&S, sizeof S);
    hl_zero(&D, sizeof D);
    hl_zero(&B, sizeof B);
    helios_panic();
    hl_fs = fs > 8000.0f ? fs : 44100.0f;
    hl_inv_fs = 1.0f / hl_fs;
    D.rng[0] = 0x48454C49u;
    D.rng[1] = 0x4F533131u;
    S.playing = 1;
    S.evolve  = 1; /* Default to gentle SLOW DRIFT */
    S.perf_mode = HL_PERF_GEN;
    S.arp_mode  = HL_ARP_OFF;
    S.sustain_latch = 0u;
    S.play_oct  = 0;
    S.vel_sens  = 0.78f;
    S.at_sens   = 0.85f;
    for (i = 0; i < HL_NDRONE; i++) {
        for (k = 0; k < HL_NSAW; k++) {
            D.d_ph[i][k] = 0.5f * (br_randf(&D.rng[0]) + 1.0f);
            D.d_drift[i][k] = br_randf(&D.rng[0]) * 0.002f;
        }
        S.drone[i].env = 0.8f;
        D.d_key[i]  = -1;
        D.d_gate[i] = 1u;
        D.d_vel[i]  = 0.85f;
        D.d_at[i]   = 0.0f;
    }
    for (i = 0; i < HL_NMEL; i++) {
        D.m_key[i]  = -1;
        D.m_gate[i] = 1u;
        D.m_vel[i]  = 0.85f;
        D.m_at[i]   = 0.0f;
    }
    D.m_pend_dir[0] = D.m_pend_dir[1] = 1;
    D.arp_dir = 1;
    helios_load_patch(0);
}

void helios_get_state(helios_state_t *out) { *out = S; }
void helios_set_state(const helios_state_t *in) { S = *in; }

const char *helios_patch_name(uint32_t idx)    { return HL_PATCHES[idx % HL_NPATCHES].name; }
uint8_t helios_patch_vibe(uint32_t idx)        { return HL_PATCHES[idx % HL_NPATCHES].vibe; }
const char *helios_cart_name(uint32_t idx)     { return HL_CART_NAMES[idx % HL_NCART]; }
const char *helios_scale_name(uint32_t idx)    { return HL_SCALE_NAMES[idx % 7u]; }
const char *helios_wave_name(uint32_t idx)     { return HL_WAVE_NAMES[idx % 6u]; }
const char *helios_keymode_name(uint8_t mode)  { return HL_KEYMODE_NAMES[mode % 3u]; }
const char *helios_perfmode_name(uint8_t mode) { return HL_PERFMODE_NAMES[mode % 3u]; }
const char *helios_arpmode_name(uint8_t mode)  { return HL_ARPMODE_NAMES[mode % 6u]; }
static void hl_snap_scene(hl_scene_t *sc)
{
    uint32_t i;
    sc->vcf_cut[0]   = S.vcf_cut[0];   sc->vcf_cut[1]   = S.vcf_cut[1];
    sc->vcf_res[0]   = S.vcf_res[0];   sc->vcf_res[1]   = S.vcf_res[1];
    sc->vcf_drive    = S.vcf_drive;
    sc->joy_x        = S.joy_x;        sc->joy_y        = S.joy_y;
    sc->spring_mix   = S.spring_mix;   sc->spring_decay = S.spring_decay;
    sc->cart_mix     = S.cart_mix;     sc->cart_decay   = S.cart_decay;
    sc->cart_shimmer = S.cart_shimmer; sc->chorus_mix   = S.chorus_mix;
    sc->ext_in_lvl   = S.ext_in_lvl;   sc->v45_xmod     = S.v45_xmod;
    for (i = 0; i < HL_NDRONE; i++) {
        sc->d_spread[i] = S.drone[i].spread;
        sc->d_morph[i]  = S.drone[i].wave_morph;
        sc->d_level[i]  = S.drone[i].level;
    }
}

static void hl_apply_scene_lerp(const hl_scene_t *a, const hl_scene_t *b, float t)
{
    uint32_t i;
    float u = 1.0f - t;
    S.vcf_cut[0]   = a->vcf_cut[0] * u   + b->vcf_cut[0] * t;
    S.vcf_cut[1]   = a->vcf_cut[1] * u   + b->vcf_cut[1] * t;
    S.vcf_res[0]   = a->vcf_res[0] * u   + b->vcf_res[0] * t;
    S.vcf_res[1]   = a->vcf_res[1] * u   + b->vcf_res[1] * t;
    S.vcf_drive    = a->vcf_drive * u    + b->vcf_drive * t;
    S.joy_x        = a->joy_x * u        + b->joy_x * t;
    S.joy_y        = a->joy_y * u        + b->joy_y * t;
    S.spring_mix   = a->spring_mix * u   + b->spring_mix * t;
    S.spring_decay = a->spring_decay * u + b->spring_decay * t;
    S.cart_mix     = a->cart_mix * u     + b->cart_mix * t;
    S.cart_decay   = a->cart_decay * u   + b->cart_decay * t;
    S.cart_shimmer = a->cart_shimmer * u + b->cart_shimmer * t;
    S.chorus_mix   = a->chorus_mix * u   + b->chorus_mix * t;
    S.ext_in_lvl   = a->ext_in_lvl * u   + b->ext_in_lvl * t;
    S.v45_xmod     = a->v45_xmod * u     + b->v45_xmod * t;
    for (i = 0; i < HL_NDRONE; i++) {
        S.drone[i].spread     = a->d_spread[i] * u + b->d_spread[i] * t;
        S.drone[i].wave_morph = a->d_morph[i] * u  + b->d_morph[i] * t;
        S.drone[i].level      = a->d_level[i] * u  + b->d_level[i] * t;
    }
}

const char *helios_mod_src_name(unsigned src)  { return HL_MOD_SRC_NAMES[src % HL_NMOD_SRC]; }
const char *helios_mod_dst_name(unsigned dst)  { return HL_MOD_DST_NAMES[dst % HL_NMOD_DST]; }
const char *helios_evolve_name(uint8_t ev)     { return HL_EVOLVE_NAMES[ev & 3u]; }
const char *helios_tuning_name(uint8_t tun)    { return HL_TUNING_NAMES[tun % 5u]; }
const char *helios_chord_name(uint8_t ch)      { return HL_CHORD_NAMES[ch % 6u]; }
const char *helios_extsrc_name(uint8_t src)    { return HL_EXTSRC_NAMES[src % 3u]; }

int helios_wants_usb_in(void)
{
    return (S.ext_src != 0u && S.ext_in_lvl > 0.005f) ? 1 : 0;
}

int helios_get_play_oct(void)
{
    return (int)S.play_oct;
}

void helios_set_spec_en(int on)
{
    D.spec_en = on ? 1u : 0u;
}

void helios_feed_usb_audio(const int32_t *lr, uint32_t n)
{
    uint32_t i;
    if (!lr || n == 0u) { D.usb_in_ready = 0u; return; }
    if (n > HL_MAXN) n = HL_MAXN;
    for (i = 0; i < n; i++) {
        D.usb_in[0][i] = (float)lr[i * 2u + 0u] * (1.0f / 8388608.0f);
        D.usb_in[1][i] = (float)lr[i * 2u + 1u] * (1.0f / 8388608.0f);
    }
    D.usb_in_ready = 1u;
}

void helios_midi_clock(uint32_t now_ms)
{
    if (D.clk_last_ms != 0u && now_ms > D.clk_last_ms) {
        uint32_t dt = now_ms - D.clk_last_ms;
        if (dt >= 5u && dt <= 150u) {
            if (D.clk_dt_avg <= 1.0f) D.clk_dt_avg = (float)dt;
            else D.clk_dt_avg = D.clk_dt_avg * 0.85f + (float)dt * 0.15f;
            {
                /* 24 PPQN -> BPM = 60000 / (24 * dt_ms) = 2500 / dt_ms */
                float bpm_f = 2500.0f / D.clk_dt_avg;
                int b = (int)(bpm_f + 0.5f);
                if (b < 30) b = 30;
                if (b > 200) b = 200;
                S.bpm = (uint8_t)b;
            }
            S.ext_clk_lock = 1u;
            D.clk_age_blocks = 0u;
        }
    }
    D.clk_last_ms = now_ms;
}

void helios_midi_start(void)
{
    uint32_t v;
    S.playing = 1u;
    for (v = 0; v < HL_NMEL; v++) {
        S.mel[v].cur_step = 0u;
        D.m_seq_ph[v] = 0.0f;
    }
    D.arp_ph = 0.0f;
    D.arp_idx = 0;
}

void helios_midi_stop(void)
{
    S.ext_clk_lock = 0u;
    D.clk_last_ms = 0u;
}

void helios_midi_pitch_bend(int16_t bend_14b)
{
    S.pitch_bend = br_clampf((float)bend_14b * (1.0f / 8192.0f), -1.0f, 1.0f);
}

void helios_midi_chan_at(uint8_t press)
{
    D.chan_at = br_clampf((float)press * (1.0f / 127.0f), 0.0f, 1.0f);
}

void helios_midi_poly_at(uint8_t note, uint8_t press)
{
    unsigned k = (note >= 53u) ? (note - 53u) : 0u;
    if (k < HL_NKEYS)
        helios_fm1_key_pressure(k, (float)press * (1.0f / 127.0f));
    else
        helios_midi_chan_at(press);
}

void helios_midi_sustain_pedal(uint8_t down)
{
    uint8_t dn = down ? 1u : 0u;
    if (dn == D.midi_sustain) return;
    D.midi_sustain = dn;
    if (dn) {
        if (!S.sustain_latch) helios_toggle_sustain_latch();
    } else {
        if (S.sustain_latch) helios_toggle_sustain_latch();
    }
}

void helios_capture_scene_b(void)
{
    hl_snap_scene(&hl_scene_b);
    S.scene_b_set = 1u;
    S.morph_pos = 1.0f;
    S.compare_on = 0u;
}

void helios_set_morph(float pos)
{
    pos = br_clampf(pos, 0.0f, 1.0f);
    S.morph_pos = pos;
    if (S.scene_b_set) {
        S.compare_on = 0u;
        hl_apply_scene_lerp(&hl_scene_a, &hl_scene_b, pos);
    }
}

void helios_toggle_compare(void)
{
    if (!S.compare_on) {
        hl_snap_scene(&hl_compare_bak);
        hl_apply_scene_lerp(&hl_scene_a, &hl_scene_a, 0.0f);
        S.compare_on = 1u;
    } else {
        hl_apply_scene_lerp(&hl_compare_bak, &hl_compare_bak, 0.0f);
        S.compare_on = 0u;
    }
}

void helios_step_patch(int dir)
{
    int next = ((int)S.patch + dir) % (int)HL_NPATCHES;
    if (next < 0) next += (int)HL_NPATCHES;
    helios_load_patch((uint32_t)next);
}

void helios_toggle_play(void)     { S.playing ^= 1u; }
void helios_set_evolve(uint8_t e) { S.evolve = e & 3u; }
void helios_cycle_evolve(void)    { S.evolve = (S.evolve + 1u) & 3u; }

void helios_set_perf_mode(uint8_t mode)
{
    static float   saved_d_note[HL_NDRONE];
    static uint8_t saved_d_hold[HL_NDRONE];
    static uint8_t saved_d_valid = 0u;
    uint32_t i;
    uint8_t prev = S.perf_mode;
    uint8_t next = mode % 3u;
    /* Snapshot drone pitches & hold states before PLAY 8V borrows all 6 drone voices for poly keys */
    if (next == HL_PERF_PLAY && prev != HL_PERF_PLAY) {
        for (i = 0; i < HL_NDRONE; i++) {
            saved_d_note[i] = S.drone[i].note;
            saved_d_hold[i] = S.drone[i].hold;
        }
        saved_d_valid = 1u;
    }
    S.perf_mode = next;
    S.held_mask = 0u;
    S.latch_mask = 0u;
    for (i = 0; i < HL_NKEYS; i++) {
        S.key_vel[i] = 0u;
        S.key_at[i]  = 0u;
    }
    if (next == HL_PERF_GEN) {
        if (S.pure_mode) {
            /* PURE HW Mode: Restore pre-PLAY8V drone pitches & hold states; never load factory presets! */
            for (i = 0; i < HL_NDRONE; i++) {
                if (prev == HL_PERF_PLAY && saved_d_valid) {
                    S.drone[i].note = saved_d_note[i];
                    D.d_cur_note[i] = saved_d_note[i];
                    S.drone[i].hold = saved_d_hold[i];
                }
                D.d_gate[i] = S.drone[i].hold;
                D.d_key[i]  = -1;
                D.d_at[i]   = 0.0f;
            }
            for (i = 0; i < HL_NMEL; i++) {
                D.m_gate[i] = S.mel[i].hold;
                D.m_key[i]  = -1;
                D.m_at[i]   = 0.0f;
            }
        } else {
            /* ENTROPY Mode: Restore autonomous generative drone & dual 5-step sequencers */
            const hl_patch_t *p = &HL_PATCHES[S.patch % HL_NPATCHES];
            for (i = 0; i < HL_NDRONE; i++) {
                S.drone[i].note = p->d_note[i];
                S.drone[i].hold = 1u;
                D.d_gate[i]     = 1u;
                D.d_key[i]      = -1;
                D.d_at[i]       = 0.0f;
            }
            for (i = 0; i < HL_NMEL; i++) {
                S.mel[i].hold    = 1u;
                S.mel[i].seq_run = p->m_seq[i];
                D.m_gate[i]      = 1u;
                D.m_key[i]       = -1;
                D.m_at[i]        = 0.0f;
            }
        }
    } else if (next == HL_PERF_HYBRID) {
        /* HYBRID: Left Hand (keys 0..11) directs 6 Drones; Right Hand (keys 12..26) plays V4/V5 solo lead */
        for (i = 0; i < HL_NDRONE; i++) {
            if (S.pure_mode && prev == HL_PERF_PLAY && saved_d_valid) {
                S.drone[i].note = saved_d_note[i];
                D.d_cur_note[i] = saved_d_note[i];
                S.drone[i].hold = saved_d_hold[i];
            } else if (!S.pure_mode) {
                S.drone[i].hold = 1u;
            }
            D.d_gate[i]     = S.drone[i].hold;
            D.d_key[i]      = -1;
            D.d_at[i]       = 0.0f;
        }
        for (i = 0; i < HL_NMEL; i++) {
            S.mel[i].hold    = 0u;
            S.mel[i].seq_run = 0u;
            S.mel[i].attack  = 0.04f;
            S.mel[i].release = 0.25f;
            D.m_gate[i]      = 0u;
            D.m_key[i]       = -1;
            D.m_at[i]        = 0.0f;
        }
    } else {
        /* PLAY 8V: 8-Voice Polyphonic 32-Oscillator Keyboard Instrument (Silent until keys pressed!) */
        for (i = 0; i < HL_NDRONE; i++) {
            S.drone[i].hold    = 0u;
            S.drone[i].attack  = 0.03f;
            S.drone[i].release = 0.22f;
            D.d_gate[i]        = 0u;
            D.d_key[i]         = -1;
            D.d_at[i]          = 0.0f;
            D.d_vel[i]         = 0.8f;
        }
        for (i = 0; i < HL_NMEL; i++) {
            S.mel[i].hold    = 0u;
            S.mel[i].seq_run = 0u;
            S.mel[i].attack  = 0.02f;
            S.mel[i].release = 0.22f;
            D.m_gate[i]      = 0u;
            D.m_key[i]       = -1;
            D.m_at[i]        = 0.0f;
            D.m_vel[i]       = 0.8f;
        }
    }
}

void helios_cycle_perf_mode(void)
{
    helios_set_perf_mode((uint8_t)((S.perf_mode + 1u) % 3u));
}

void helios_set_arp_mode(uint8_t mode)
{
    S.arp_mode = mode % 6u;
    D.arp_ph = 0.0f;
    D.arp_idx = 0;
    D.arp_dir = 1;
}

void helios_cycle_arp_mode(void)
{
    helios_set_arp_mode((uint8_t)((S.arp_mode + 1u) % 6u));
}

void helios_toggle_sustain_latch(void)
{
    uint32_t i;
    S.sustain_latch ^= 1u;
    if (!S.sustain_latch) {
        /* Release any notes that are not physically held right now */
        S.latch_mask = S.held_mask;
        for (i = 0; i < HL_NDRONE; i++) {
            if (D.d_key[i] >= 0 && !(S.held_mask & (1u << (unsigned)D.d_key[i]))) {
                D.d_gate[i] = 0u;
                if (S.perf_mode == HL_PERF_PLAY) S.drone[i].hold = 0u;
            }
        }
        for (i = 0; i < HL_NMEL; i++) {
            if (D.m_key[i] >= 0 && !(S.held_mask & (1u << (unsigned)D.m_key[i]))) {
                D.m_gate[i] = 0u;
                if (S.perf_mode != HL_PERF_GEN) S.mel[i].hold = 0u;
            }
        }
    } else {
        S.latch_mask = S.held_mask;
    }
}

void helios_step_play_octave(int delta)
{
    int nxt = (int)S.play_oct + delta;
    if (nxt < -2) nxt = -2;
    if (nxt > 2)  nxt = 2;
    S.play_oct = (int8_t)nxt;
}

void helios_kick_spring(float intensity)
{
    D.spr_kick_imp = br_clampf(D.spr_kick_imp + intensity, 0.25f, 1.0f);
    S.spring_kick_env = D.spr_kick_imp;
}

void helios_toggle_cart_freeze(void)
{
    S.cart_freeze ^= 1u;
}

void helios_cycle_joy_rec(void)
{
    if (S.joy_rec_state == 0u) {
        S.joy_rec_state = 1u; /* Start recording 8s 2D Joystick gesture */
        D.joy_rec_ph = 0.0f;
        D.joy_rec_len = 0u;
    } else if (S.joy_rec_state == 1u) {
        if (D.joy_rec_len < 4u) {
            S.joy_rec_state = 0u;
        } else {
            S.joy_rec_state = 2u; /* Loop recorded gesture */
            D.joy_rec_ph = 0.0f;
        }
    } else {
        S.joy_rec_state = 0u; /* Return to manual */
    }
}

void helios_touch_plate(unsigned mel_idx, unsigned plate_idx, float vel, float press)
{
    hl_mel_cfg_t *m = &S.mel[mel_idx & 1u];
    m->cur_plate = (uint8_t)(plate_idx % HL_NPLATES);
    if (m->seq_run) {
        m->steps[m->cur_step % HL_NSTEPS] = m->cur_plate;
        m->step_gate[m->cur_step % HL_NSTEPS] = 1u;
    }
    m->hold  = 1u;
    m->press = br_clampf(0.35f + 0.45f * vel + 0.50f * press, 0.25f, 1.0f);
    S.last_vel = vel;
    S.vcf_pluck_env = br_clampf(S.vcf_pluck_env + vel * S.vel_sens * 0.75f, 0.0f, 1.0f);
    D.xmod_pluck    = br_clampf(D.xmod_pluck + vel * S.vel_sens * 0.65f, 0.0f, 1.0f);
}

void helios_retune_drones_to_root(uint8_t new_root)
{
    uint32_t i;
    int diff = (int)(new_root % 12u) - (int)(S.root % 12u);
    S.root = new_root % 12u;
    for (i = 0; i < HL_NDRONE; i++) {
        S.drone[i].note = br_clampf(S.drone[i].note + (float)diff, 24.0f, 84.0f);
    }
}

void helios_trigger_sh_burst(void)
{
    D.sh_val = br_randf(&D.rng[0]);
    S.mel[0].press = 1.0f;
    S.mel[1].press = 1.0f;
    S.vcf_pluck_env = 1.0f;
}

/* Convert FM-1 physical key (0..26 = F3..G5) to MIDI pitch with play_oct and microtonal plate trim */
static float hl_key_to_midi(unsigned key_0_to_26)
{
    int base = 53 + (int)key_0_to_26 + (int)S.play_oct * 12; /* Key 0 on FM-1 = F3 (MIDI 53) */
    if (base < 24) base = 24;
    if (base > 96) base = 96;
    return (float)base + S.plate_trim[key_0_to_26 % HL_NPLATES] * 0.35f;
}

/* Re-voice the 6 Drone Voices from held Left-Hand keys (0..11) in HYBRID mode */
static void hl_hybrid_voice_drones(unsigned trigger_key, float vel, float press)
{
    uint32_t lh_mask = S.latch_mask & 0x0FFFu;
    uint8_t held_k[6];
    uint32_t n_held = 0u, k, i;
    if (lh_mask == 0u) lh_mask = (1u << (trigger_key % 12u));
    for (k = 0; k < 12u && n_held < 6u; k++) {
        if (lh_mask & (1u << k))
            held_k[n_held++] = (uint8_t)k;
    }
    if (n_held == 0u) {
        held_k[0] = (uint8_t)(trigger_key % 12u);
        n_held = 1u;
    }
    S.root = (uint8_t)((5u + held_k[0]) % 12u); /* Key 0 = F (pitch class 5) */
    if (n_held == 1u) {
        /* 1 key held: Lush 6-drone root + fifth + octave + sub-fifth chord stack */
        static const int8_t STACK[6] = {-12, -5, 0, 7, 12, 19};
        float root_midi = 41.0f + (float)held_k[0] + (float)S.play_oct * 12.0f;
        for (i = 0; i < HL_NDRONE; i++) {
            S.drone[i].note = br_clampf(root_midi + (float)STACK[i], 24.0f, 84.0f);
            S.drone[i].hold = 1u;
            D.d_key[i]      = (int8_t)held_k[0];
            D.d_gate[i]     = 1u;
            D.d_vel[i]      = vel;
            D.d_at[i]       = press;
            D.d_pluck[i]    = vel;
        }
    } else {
        /* Multi-key left-hand chord: distribute held chord tones across Left (Core 0) & Right (Core 1) drone banks! */
        for (i = 0; i < HL_NDRONE; i++) {
            uint8_t hk = held_k[i % n_held];
            int oct_off = (i < n_held) ? -12 : 0;
            float nm = 53.0f + (float)hk + (float)oct_off + (float)S.play_oct * 12.0f;
            S.drone[i].note = br_clampf(nm, 24.0f, 84.0f);
            S.drone[i].hold = 1u;
            D.d_key[i]      = (int8_t)hk;
            D.d_gate[i]     = 1u;
            D.d_vel[i]      = vel;
            D.d_at[i]       = press;
            D.d_pluck[i]    = vel;
        }
    }
}

/* Allocate or retrigger one of the 6 5-Saw Supersaw Voices + 2 Multi-Wave Melodic Voices in PLAY 8V mode */
static void hl_play8v_note_on(unsigned key_0_to_26, float vel, float press)
{
    uint32_t i;
    int best_d = -1;
    uint32_t best_age = 0xFFFFFFFFu;
    float midi_note = hl_key_to_midi(key_0_to_26);

    D.voice_clock++;

    /* 1. Check if this key is already assigned to one of the 6 5-saw voices */
    for (i = 0; i < HL_NDRONE; i++) {
        if (D.d_key[i] == (int8_t)key_0_to_26) {
            best_d = (int)i;
            break;
        }
    }
    /* 2. Prefer an idle/released voice with lowest envelope */
    if (best_d < 0) {
        float min_env = 99.0f;
        for (i = 0; i < HL_NDRONE; i++) {
            if (!D.d_gate[i] && S.drone[i].env < min_env) {
                min_env = S.drone[i].env;
                best_d = (int)i;
            }
        }
    }
    /* 3. Steal the oldest held voice */
    if (best_d < 0) {
        for (i = 0; i < HL_NDRONE; i++) {
            if (D.d_age[i] < best_age) {
                best_age = D.d_age[i];
                best_d = (int)i;
            }
        }
    }
    if (best_d >= 0) {
        uint32_t vi = (uint32_t)best_d;
        S.drone[vi].note = midi_note;
        S.drone[vi].hold = 1u;
        D.d_key[vi]      = (int8_t)key_0_to_26;
        D.d_gate[vi]     = 1u;
        D.d_vel[vi]      = vel;
        D.d_at[vi]       = press;
        D.d_pluck[vi]    = vel;
        D.d_age[vi]      = D.voice_clock;
    }

    /* Also layer Melodic Voice V4 (Core 0) or V5 (Core 1) for multi-wave body & V4<->V5 X-MOD bite! */
    {
        unsigned mv = D.duo_turn & 1u;
        D.duo_turn ^= 1u;
        if (!D.m_gate[0]) mv = 0u;
        else if (!D.m_gate[1]) mv = 1u;
        S.mel[mv].cur_plate = (uint8_t)(key_0_to_26 % HL_NPLATES);
        S.mel[mv].cur_note  = midi_note + (float)S.mel[mv].octave * 12.0f;
        S.mel[mv].hold      = 1u;
        S.mel[mv].seq_run   = 0u;
        S.mel[mv].press     = br_clampf(0.30f + 0.70f * (press > vel ? press : vel), 0.25f, 1.0f);
        D.m_key[mv]         = (int8_t)key_0_to_26;
        D.m_gate[mv]        = 1u;
        D.m_vel[mv]         = vel;
        D.m_at[mv]          = press;
        D.m_pluck[mv]       = vel;
        D.m_age[mv]         = D.voice_clock;
    }
}

/* 16 bottom-row ("white") capacitive keys on M-VAVE FM-1 (F3..G5); upper black keys ignored in Touch Plate mode */
static const uint8_t HL_WHITE_KEYS[16] = {
    0u, 2u, 4u, 6u, 7u, 9u, 11u, 12u,
    14u, 16u, 18u, 19u, 21u, 23u, 24u, 26u
};

static int hl_key_to_white(unsigned key)
{
    int i;
    for (i = 0; i < 16; i++)
        if ((unsigned)HL_WHITE_KEYS[i] == key) return i;
    return -1;
}

/* Map the M-VAVE FM-1's 27 Velocity + Polyphonic-Aftertouch Keys (0..26 = F3..G5) across GEN / HYBRID / PLAY 8V */
void helios_fm1_key_on(unsigned key_0_to_26, float vel, float press)
{
    uint32_t i;
    if (key_0_to_26 >= HL_NKEYS) return;

    vel   = br_clampf(vel, 0.05f, 1.0f);
    press = br_clampf(press, 0.0f, 1.0f);

    /* If Sustain Latch is active and all physical keys were up before this new press, clear the old latched chord */
    if (S.sustain_latch && S.held_mask == 0u && S.perf_mode == HL_PERF_PLAY) {
        S.latch_mask = 0u;
        for (i = 0; i < HL_NDRONE; i++) {
            D.d_gate[i] = 0u;
            S.drone[i].hold = 0u;
        }
        for (i = 0; i < HL_NMEL; i++) {
            D.m_gate[i] = 0u;
            S.mel[i].hold = 0u;
        }
    }

    S.held_mask  |= (1u << key_0_to_26);
    S.latch_mask |= (1u << key_0_to_26);
    S.key_vel[key_0_to_26] = (uint8_t)(vel * 127.0f + 0.5f);
    S.key_at[key_0_to_26]  = (uint8_t)(press * 127.0f + 0.5f);
    S.last_vel = vel;
    if (press > S.max_at) S.max_at = press;

    /* Trigger global Velocity Analog VCF Pluck & V4<->V5 FM Cross-Mod Transient Bite */
    S.vcf_pluck_env = br_clampf(S.vcf_pluck_env + vel * S.vel_sens * 0.85f, 0.0f, 1.0f);
    D.xmod_pluck    = br_clampf(D.xmod_pluck + vel * S.vel_sens * 0.75f, 0.0f, 1.0f);

    /* ---------------- MODE 2: PLAY 8V (8-Voice Polyphonic 32-Oscillator Synth + Chord Memory) ---------------- */
    if (S.perf_mode == HL_PERF_PLAY) {
        static const int8_t CHORD_INT[6][4] = {
            {0, -1, -1, -1},   /* 0: OFF (Single note) */
            {0,  7, 12, -1},   /* 1: 5TH PWR */
            {0,  3,  7, 10},   /* 2: MIN 7 */
            {0,  4,  7, 14},   /* 3: MAJ 9 */
            {0,  2,  7, 12},   /* 4: SUS 2 */
            {0,  2,  5,  7}    /* 5: AMBIENT CLUSTER */
        };
        const int8_t *ci = CHORD_INT[S.chord_mode % 6u];
        for (i = 0; i < 4u; i++) {
            if (ci[i] < 0) break;
            unsigned ck = key_0_to_26 + (unsigned)ci[i];
            if (ck < HL_NKEYS) hl_play8v_note_on(ck, vel, press);
        }
        return;
    }

    /* ---------------- MODE 1: HYBRID (Left-Hand Drone Chords 0..11 + Right-Hand Lead 12..26) ---------------- */
    if (S.perf_mode == HL_PERF_HYBRID) {
        if (key_0_to_26 < 12u) {
            /* Left Hand (F3..E4): Re-voice the 6 Drone Voices to left-hand root / triad */
            hl_hybrid_voice_drones(key_0_to_26, vel, press);
        } else {
            /* Right Hand (F4..G5): Expressive Velocity + Polyphonic-Aftertouch Lead on V4 & V5 */
            float lead_midi = hl_key_to_midi(key_0_to_26);
            unsigned pl = (key_0_to_26 - 12u) % HL_NPLATES;
            D.voice_clock++;
            if (S.key_mode == 2u) {
                /* POLY 2V Duophonic Right-Hand Split between V4 and V5 */
                unsigned mv = (!D.m_gate[0]) ? 0u : (!D.m_gate[1]) ? 1u : (D.duo_turn & 1u);
                D.duo_turn ^= 1u;
                S.mel[mv].cur_plate = (uint8_t)pl;
                if (!D.m_gate[mv] && S.mel[mv].env < 0.08f)
                    S.mel[mv].cur_note = lead_midi + (float)S.mel[mv].octave * 12.0f;
                S.mel[mv].hold    = 1u;
                S.mel[mv].seq_run = 0u;
                S.mel[mv].press   = br_clampf(0.30f + 0.70f * (press > vel ? press : vel), 0.25f, 1.0f);
                D.m_key[mv]       = (int8_t)key_0_to_26;
                D.m_gate[mv]      = 1u;
                D.m_vel[mv]       = vel;
                D.m_at[mv]        = press;
                D.m_pluck[mv]     = vel;
                D.m_age[mv]       = D.voice_clock;
            } else {
                /* Stereo Unison / Interval Lead on both V4 (Core 0) & V5 (Core 1) */
                for (i = 0; i < HL_NMEL; i++) {
                    S.mel[i].cur_plate = (uint8_t)pl;
                    if (!D.m_gate[i] && S.mel[i].env < 0.08f)
                        S.mel[i].cur_note = lead_midi + (float)S.mel[i].octave * 12.0f;
                    S.mel[i].hold    = 1u;
                    S.mel[i].seq_run = 0u;
                    S.mel[i].press   = br_clampf(0.30f + 0.70f * (press > vel ? press : vel), 0.25f, 1.0f);
                    D.m_key[i]       = (int8_t)key_0_to_26;
                    D.m_gate[i]      = 1u;
                    D.m_vel[i]       = vel;
                    D.m_at[i]        = press;
                    D.m_pluck[i]     = vel;
                    D.m_age[i]       = D.voice_clock;
                }
            }
        }
        return;
    }

    /* ---------------- MODE 0: GEN / PURE (Bottom-Row White Keys Only = 12 Flat Touch Plates) ---------------- */
    {
        int wi = hl_key_to_white(key_0_to_26);
        if (wi < 0) {
            /* Upper-row "black" keys are ignored in Touch Plate mode */
            S.held_mask  &= ~(1u << key_0_to_26);
            S.latch_mask &= ~(1u << key_0_to_26);
            return;
        }
        if (wi == 14) {
            /* Bottom-row F5 (key 24): Retune 6 Drone Voices to Melodic V4's active plate */
            uint8_t deg = HL_SCALES[S.scale % 6u][S.mel[0].cur_plate % 12u];
            helios_retune_drones_to_root((uint8_t)((S.root + deg) % 12u));
            return;
        }
        if (wi == 15) {
            /* Bottom-row G5 (key 26): Vector Joystick Y Swell to +85% */
            S.joy_y = 0.85f;
            return;
        }
        if (wi >= 12) return; /* White keys 12..13 (D5, E5) unused spacer */

        /* White keys 0..11 (F3..C5) = 12 flat capacitive touch plates */
        {
            unsigned pl = (unsigned)wi;
            unsigned mv;
            if (S.key_mode == 0u) {
                /* SPLIT 6+6: Left 6 white keys (F3..D4) -> V4 (Plates 0..5); Right 6 (E4..C5) -> V5 (Plates 6..11) */
                mv = (pl < 6u) ? 0u : 1u;
            } else {
                /* DUAL 12 / POLY 2V: Independent 2-voice assignment across all 12 bottom-row plates!
                 * If Sustain Latch is ON and this plate is already latched, tapping it again toggles that voice OFF! */
                if (S.sustain_latch) {
                    for (i = 0; i < HL_NMEL; i++) {
                        if (S.mel[i].hold && S.mel[i].cur_plate == (uint8_t)pl) {
                            S.mel[i].hold = 0u;
                            D.m_gate[i]   = 0u;
                            D.m_key[i]    = -1;
                            return;
                        }
                    }
                }
                if (!D.m_gate[0] && !S.mel[0].hold)      mv = 0u;
                else if (!D.m_gate[1] && !S.mel[1].hold) mv = 1u;
                else { mv = D.duo_turn & 1u; D.duo_turn ^= 1u; }
            }
            /* In SPLIT 6+6 with Sustain Latch ON, tapping an already-latched plate toggles it OFF */
            if (S.key_mode == 0u && S.sustain_latch && S.mel[mv].hold && S.mel[mv].cur_plate == (uint8_t)pl) {
                S.mel[mv].hold = 0u;
                D.m_gate[mv]   = 0u;
                D.m_key[mv]    = -1;
                return;
            }
            helios_touch_plate(mv, pl, vel, press);
            D.m_key[mv]  = (int8_t)key_0_to_26;
            D.m_gate[mv] = 1u;
        }
    }
}

void helios_fm1_key_pressure(unsigned key_0_to_26, float press)
{
    uint32_t i;
    if (key_0_to_26 >= HL_NKEYS) return;
    press = br_clampf(press, 0.0f, 1.0f);
    S.key_at[key_0_to_26] = (uint8_t)(press * 127.0f + 0.5f);

    /* Update per-voice polyphonic aftertouch for every voice currently playing this key */
    for (i = 0; i < HL_NDRONE; i++) {
        if (D.d_key[i] == (int8_t)key_0_to_26 ||
            (S.perf_mode == HL_PERF_HYBRID && key_0_to_26 < 12u)) {
            D.d_at[i] = press;
        }
    }
    for (i = 0; i < HL_NMEL; i++) {
        if (D.m_key[i] == (int8_t)key_0_to_26) {
            D.m_at[i] = press;
            S.mel[i].press = br_clampf(0.30f + 0.70f * press, S.mel[i].press, 1.0f);
        }
    }

    if (S.perf_mode == HL_PERF_GEN) {
        if (key_0_to_26 == 26u) {
            S.joy_y = br_clampf(0.35f + 0.65f * press, -1.0f, 1.0f);
            return;
        }
        if (key_0_to_26 >= 24u) return;
        if (S.key_mode == 0u) {
            unsigned v = (key_0_to_26 < 12u) ? 0u : 1u;
            S.mel[v].press = br_clampf(0.30f + 0.70f * press, S.mel[v].press, 1.0f);
        } else {
            S.mel[0].press = br_clampf(0.30f + 0.70f * press, S.mel[0].press, 1.0f);
            S.mel[1].press = br_clampf(0.30f + 0.70f * press, S.mel[1].press, 1.0f);
        }
    }
}

void helios_fm1_key_off(unsigned key_0_to_26)
{
    uint32_t i;
    if (key_0_to_26 >= HL_NKEYS) return;

    S.held_mask &= ~(1u << key_0_to_26);
    S.key_at[key_0_to_26] = 0u;
    if (!S.sustain_latch) {
        S.latch_mask &= ~(1u << key_0_to_26);
    }

    if (S.perf_mode == HL_PERF_PLAY) {
        if (!S.sustain_latch) {
            static const int8_t CHORD_INT[6][4] = {
                {0, -1, -1, -1}, {0, 7, 12, -1}, {0, 3, 7, 10},
                {0, 4, 7, 14}, {0, 2, 7, 12}, {0, 2, 5, 7}
            };
            const int8_t *ci = CHORD_INT[S.chord_mode % 6u];
            uint32_t c;
            for (c = 0; c < 4u; c++) {
                if (ci[c] < 0) break;
                int8_t tk = (int8_t)(key_0_to_26 + (unsigned)ci[c]);
                for (i = 0; i < HL_NDRONE; i++) {
                    if (D.d_key[i] == tk) {
                        D.d_gate[i] = 0u;
                        D.d_at[i]   = 0.0f;
                        S.drone[i].hold = 0u;
                    }
                }
                for (i = 0; i < HL_NMEL; i++) {
                    if (D.m_key[i] == tk) {
                        D.m_gate[i] = 0u;
                        D.m_at[i]   = 0.0f;
                        S.mel[i].hold = 0u;
                    }
                }
            }
        }
        return;
    }

    if (S.perf_mode == HL_PERF_HYBRID) {
        if (key_0_to_26 < 12u) {
            /* Left hand released: relax aftertouch pressure; drones keep holding chord unless sustain_latch is off and user double-taps */
            for (i = 0; i < HL_NDRONE; i++) D.d_at[i] = 0.0f;
        } else if (!S.sustain_latch) {
            /* Right hand lead key released: check if another right-hand key is still held for legato trill! */
            uint32_t rh_held = S.held_mask & 0x07FFF000u;
            for (i = 0; i < HL_NMEL; i++) {
                if (D.m_key[i] == (int8_t)key_0_to_26) {
                    if (rh_held) {
                        /* Legato fallback to highest still-held right-hand key */
                        int k;
                        for (k = 26; k >= 12; k--) {
                            if (rh_held & (1u << (unsigned)k)) {
                                D.m_key[i] = (int8_t)k;
                                S.mel[i].cur_plate = (uint8_t)((k - 12) % HL_NPLATES);
                                break;
                            }
                        }
                    } else {
                        D.m_gate[i]   = 0u;
                        D.m_at[i]     = 0.0f;
                        S.mel[i].hold = 0u;
                    }
                }
            }
        }
        return;
    }

    if (key_0_to_26 == 26u) {
        S.joy_y = 0.0f;
        return;
    }
    /* Momentary Touch Plate release in GEN / PURE mode (unless Sustain Latch is ON) */
    if (!S.sustain_latch) {
        for (i = 0; i < HL_NMEL; i++) {
            if (D.m_key[i] == (int8_t)key_0_to_26) {
                D.m_gate[i]   = 0u;
                D.m_at[i]     = 0.0f;
                D.m_key[i]    = -1;
                S.mel[i].hold = 0u;
            }
        }
    }
}

void helios_toggle_hold(unsigned col_idx)
{
    if (col_idx < 3u) S.drone[col_idx].hold ^= 1u;
    else if (col_idx == 3u) S.mel[0].seq_run ^= 1u;
    else if (col_idx == 4u) { S.joy_x = 0.0f; S.joy_y = 0.0f; }
    else if (col_idx == 5u) S.mel[1].seq_run ^= 1u;
    else if (col_idx >= 6u && col_idx <= 8u) S.drone[col_idx - 3u].hold ^= 1u;
}

void helios_toggle_step_gate(unsigned mel_idx, unsigned step_idx)
{
    hl_mel_cfg_t *m = &S.mel[mel_idx & 1u];
    m->step_gate[step_idx % HL_NSTEPS] ^= 1u;
}

void helios_pin_step(unsigned src, unsigned dst, int delta)
{
    static const int8_t PIN_ORDER[4] = {0, 1, 2, -1};
    int8_t cur = S.pin_matrix[src % HL_NMOD_SRC][dst % HL_NMOD_DST];
    int idx = 0, k;
    for (k = 0; k < 4; k++) if (PIN_ORDER[k] == cur) { idx = k; break; }
    idx = (idx + (delta >= 0 ? 1 : 3)) & 3;
    S.pin_matrix[src % HL_NMOD_SRC][dst % HL_NMOD_DST] = PIN_ORDER[idx];
}

void helios_pin_clear(unsigned src, unsigned dst, int clear_all)
{
    if (clear_all) hl_zero(S.pin_matrix, sizeof S.pin_matrix);
    else S.pin_matrix[src % HL_NMOD_SRC][dst % HL_NMOD_DST] = 0;
}

void helios_pin_preset_step(int dir)
{
    int nxt = ((int)S.pin_preset + dir) % 5;
    if (nxt < 0) nxt += 5;
    hl_apply_pin_preset((uint8_t)nxt);
}

void helios_adjust_cv_gen(unsigned slot, int delta)
{
    float step = (float)delta * 0.04f;
    switch (slot & 3u) {
    case 0: S.sh_rate    = br_clampf(S.sh_rate + step, 0.0f, 1.0f); break;
    case 1: S.slew_amt   = br_clampf(S.slew_amt + step, 0.0f, 1.0f); break;
    case 2: S.ext_in_lvl = br_clampf(S.ext_in_lvl + step, 0.0f, 1.0f); break;
    case 3: S.v45_xmod   = br_clampf(S.v45_xmod + step, 0.0f, 1.0f); break;
    }
}

void helios_render(int32_t *lr, uint32_t n, float gain)
{
    uint32_t i, v, k, src, dst;
    float dt_block;
    float mod_dst[HL_NMOD_DST];
    float pk_l = 0.0f, pk_r = 0.0f;

    if (n > HL_MAXN) n = HL_MAXN;
    dt_block = (float)n * hl_inv_fs;

    if (!S.playing || gain <= 1e-5f) {
        for (i = 0; i < n * 2u; i++) lr[i] = 0;
        S.vu_l *= 0.85f; S.vu_r *= 0.85f;
        return;
    }

    hl_c0_samples += n;

    /* 1A. 8-Second 2D Vector Joystick Motion Recorder (64 keyframes @ 8 Hz) */
    if (S.joy_rec_state == 1u) {
        D.joy_rec_ph += 8.0f * dt_block;
        if (D.joy_rec_ph >= 1.0f) {
            D.joy_rec_ph -= 1.0f;
            if (D.joy_rec_len < 64u) {
                D.joy_rec_x[D.joy_rec_len] = S.joy_x;
                D.joy_rec_y[D.joy_rec_len] = S.joy_y;
                D.joy_rec_len++;
            } else {
                S.joy_rec_state = 2u; /* Auto-loop after 8 seconds */
                D.joy_rec_ph = 0.0f;
            }
        }
    } else if (S.joy_rec_state == 2u && D.joy_rec_len >= 2u) {
        D.joy_rec_ph += 8.0f * dt_block;
        while (D.joy_rec_ph >= (float)D.joy_rec_len) D.joy_rec_ph -= (float)D.joy_rec_len;
        {
            uint32_t i0 = (uint32_t)D.joy_rec_ph % (uint32_t)D.joy_rec_len;
            uint32_t i1 = (i0 + 1u) % (uint32_t)D.joy_rec_len;
            float fr = D.joy_rec_ph - (float)(uint32_t)D.joy_rec_ph;
            S.joy_x = D.joy_rec_x[i0] + (D.joy_rec_x[i1] - D.joy_rec_x[i0]) * fr;
            S.joy_y = D.joy_rec_y[i0] + (D.joy_rec_y[i1] - D.joy_rec_y[i0]) * fr;
        }
    }

    /* 1B. Entropy Harmonic Composer & Rarity-Weighted Living Evolution Engine (with Analog Warmth Guard for CALM patches!) */
    if (!S.pure_mode && S.evolve > 0u && S.perf_mode != HL_PERF_PLAY) {
        int warm_guard = (HL_PATCHES[S.patch % HL_NPATCHES].vibe == 1u);
        float ev_spd = S.evolve == 1u ? 0.032f : S.evolve == 2u ? 0.075f : 0.145f;
        if (warm_guard) ev_spd *= 0.72f;
        D.ev_joy_ph += ev_spd * dt_block;
        if (D.ev_joy_ph >= 1.0f) D.ev_joy_ph -= 1.0f;
        if (S.joy_rec_state == 0u) {
            S.joy_x = 0.55f * fm_sin_turns(D.ev_joy_ph) + 0.25f * fm_sin_turns(D.ev_joy_ph * 2.0f + 0.17f);
            S.joy_y = 0.50f * fm_sin_turns(D.ev_joy_ph + 0.25f) + 0.25f * fm_sin_turns(D.ev_joy_ph * 3.0f);
        }

        /* Initialize smooth evolution targets from current state on first tick */
        if (!D.ev_tgt_ready) {
            D.ev_tgt_cut[0] = S.vcf_cut[0];   D.ev_tgt_cut[1] = S.vcf_cut[1];
            D.ev_tgt_res[0] = S.vcf_res[0];   D.ev_tgt_res[1] = S.vcf_res[1];
            D.ev_tgt_drive  = S.vcf_drive;    D.ev_tgt_spr    = S.spring_mix;
            D.ev_tgt_cmix   = S.cart_mix;     D.ev_tgt_shim   = S.cart_shimmer;
            D.ev_tgt_chor   = S.chorus_mix;
            for (v = 0; v < HL_NDRONE; v++) {
                D.ev_tgt_morph[v]  = S.drone[v].wave_morph;
                D.ev_tgt_spread[v] = S.drone[v].spread;
                if (D.d_cur_note[v] < 12.0f) D.d_cur_note[v] = S.drone[v].note;
            }
            D.ev_tgt_ready = 1u;
        }

        /* Auto-release temporary Cartridge Freeze after its storm window expires */
        if (D.ev_freeze_t > 0.0f) {
            D.ev_freeze_t -= dt_block;
            if (D.ev_freeze_t <= 0.0f) {
                D.ev_freeze_t = 0.0f;
                S.cart_freeze = 0u;
            }
        }
        /* Melodic breathing rest countdown */
        if (D.ev_mel_rest_t > 0.0f) {
            D.ev_mel_rest_t -= dt_block;
            if (D.ev_mel_rest_t <= 0.0f) {
                D.ev_mel_rest_t = 0.0f;
                S.mel[0].hold = 1u;
                S.mel[1].hold = 1u;
            }
        }

        /* Smooth analog parameter glide toward evolution targets (zero clicks!) */
        if (!S.compare_on && !S.scene_b_set) {
            float kg = 1.0f - fm_expf(-dt_block * (S.evolve == 3u ? 0.32f : S.evolve == 2u ? 0.20f : 0.12f));
            S.vcf_cut[0]   += (D.ev_tgt_cut[0] - S.vcf_cut[0]) * kg;
            S.vcf_cut[1]   += (D.ev_tgt_cut[1] - S.vcf_cut[1]) * kg;
            S.vcf_res[0]   += (D.ev_tgt_res[0] - S.vcf_res[0]) * kg;
            S.vcf_res[1]   += (D.ev_tgt_res[1] - S.vcf_res[1]) * kg;
            S.vcf_drive    += (D.ev_tgt_drive  - S.vcf_drive)  * kg;
            S.spring_mix   += (D.ev_tgt_spr    - S.spring_mix) * kg;
            S.cart_mix     += (D.ev_tgt_cmix   - S.cart_mix)   * kg;
            S.cart_shimmer += (D.ev_tgt_shim   - S.cart_shimmer) * kg;
            S.chorus_mix   += (D.ev_tgt_chor   - S.chorus_mix) * kg;
            for (v = 0; v < HL_NDRONE; v++) {
                S.drone[v].wave_morph += (D.ev_tgt_morph[v]  - S.drone[v].wave_morph) * kg;
                S.drone[v].spread     += (D.ev_tgt_spread[v] - S.drone[v].spread)     * kg;
            }
        }

        /* ---- A. HARMONIC CONDUCTOR: Gorgeous Open-Voiced Chord Progressions + Sparkling Chord-Tracking Arps (GEN mode) ---- */
        if (S.perf_mode == HL_PERF_GEN) {
            float chord_dur = (S.evolve == 3u) ? 7.5f : (S.evolve == 2u) ? 11.0f : 16.0f;
            D.ev_chord_t += dt_block;
            if (D.ev_chord_t >= chord_dur) {
                uint32_t r = br_rand(&D.rng[0]);
                uint8_t sc = S.scale % 7u;
                D.ev_chord_t = 0.0f;

                /* Manage Key Journeys (Circle of Fifths: +5 fourth / +7 fifth / relative major-minor) */
                if (D.ev_journey_rem > 0u) {
                    D.ev_journey_rem--;
                    if (D.ev_journey_rem == 0u) {
                        S.root  = D.ev_home_root % 12u;
                        S.scale = D.ev_home_scale % 7u;
                        sc = S.scale;
                    }
                } else if (S.evolve >= 2u && (D.ev_prog_step & 3u) == 3u && (r % 100u) < (S.evolve == 3u ? 50u : 30u)) {
                    static const int8_t J_OFF[4] = {+5, +7, +5, +7}; /* Pure consonant 4th & 5th journeys for warm beauty */
                    int nr = ((int)D.ev_home_root + (int)J_OFF[(r >> 8) & 3u] + 12) % 12;
                    S.root = (uint8_t)nr;
                    D.ev_journey_rem = (uint8_t)(2u + ((r >> 12) % 3u));
                }

                if (warm_guard) {
                    /* GORGEOUS OPEN-VOICED AMBIENT CHORDS (Zero minor-2nd clashes, zero tritones!)
                     * Each chord is 6 exact semitone offsets above bass root (V1..V8):
                     *   Major (sc == 6 || sc == 3 || sc == 4): I(add9) -> V -> vi7 -> IVmaj7
                     *   Minor/Dorian (sc == 0 || sc == 2):     i(add9) -> bVImaj7 -> bIII(add9) -> bVII */
                    static const uint8_t MAJ_CHORDS[4][6] = {
                        {0, 7, 12, 16, 19, 26}, /* I  Maj add9 (Root, 5, Oct, Maj3, 5, 9) */
                        {7, 14, 19, 23, 26, 31}, /* V  Maj open */
                        {9, 16, 21, 24, 28, 31}, /* vi Min7 lush */
                        {5, 12, 17, 21, 24, 28}  /* IV Maj7 angelic */
                    };
                    static const uint8_t MIN_CHORDS[4][6] = {
                        {0, 7, 12, 15, 19, 26}, /* i    Min add9 */
                        {8, 15, 20, 24, 27, 31}, /* bVI  Maj7 warm */
                        {3, 10, 15, 19, 22, 29}, /* bIII Maj add9 */
                        {10, 17, 22, 26, 29, 34} /* bVII Maj open */
                    };
                    /* Sparkling chord-tone arpeggio plate indices (scale degrees) for V4 & V5 */
                    static const uint8_t ARP_DEG[4][5] = {
                        {0, 2, 4, 7, 4}, /* I  / i  arp: 1-3-5-8-5 */
                        {4, 6, 8, 11, 8},/* V  / bVI arp */
                        {5, 7, 9, 11, 7},/* vi / bIII arp */
                        {3, 5, 7, 10, 7} /* IV / bVII arp */
                    };
                    int is_maj = (sc == 6u || sc == 3u || sc == 4u);
                    uint8_t ci = D.ev_prog_step & 3u;
                    const uint8_t *ch = is_maj ? MAJ_CHORDS[ci] : MIN_CHORDS[ci];
                    int bass_base = 36 + (int)S.root;
                    if (bass_base > 43) bass_base -= 12; /* Keep bass rich & warm (C2..G2) */
                    for (v = 0; v < HL_NDRONE; v++) {
                        S.drone[v].note = br_clampf((float)(bass_base + (int)ch[v]), 28.0f, 76.0f);
                    }
                    /* Update V4 & V5 5-step sequences into a sparkling arpeggio of the active chord! */
                    for (k = 0; k < HL_NSTEPS; k++) {
                        S.mel[0].steps[k] = ARP_DEG[ci][k] % HL_NPLATES;
                        S.mel[1].steps[k] = ARP_DEG[ci][4u - k] % HL_NPLATES; /* Counter-arpeggio on Right! */
                    }
                    D.ev_prog_step++;
                } else {
                    static const uint8_t PROG[7][4] = {
                        {0, 5, 6, 3}, {0, 1, 3, 6}, {0, 3, 6, 4}, {0, 1, 4, 5},
                        {0, 4, 3, 1}, {0, 5, 4, 3}, {0, 4, 5, 3}
                    };
                    static const uint8_t VOICE_COL[6][6] = {
                        {0, 4, 2, 4, 6, 2}, {0, 4, 2, 6, 8, 4}, {0, 4, 1, 4, 7, 4},
                        {0, 4, 0, 4, 2, 4}, {0, 3, 6, 2, 5, 1}, {0, 4, 2, 5, 7, 9}
                    };
                    uint8_t deg = ((r & 3u) != 0u) ? PROG[sc][D.ev_prog_step & 3u] : (uint8_t)((D.ev_last_deg + 2u + ((r >> 6) % 4u)) % 7u);
                    uint8_t col = (uint8_t)((r >> 10) % 6u);
                    D.ev_prog_step++;
                    D.ev_last_deg = deg;
                    for (v = 0; v < HL_NDRONE; v++) {
                        static const int BASE_OCT[6] = {36, 43, 48, 55, 60, 67};
                        uint8_t sd = (uint8_t)((deg + VOICE_COL[col][v]) % 7u);
                        uint8_t semi = HL_SCALES[sc][sd];
                        float target = (float)(BASE_OCT[v] + (int)S.root + (int)semi);
                        if (v >= 2u) {
                            while (target - S.drone[v].note > 7.0f && target > 45.0f) target -= 12.0f;
                            while (S.drone[v].note - target > 7.0f && target < 79.0f) target += 12.0f;
                        }
                        S.drone[v].note = br_clampf(target, 26.0f, 82.0f);
                    }
                    for (k = 0; k < HL_NMEL; k++) {
                        uint32_t stp = (r >> (k * 5u)) % HL_NSTEPS;
                        uint8_t mdeg = (uint8_t)((deg + VOICE_COL[col][(stp + k * 2u) % 6u]) % 7u);
                        S.mel[k].steps[stp] = (uint8_t)((mdeg + (k ? 5u : 0u)) % HL_NPLATES);
                    }
                }
            }
        }

        /* ---- B. ENTROPY RARITY-WEIGHTED TIMBRAL EVENTS (COMMON / UNCOMMON / RARE / STORM) ---- */
        D.ev_timer += dt_block;
        if (D.ev_timer >= (S.evolve == 3u ? 2.6f : S.evolve == 2u ? 4.4f : 7.0f)) {
            uint32_t r = br_rand(&D.rng[0]);
            uint32_t roll = r % 1000u;
            uint32_t w_com = (S.evolve == 1u) ? 860u : (S.evolve == 2u) ? 580u : 360u;
            uint32_t w_unc = (S.evolve == 1u) ? 980u : (S.evolve == 2u) ? 870u : 690u;
            uint32_t w_rar = (S.evolve == 1u) ? 1000u : (S.evolve == 2u) ? 985u : 920u;
            float span = (S.evolve == 3u) ? 0.34f : (S.evolve == 2u) ? 0.22f : 0.13f;
            D.ev_timer = 0.0f;

            if (roll < w_com) {
                /* COMMON: Organic breathing around baseline (Ultra-Warm ceiling on P17..P20 & all CALM patches!) */
                uint32_t dv = (r >> 10) % HL_NDRONE;
                int is_sub_bliss = (S.patch >= 16u && S.patch <= 19u);
                float max_cut = is_sub_bliss ? 0.32f : warm_guard ? 0.46f : 0.88f;
                float min_cut = is_sub_bliss ? 0.16f : warm_guard ? 0.20f : 0.14f;
                float max_res = is_sub_bliss ? 0.24f : warm_guard ? 0.36f : 0.82f;
                float max_mrp = warm_guard ? 0.48f : 1.00f;
                float max_spr = warm_guard ? 0.34f : 0.82f;
                D.ev_tgt_cut[0] = br_clampf(hl_scene_a.vcf_cut[0] + br_randf(&D.rng[0]) * span, min_cut, max_cut);
                D.ev_tgt_cut[1] = br_clampf(hl_scene_a.vcf_cut[1] + br_randf(&D.rng[0]) * span, min_cut, max_cut);
                D.ev_tgt_res[0] = br_clampf(hl_scene_a.vcf_res[0] + br_randf(&D.rng[0]) * (span * 0.6f), 0.05f, max_res);
                D.ev_tgt_res[1] = br_clampf(hl_scene_a.vcf_res[1] + br_randf(&D.rng[0]) * (span * 0.6f), 0.05f, max_res);
                D.ev_tgt_morph[dv]  = br_clampf(D.ev_tgt_morph[dv] + br_randf(&D.rng[0]) * (span * 1.2f), 0.05f, max_mrp);
                D.ev_tgt_spread[dv] = br_clampf(hl_scene_a.d_spread[dv] + br_randf(&D.rng[0]) * span, 0.08f, max_spr);
                D.ev_tgt_spr  = br_clampf(hl_scene_a.spring_mix + br_randf(&D.rng[0]) * (span * 0.6f), 0.05f, warm_guard ? 0.42f : 0.72f);
                D.ev_tgt_shim = br_clampf(hl_scene_a.cart_shimmer + br_randf(&D.rng[0]) * span, 0.08f, 0.85f);
            } else if (roll < w_unc) {
                /* UNCOMMON: Deeper filter/timbre shifts, Melodic Wave/Seq mutations, Breathing Rests */
                uint32_t act = (r >> 12) % 5u;
                if (act == 0u) {
                    if (warm_guard) {
                        /* Warmth Guard: stay strictly in silky LP 12dB with gentle drive */
                        S.vcf_mode[0] = S.vcf_mode[1] = 0u;
                        D.ev_tgt_drive = br_clampf(hl_scene_a.vcf_drive + br_randf(&D.rng[0]) * 0.08f, 0.04f, 0.22f);
                    } else {
                        S.vcf_link = 0u;
                        S.vcf_mode[(r >> 16) & 1u] = (uint8_t)(((r >> 18) & 1u) ? 3u : 0u);
                        D.ev_tgt_drive = br_clampf(hl_scene_a.vcf_drive + br_randf(&D.rng[0]) * 0.30f, 0.05f, 0.82f);
                    }
                } else if (act == 1u) {
                    static const uint8_t WARM_W[4] = {0, 2, 3, 5}; /* SAW+SUB, FOLD TRI, FM BELL, HARMONIC */
                    unsigned mi = (r >> 16) & 1u;
                    S.mel[mi].wave    = warm_guard ? WARM_W[(r >> 18) & 3u] : (uint8_t)((r >> 18) % 6u);
                    S.mel[mi].seq_div = warm_guard ? (uint8_t)((r >> 21) % 3u) : (uint8_t)((r >> 21) % 7u);
                    S.mel[mi].seq_len = (uint8_t)(3u + ((r >> 24) % 3u));
                } else if (act == 2u && S.perf_mode == HL_PERF_GEN && D.ev_mel_rest_t <= 0.0f) {
                    /* Entropy Melodic Breathing Rest (6..12 s so the pure 30-saw drone shines!) */
                    unsigned mi = (r >> 16) & 1u;
                    S.mel[mi].hold = 0u;
                    D.ev_mel_rest_t = 6.0f + (float)((r >> 18) % 7u);
                } else if (act == 3u) {
                    if (warm_guard) {
                        /* Warmth Guard: lush chorus & shimmer bloom instead of loud spring kick */
                        D.ev_tgt_chor = br_clampf(hl_scene_a.chorus_mix + 0.14f, 0.45f, 0.88f);
                        D.ev_tgt_cmix = br_clampf(hl_scene_a.cart_mix + 0.12f, 0.45f, 0.88f);
                    } else {
                        helios_kick_spring(0.35f + 0.25f * (float)((r >> 16) & 1u));
                        D.ev_tgt_cmix = br_clampf(hl_scene_a.cart_mix + 0.18f, 0.20f, 0.90f);
                    }
                } else {
                    float wm = warm_guard
                        ? (0.14f + (float)((r >> 16) % 34u) * 0.01f) /* 0.14..0.47 warm SAW <-> TRI */
                        : ((float)((r >> 16) % 100u) * 0.01f);
                    for (v = 0; v < HL_NDRONE; v++) D.ev_tgt_morph[v] = wm;
                }
            } else if (roll < w_rar) {
                /* RARE: Cartridge Rotation, Scale/Mode Shift, 8x8 Pin Matrix Template, or Gentle BPM Drift */
                uint32_t act = (r >> 12) % 4u;
                if (act == 0u) {
                    static const uint8_t WARM_CARTS[5] = {0, 1, 2, 3, 9}; /* SHIMMER, ECHO, BLACKHOLE, GRANULAR, CHOR+PITCH */
                    S.cart_type = warm_guard
                        ? WARM_CARTS[(r >> 16) % 5u]
                        : (uint8_t)((r >> 16) % ((S.evolve == 3u) ? 11u : 7u));
                } else if (act == 1u) {
                    if (!warm_guard) hl_apply_pin_preset((uint8_t)((r >> 16) % 5u));
                } else if (act == 2u && !S.ext_clk_lock) {
                    int nb = (int)S.bpm + (((r >> 16) & 1u) ? +3 : -3);
                    int lo = warm_guard ? 48 : 42, hi = warm_guard ? 78 : 128;
                    if (nb < lo) nb = lo;
                    if (nb > hi) nb = hi;
                    S.bpm = (uint8_t)nb;
                } else if (S.perf_mode == HL_PERF_GEN) {
                    static const uint8_t WARM_SC[3] = {6, 2, 0}; /* MAJOR, DORIAN, MINOR — never tritone/horror! */
                    S.scale = warm_guard ? WARM_SC[(r >> 16) % 3u] : (uint8_t)((r >> 16) % 7u);
                }
            } else {
                /* STORM (WILD / rare CINEMA): 6-second Infinite Cartridge FREEZE pad while drones shift underneath! */
                if (!S.cart_freeze) {
                    S.cart_freeze = 1u;
                    D.ev_freeze_t = 5.5f + (float)((r >> 16) % 4u);
                }
                if (warm_guard) {
                    D.ev_tgt_cut[0] = 0.28f + (float)((r >> 12) % 24u) * 0.01f;
                    D.ev_tgt_cut[1] = 0.28f + (float)((r >> 18) % 24u) * 0.01f;
                } else {
                    D.ev_tgt_cut[0] = 0.25f + (float)((r >> 12) % 55u) * 0.01f;
                    D.ev_tgt_cut[1] = 0.25f + (float)((r >> 18) % 55u) * 0.01f;
                    helios_kick_spring(0.65f);
                }
            }
        }
    }

    /* 2. Evaluate 8 Modular CV Sources & Route Through the 8x8 Gold Pin Matrix */
    {
        float lfo_mul = S.lfo_fast ? 16.0f : 1.0f;
        D.vcf_lfo_ph += (0.08f + 0.55f * S.vcf_lfo) * lfo_mul * dt_block;
        while (D.vcf_lfo_ph >= 1.0f) D.vcf_lfo_ph -= 1.0f;
        D.lfo2_ph += (0.22f + 2.8f * S.sh_rate) * lfo_mul * dt_block;
        while (D.lfo2_ph >= 1.0f) D.lfo2_ph -= 1.0f;
    }

    {
        float sh_hz = 0.35f + 14.0f * S.sh_rate * S.sh_rate;
        D.sh_ph += sh_hz * dt_block;
        if (D.sh_ph >= 1.0f) {
            D.sh_ph -= 1.0f;
            D.sh_val = br_randf(&D.rng[0]);
        }
        float slew_tc = 0.004f + 0.65f * S.slew_amt * S.slew_amt;
        float ks = 1.0f - fm_expf(-dt_block / slew_tc);
        D.slew_val += (D.sh_val - D.slew_val) * ks;
    }

    if (S.ext_comp && !D.prev_comp) {
        S.mel[0].press = br_clampf(S.mel[0].press + 0.40f, 0.0f, 1.0f);
        S.mel[1].press = br_clampf(S.mel[1].press + 0.40f, 0.0f, 1.0f);
    }
    D.prev_comp = S.ext_comp;

    S.mel[0].press = 0.25f + (S.mel[0].press - 0.25f) * fm_expf(-dt_block * 1.4f);
    S.mel[1].press = 0.25f + (S.mel[1].press - 0.25f) * fm_expf(-dt_block * 1.4f);

    /* External MIDI Clock lock auto-timeout (~1.5s without 0xF8 pulses) */
    if (S.ext_clk_lock) {
        if (++D.clk_age_blocks > 260u) {
            S.ext_clk_lock = 0u;
            D.clk_last_ms = 0u;
        }
    }

    /* Compute peak Polyphonic Aftertouch across all 27 keys + MIDI Channel AT & decay Velocity Pluck */
    {
        float pk_at = D.chan_at;
        for (k = 0; k < HL_NKEYS; k++) {
            if ((S.held_mask & (1u << k)) && S.key_at[k] > 0u) {
                float a = (float)S.key_at[k] * (1.0f / 127.0f);
                if (a > pk_at) pk_at = a;
            }
        }
        S.max_at += (pk_at - S.max_at) * (1.0f - fm_expf(-dt_block * 18.0f));
        S.vcf_pluck_env *= fm_expf(-dt_block * 7.5f);
        D.xmod_pluck    *= fm_expf(-dt_block * 11.0f);
    }

    S.mod_src_val[HL_SRC_LFO1]   = fm_sin_turns(D.vcf_lfo_ph);
    S.mod_src_val[HL_SRC_LFO2]   = fm_sin_turns(D.lfo2_ph);
    S.mod_src_val[HL_SRC_SH]     = D.sh_val;
    S.mod_src_val[HL_SRC_SLEW]   = D.slew_val;
    S.mod_src_val[HL_SRC_ENVFOL] = S.ext_env * 2.0f - 0.3f;
    S.mod_src_val[HL_SRC_JOYX]   = S.joy_x;
    S.mod_src_val[HL_SRC_JOYY]   = S.joy_y;
    {
        float mp = (S.mel[0].press > S.mel[1].press ? S.mel[0].press : S.mel[1].press);
        if (S.max_at > mp) mp = S.max_at;
        S.mod_src_val[HL_SRC_PRESS] = mp;
    }

    for (dst = 0; dst < HL_NMOD_DST; dst++) {
        float acc = 0.0f;
        for (src = 0; src < HL_NMOD_SRC; src++) {
            int8_t pin = S.pin_matrix[src][dst];
            if (pin != 0) {
                float w = (pin == 1) ? 0.50f : (pin == 2) ? 1.00f : -0.75f;
                acc += S.mod_src_val[src] * w;
            }
        }
        mod_dst[dst] = br_clampf(acc, -1.5f, 1.5f);
    }

    /* 3A. Expressive Multi-Mode Arpeggiator (HYBRID Right-Hand or PLAY 8V Full-Keyboard) */
    if (S.arp_mode > 0u && S.perf_mode != HL_PERF_GEN) {
        static const float ARP_DIV[6] = {1.0f, 2.0f, 3.0f, 4.0f, 2.0f, 3.0f};
        uint32_t arp_mask = (S.perf_mode == HL_PERF_HYBRID) ? (S.latch_mask & 0x07FFF000u) : S.latch_mask;
        uint8_t arp_keys[HL_NKEYS];
        uint32_t n_arp = 0u;
        for (k = 0; k < HL_NKEYS; k++) {
            if (arp_mask & (1u << k))
                arp_keys[n_arp++] = (uint8_t)k;
        }
        if (n_arp > 0u) {
            float bps = (float)(S.bpm < 30u ? 30u : S.bpm) * (1.0f / 60.0f);
            float rate = bps * ARP_DIV[S.mel[0].seq_div % 6u];
            /* Polyphonic aftertouch slightly accelerates & opens gate duty cycle */
            float gate_duty = br_clampf(0.58f + 0.34f * S.max_at * S.at_sens, 0.35f, 0.95f);
            D.arp_ph += rate * dt_block;
            if (D.arp_ph >= 1.0f) {
                uint8_t ak;
                float avel, aat;
                D.arp_ph -= 1.0f;
                if (S.arp_mode == HL_ARP_UP) {
                    D.arp_idx = (int8_t)(((int)D.arp_idx + 1) % (int)n_arp);
                } else if (S.arp_mode == HL_ARP_DOWN) {
                    D.arp_idx = (int8_t)(((int)D.arp_idx + (int)n_arp - 1) % (int)n_arp);
                } else if (S.arp_mode == HL_ARP_UPDN) {
                    if (n_arp <= 1u) {
                        D.arp_idx = 0;
                    } else {
                        int nxt = (int)D.arp_idx + (int)D.arp_dir;
                        if (nxt >= (int)n_arp) { nxt = (int)n_arp - 2; D.arp_dir = -1; }
                        else if (nxt < 0)      { nxt = 1; D.arp_dir = 1; }
                        D.arp_idx = (int8_t)nxt;
                    }
                } else if (S.arp_mode == HL_ARP_RND) {
                    D.arp_idx = (int8_t)(br_rand(&D.rng[0]) % n_arp);
                } else {
                    /* STRUM CHORD: cycle rapidly across held chord notes */
                    D.arp_idx = (int8_t)(((int)D.arp_idx + 1) % (int)n_arp);
                }
                ak   = arp_keys[(unsigned)D.arp_idx % n_arp];
                avel = S.key_vel[ak] ? (float)S.key_vel[ak] * (1.0f / 127.0f) : 0.82f;
                aat  = (float)S.key_at[ak] * (1.0f / 127.0f);
                S.vcf_pluck_env = br_clampf(S.vcf_pluck_env + avel * S.vel_sens * 0.65f, 0.0f, 1.0f);
                if (S.perf_mode == HL_PERF_PLAY) {
                    hl_play8v_note_on(ak, avel, aat);
                } else {
                    float nm = hl_key_to_midi(ak);
                    for (v = 0; v < HL_NMEL; v++) {
                        S.mel[v].cur_plate = (uint8_t)((ak - 12u) % HL_NPLATES);
                        S.mel[v].cur_note  = nm + (float)S.mel[v].octave * 12.0f;
                        S.mel[v].hold      = 1u;
                        D.m_key[v]         = (int8_t)ak;
                        D.m_gate[v]        = 1u;
                        D.m_vel[v]         = avel;
                        D.m_at[v]          = aat;
                    }
                }
            }
            D.arp_gate_on = (D.arp_ph < gate_duty) ? 1u : 0u;
        } else {
            D.arp_gate_on = 0u;
        }
    }

    /* 3B. Advance Polymetric Dual 5-Step Analog Sequencers (GEN mode) or Direct Playable Envelopes (HYBRID / PLAY 8V) */
    {
        static const float DIV_MUL[7] = {0.25f, 0.5f, 1.0f, 2.0f, 1.0f, 0.75f, 1.25f};
        float bps = (float)(S.bpm < 30u ? 30u : S.bpm) * (1.0f / 60.0f);
        for (v = 0; v < HL_NMEL; v++) {
            hl_mel_cfg_t *m = &S.mel[v];
            uint8_t gate_on = m->hold;
            uint8_t slen = (m->seq_len >= 2u && m->seq_len <= HL_NSTEPS) ? m->seq_len : HL_NSTEPS;
            float tgt_note;
            if (S.perf_mode == HL_PERF_GEN && m->seq_run) {
                float rate = bps * DIV_MUL[m->seq_div % 7u];
                D.m_seq_ph[v] += rate * dt_block;
                if (D.m_seq_ph[v] >= 1.0f) {
                    D.m_seq_ph[v] -= 1.0f;
                    if (m->seq_div == 6u) {
                        /* RANDOM */
                        m->cur_step = (uint8_t)(br_rand(&D.rng[0]) % slen);
                    } else if (m->seq_div == 5u) {
                        /* PENDULUM */
                        int nxt = (int)m->cur_step + (int)D.m_pend_dir[v];
                        if (nxt >= (int)slen) { nxt = (int)slen - 2; D.m_pend_dir[v] = -1; }
                        else if (nxt < 0)     { nxt = 1; D.m_pend_dir[v] = 1; }
                        if (nxt < 0) nxt = 0;
                        m->cur_step = (uint8_t)nxt;
                    } else if (m->seq_div == 4u) {
                        /* REVERSE */
                        m->cur_step = (uint8_t)((m->cur_step + slen - 1u) % slen);
                    } else {
                        /* FORWARD */
                        m->cur_step = (uint8_t)((m->cur_step + 1u) % slen);
                    }
                    {
                        uint8_t stp = m->cur_step % HL_NSTEPS;
                        uint8_t pr  = m->step_prob[stp];
                        if (pr == 0u) pr = 10u;
                        uint8_t fire = m->step_gate[stp] && ((br_rand(&D.rng[0]) % 10u) < pr);
                        if (fire) {
                            m->cur_plate = m->steps[stp] % HL_NPLATES;
                            m->press = m->step_acc[stp] ? 1.0f : 0.78f;
                            if (m->step_acc[stp])
                                S.vcf_pluck_env = br_clampf(S.vcf_pluck_env + 0.55f, 0.0f, 1.0f);
                        } else {
                            /* GATE OFF / Rest: release envelope cleanly without gliding to the muted step's pitch */
                            m->press = 0.15f;
                        }
                    }
                }
                gate_on = m->hold && (m->step_gate[m->cur_step % HL_NSTEPS] != 0u) && (m->press > 0.20f);
            } else if (S.perf_mode != HL_PERF_GEN) {
                gate_on = D.m_gate[v] && m->hold;
                if (S.arp_mode > 0u) gate_on = gate_on && D.arp_gate_on;
            }
            {
                /* Velocity speeds up attack snap in HYBRID/PLAY mode */
                float atk_t = (S.perf_mode == HL_PERF_GEN)
                    ? (0.01f + 1.4f * m->attack)
                    : (0.004f + 0.35f * m->attack * (1.15f - 0.65f * D.m_vel[v]));
                float rel_t = (S.perf_mode == HL_PERF_GEN)
                    ? (0.01f + 1.4f * m->release)
                    : (0.015f + 0.85f * m->release);
                float k_env = 1.0f - fm_expf(-dt_block / (gate_on ? atk_t : rel_t));
                float vel_peak = (S.perf_mode == HL_PERF_GEN)
                    ? 1.0f
                    : br_clampf((1.0f - 0.55f * S.vel_sens) + 0.65f * S.vel_sens * D.m_vel[v] + 0.25f * S.at_sens * D.m_at[v], 0.2f, 1.35f);
                m->env += ((gate_on ? vel_peak : 0.0f) - m->env) * k_env;

                if (S.perf_mode != HL_PERF_GEN && D.m_key[v] >= 0) {
                    if (S.perf_mode == HL_PERF_HYBRID && S.key_mode == 1u) {
                        /* Scale-quantized right hand in DUAL 12 mode */
                        tgt_note = hl_plate_to_note(S.root, S.scale, m->cur_plate, (int8_t)(m->octave + S.play_oct));
                    } else {
                        /* Full 27-key chromatic in SPLIT 6+6 and POLY 2V */
                        tgt_note = hl_key_to_midi((unsigned)D.m_key[v]) + (float)m->octave * 12.0f;
                    }
                } else {
                    tgt_note = hl_plate_to_note(S.root, S.scale, m->cur_plate, m->octave);
                }
                float g_time = 0.003f + (S.perf_mode == HL_PERF_PLAY ? 0.25f : 1.1f) * m->glide * m->glide;
                float kg = 1.0f - fm_expf(-dt_block / g_time);
                m->cur_note += (tgt_note - m->cur_note) * kg;
            }
        }
    }

    /* 4. Prepare Block Increments for Core 0 (Left Bank V1..V3 + V4) & Core 1 (Right Bank V6..V8 + V5) */
    B.n = n;
    {
        float vca_mod = br_clampf(1.0f + 0.35f * mod_dst[HL_DST_VCA], 0.15f, 1.45f);
        uint32_t active_drn = 0u;
        float drn_base_gain;
        for (v = 0; v < HL_NDRONE; v++)
            if (S.drone[v].hold || S.drone[v].env > 0.02f) active_drn++;
        /* Solo/duo drones play full & loud (+5.5 dB vs old 0.16); gentle headroom scaling as more stack */
        drn_base_gain = (S.perf_mode == HL_PERF_PLAY)
            ? 0.24f
            : (0.31f / (1.0f + 0.14f * (float)(active_drn > 1u ? active_drn - 1u : 0u)));
        for (v = 0; v < HL_NDRONE; v++) {
            hl_drone_cfg_t *d = &S.drone[v];
            float drn_cv = (v < 3u) ? mod_dst[HL_DST_DRNL] : mod_dst[HL_DST_DRNR];
            uint8_t d_on = d->hold;
            float tc, ke, base_hz, sp_eff, g_v, vel_scale = 1.0f, at_v = D.d_at[v] * S.at_sens;
            if (S.perf_mode == HL_PERF_PLAY) {
                d_on = D.d_gate[v] && d->hold;
                if (S.arp_mode > 0u && S.arp_mode != HL_ARP_CHORD) d_on = d_on && D.arp_gate_on;
                tc = d_on ? (0.004f + 0.45f * d->attack * (1.15f - 0.65f * D.d_vel[v]))
                          : (0.020f + 0.90f * d->release);
                vel_scale = br_clampf((1.0f - 0.55f * S.vel_sens) + 0.65f * S.vel_sens * D.d_vel[v] + 0.30f * at_v, 0.15f, 1.35f);
            } else {
                tc = d_on ? (0.015f + 4.0f * d->attack * d->attack) : (0.04f + 6.0f * d->release * d->release);
                vel_scale = 1.0f + 0.25f * at_v;
            }
            ke = 1.0f - fm_expf(-dt_block / tc);
            /* Per-drone adjustable glide: 0% = instant hardware pitch (~12ms anti-click), 100% = 3.0s tape glide */
            if (S.perf_mode == HL_PERF_PLAY || D.d_cur_note[v] < 12.0f) {
                D.d_cur_note[v] = d->note;
            } else if (d->glide < 0.01f) {
                float kn = 1.0f - fm_expf(-dt_block * 80.0f);
                D.d_cur_note[v] += (d->note - D.d_cur_note[v]) * kn;
            } else {
                float g_t = 0.02f + 2.95f * d->glide * d->glide;
                float kn = 1.0f - fm_expf(-dt_block / g_t);
                D.d_cur_note[v] += (d->note - D.d_cur_note[v]) * kn;
            }
            /* Per-voice Polyphonic Aftertouch adds organic pitch vibrato & detune spread bloom! */
            float at_vib = (at_v > 0.02f) ? (fm_sin_turns(D.m_vib_ph[v & 1u] + (float)v * 0.17f) * at_v * 0.18f) : 0.0f;
            float pb_semi = (S.perf_mode == HL_PERF_PLAY) ? (S.pitch_bend * 2.0f) : 0.0f;
            base_hz = 440.0f * fm_exp2f((D.d_cur_note[v] + drn_cv * 0.45f + at_vib + pb_semi - 69.0f) * (1.0f / 12.0f));
            /* In PLAY 8V mode, keep chord spread musical (0..0.48) unless pushed into swarm by aftertouch */
            float base_sp = (S.perf_mode == HL_PERF_PLAY) ? (d->spread * 0.45f) : d->spread;
            sp_eff = br_clampf(base_sp + 0.18f * drn_cv + 0.36f * at_v + 0.12f * D.d_pluck[v] * S.vel_sens, 0.0f, 1.0f);
            D.d_pluck[v] *= fm_expf(-dt_block * 9.0f);

            d->env += ((d_on ? vel_scale : 0.0f) - d->env) * ke;
            g_v = d->env * d->level * drn_base_gain * vca_mod;
            B.d_gain_l[v] = (d->pan <= 0) ? g_v : g_v * 0.25f;
            B.d_gain_r[v] = (d->pan >= 0) ? g_v : g_v * 0.25f;

            for (k = 0; k < HL_NSAW; k++) {
                D.d_drift[v][k] += 0.02f * dt_block * (br_randf(&D.rng[0]) * 0.0035f - D.d_drift[v][k]);
                float r = hl_saw_ratio(k, sp_eff, d->sub) * (1.0f + D.d_drift[v][k]);
                B.d_inc[v][k] = br_clampf(base_hz * r * hl_inv_fs, 0.0002f, 0.42f);
            }
        }
        for (k = 0; k < 2u; k++) {
            float lfo_w = fm_sin_turns(D.vcf_lfo_ph + (k ? 0.25f : 0.0f)) * S.vcf_lfo * 0.18f;
            float pin_cv = mod_dst[k == 0u ? HL_DST_VCFL : HL_DST_VCFR] * 0.28f;
            /* Velocity Pluck Envelope + Polyphonic Aftertouch open the Dual Russian Analog VCFs! */
            float vel_open = S.vcf_pluck_env * S.vel_sens * 0.34f;
            float at_open  = S.max_at * S.at_sens * 0.38f;
            float c_norm = br_clampf(S.vcf_cut[k] + lfo_w + pin_cv + vel_open + at_open, 0.03f, 0.96f);
            float fc = 38.0f * fm_powf(280.0f, c_norm);
            fc = br_clampf(fc, 35.0f, 12500.0f);
            B.g_cut[k] = 2.0f * fm_sinf(3.14159265f * fc * hl_inv_fs);
            B.g_cut[k] = br_clampf(B.g_cut[k], 0.002f, 0.78f);
            B.res_eff[k] = br_clampf(S.vcf_res[k] + 0.15f * fm_fabsf(pin_cv) + 0.16f * S.max_at * S.at_sens, 0.0f, 0.96f);
        }
        for (v = 0; v < HL_NMEL; v++) {
            hl_mel_cfg_t *m = &S.mel[v];
            float vo_cv = mod_dst[v == 0u ? HL_DST_V4VO : HL_DST_V5VO] * 3.5f;
            float at_v  = D.m_at[v] * S.at_sens;
            D.m_vib_ph[v] += (4.8f + 0.6f * (float)v + 1.8f * at_v) * dt_block;
            if (D.m_vib_ph[v] >= 1.0f) D.m_vib_ph[v] -= 1.0f;
            float vib_semi = fm_sin_turns(D.m_vib_ph[v]) * (m->vibrato + 0.25f * m->press + 0.45f * at_v) * 0.25f;
            float hz = 440.0f * fm_exp2f((m->cur_note + vib_semi + vo_cv + S.pitch_bend * 2.0f - 69.0f) * (1.0f / 12.0f));
            B.m_inc[v] = br_clampf(hz * hl_inv_fs, 0.0004f, 0.42f);
            B.m_g[v] = m->env * m->level * 0.24f * vca_mod;
        }
        {
            float cart_cv = mod_dst[HL_DST_CART] * 0.25f + S.max_at * S.at_sens * 0.22f;
            B.sh_eff = br_clampf(S.cart_shimmer + cart_cv, 0.0f, 1.0f);
            B.fb_rev = 0.72f + 0.23f * br_clampf(S.cart_decay + 0.5f * cart_cv, 0.0f, 1.0f);
            B.k_rtone = 0.08f + 0.65f * S.cart_tone;
            B.dly_len_l = (uint32_t)((0.09f + 0.24f * S.cart_decay) * hl_fs) & 16383u;
            B.dly_len_r = (uint32_t)((0.14f + 0.31f * S.cart_decay) * hl_fs) & 16383u;
            if (B.dly_len_l < 128u) B.dly_len_l = 128u;
            if (B.dly_len_r < 128u) B.dly_len_r = 128u;
        }
    }

    /* 5. STAGE A: Dual-Core Parallel Synthesis of Left Bank (Core 0) & Right Bank (Core 1) */
    {
        int c2_fx_pending = 0;
        if (hl_cs_mode && hl_c2_run && hl_c2_wait) {
            if (hl_c2_run(hl_c2_tramp, 2u)) {
                hl_v_jobs++;
                hl_render_bank(0u, c0_flt_l, n);
                if (hl_c2_wait() < 0) {
                    hl_cs_mode = 0;
                    hl_cs_have = 0;
                    hl_cs_lost++;
                    hl_render_bank(1u, c2_flt_r, n);
                }
            } else {
                hl_render_bank(0u, c0_flt_l, n);
                hl_render_bank(1u, c2_flt_r, n);
            }
        } else if (hl_cs_mode && hl_cs_idle()) {
            hl_v_jobs++;
            __asm__ volatile("" ::: "memory");
            hl_cs_go = 2u; /* Dispatch Right Bank (V6..V8 + V5 + Analog R) to Core 1 */
            if (hl_cs_mode == 2)
                helios_core2_work();
            /* Simultaneously render Left Bank (V1..V3 + V4 + Analog L) on Core 0 */
            hl_render_bank(0u, c0_flt_l, n);
            if (!hl_cs_idle()) {
                /* Fallback if Core 1 ever timed out */
                hl_render_bank(1u, c2_flt_r, n);
            }
        } else {
            hl_render_bank(0u, c0_flt_l, n);
            hl_render_bank(1u, c2_flt_r, n);
        }

        /* 6. STAGE B: Pipelined Dual-Core BBD Ensemble Chorus + 8-Line FDN Cartridge Multi-FX on Core 1 */
        if (hl_cs_mode && hl_c2_run && hl_c2_wait) {
            for (i = 0; i < n; i++) {
                c2_in_l[i] = c0_flt_l[i];
                c2_in_r[i] = c2_flt_r[i];
                if (hl_cs_have) {
                    c0_flt_l[i] = c2_wet_l[i];
                    c2_flt_r[i] = c2_wet_r[i];
                }
            }
            if (hl_c2_run(hl_c2_tramp, 1u)) {
                hl_cs_have = 1u;
                hl_fx_jobs++;
                c2_fx_pending = 1;
            } else {
                hl_cs_have = 0u;
                hl_render_fx_core(c2_in_l, c2_in_r, c0_flt_l, c2_flt_r, n);
            }
        } else if (hl_cs_mode && hl_cs_idle()) {
            for (i = 0; i < n; i++) {
                c2_in_l[i] = c0_flt_l[i];
                c2_in_r[i] = c2_flt_r[i];
                if (hl_cs_have) {
                    c0_flt_l[i] = c2_wet_l[i];
                    c2_flt_r[i] = c2_wet_r[i];
                }
            }
            hl_cs_have = 1u;
            hl_fx_jobs++;
            __asm__ volatile("" ::: "memory");
            hl_cs_go = 1u; /* Core 1 crunches BBD Chorus + 8-Line FDN Cartridge FX in parallel with Stage 7 & UI! */
            if (hl_cs_mode == 2) {
                helios_core2_work();
                for (i = 0; i < n; i++) {
                    c0_flt_l[i] = c2_wet_l[i];
                    c2_flt_r[i] = c2_wet_r[i];
                }
            }
        } else {
            hl_cs_have = 0u;
            hl_render_fx_core(c0_flt_l, c2_flt_r, c0_flt_l, c2_flt_r, n);
        }

        /* 7. Core 0 Studio Master Stage: Pultec Sub-Bass + Sub-Tightener HPF + Mastering Tilt EQ + Tape Warmth + 16-Band Spectrum + Limiter */
        {
            /* 16-band logarithmic SVF band-pass frequencies (70 Hz .. 11.5 kHz, sub-sampled 1:4 = 11.025 kHz) */
            static const float SPEC_G[16] = {
                0.038f, 0.052f, 0.071f, 0.096f, 0.130f, 0.174f, 0.232f, 0.306f,
                0.398f, 0.510f, 0.642f, 0.790f, 0.945f, 1.110f, 1.280f, 1.440f
            };
            float hpf_a = (S.master_hpf > 0.005f) ? (0.996f - 0.022f * S.master_hpf) : 0.996f;
            float tilt  = S.master_tilt;
            float tape  = S.master_tape;
            float min_lim_g = 1.0f;
            float spec_pk[16];
            for (k = 0; k < 16u; k++) spec_pk[k] = 0.0f;

            for (i = 0; i < n; i++) {
                float flt_l = c0_flt_l[i], flt_r = c2_flt_r[i];
                float dcl = flt_l - D.dc_xl + hpf_a * D.dc_yl; D.dc_xl = flt_l; D.dc_yl = fm_flush(dcl);
                float dcr = flt_r - D.dc_xr + hpf_a * D.dc_yr; D.dc_xr = flt_r; D.dc_yr = fm_flush(dcr);
                D.xf_lo_l += 0.023f * (dcl - D.xf_lo_l);
                D.xf_lo_r += 0.023f * (dcr - D.xf_lo_r);
                float sub_m = (D.xf_lo_l + D.xf_lo_r) * 0.5f;
                float xsat = 0.38f * br_soft((sub_m + 0.10f * sub_m * sub_m) * 1.8f);
                float out_l = dcl + 0.48f * sub_m + xsat;
                float out_r = dcr + 0.48f * sub_m + xsat;

                /* Mastering Tilt EQ (~900 Hz pivot: negative = warm dark, positive = airy bright) */
                if (tilt > 0.01f || tilt < -0.01f) {
                    D.tilt_l += 0.12f * (out_l - D.tilt_l);
                    D.tilt_r += 0.12f * (out_r - D.tilt_r);
                    float hi_l = out_l - D.tilt_l;
                    float hi_r = out_r - D.tilt_r;
                    out_l += tilt * (0.45f * hi_l - 0.32f * D.tilt_l);
                    out_r += tilt * (0.45f * hi_r - 0.32f * D.tilt_r);
                }

                /* Analog Tape Warmth / Soft-Clip Saturation */
                if (tape > 0.005f) {
                    float drv = 1.0f + 2.2f * tape;
                    float tl = br_soft((out_l + 0.06f * out_l * out_l) * drv) / (1.0f + 0.45f * tape);
                    float tr = br_soft((out_r + 0.06f * out_r * out_r) * drv) / (1.0f + 0.45f * tape);
                    out_l = out_l * (1.0f - tape) + tl * tape;
                    out_r = out_r * (1.0f - tape) + tr * tape;
                }

                float abs_pk = fm_fabsf(out_l) > fm_fabsf(out_r) ? fm_fabsf(out_l) : fm_fabsf(out_r);
                if (abs_pk > D.lim_env) D.lim_env += 0.25f * (abs_pk - D.lim_env);
                else                    D.lim_env += 0.0008f * (abs_pk - D.lim_env);
                float lim_g = D.lim_env > 0.85f ? (0.85f / D.lim_env) : 1.0f;
                if (lim_g < min_lim_g) min_lim_g = lim_g;
                out_l = br_soft(out_l * lim_g * 0.95f) * gain;
                out_r = br_soft(out_r * lim_g * 0.95f) * gain;

                if (fm_fabsf(out_l) > pk_l) pk_l = fm_fabsf(out_l);
                if (fm_fabsf(out_r) > pk_r) pk_r = fm_fabsf(out_r);
                if ((i & 3u) == 0u) {
                    float sl = br_soft(dcl * lim_g * 2.2f);
                    float sr = br_soft(dcr * lim_g * 2.2f);
                    S.scope_l[D.scope_w & 63u] = sl;
                    S.scope_r[D.scope_w & 63u] = sr;
                    D.scope_w++;
                    /* Only run 16-Band Spectrum Filterbank when SPECTRUM view is active (1:8 decimated = zero overhead in WAVE/LISSAJOUS!) */
                    if (D.spec_en && (i & 7u) == 0u) {
                        float sm = (sl + sr) * 0.5f;
                        for (k = 0; k < 16u; k++) {
                            float g = SPEC_G[k];
                            float hp = sm - D.spec_lp[k] - 0.55f * D.spec_bp[k];
                            D.spec_bp[k] += g * hp;
                            D.spec_lp[k] += g * D.spec_bp[k];
                            float a = fm_fabsf(D.spec_bp[k]);
                            if (a > spec_pk[k]) spec_pk[k] = a;
                        }
                    }
                }

                int32_t ql = (int32_t)(out_l * 8388607.0f);
                int32_t qr = (int32_t)(out_r * 8388607.0f);
                if (ql > 8388607) ql = 8388607; else if (ql < -8388607) ql = -8388607;
                if (qr > 8388607) qr = 8388607; else if (qr < -8388607) qr = -8388607;
                lr[i * 2u + 0u] = ql;
                lr[i * 2u + 1u] = qr;
            }
            if (D.spec_en) {
                for (k = 0; k < 16u; k++) {
                    float v_band = br_clampf(spec_pk[k] * 1.9f, 0.0f, 1.0f);
                    D.spec_bp[k] = fm_flush(D.spec_bp[k]);
                    D.spec_lp[k] = fm_flush(D.spec_lp[k]);
                    S.spec[k] = (v_band > S.spec[k]) ? (S.spec[k] * 0.35f + v_band * 0.65f) : (S.spec[k] * 0.86f);
                }
            }
            S.gr_db = (min_lim_g < 0.999f) ? br_clampf((1.0f - min_lim_g) * 18.0f, 0.0f, 18.0f) : (S.gr_db * 0.85f);
        }

        /* Synchronize Stage B FX job before leaving app_render so Core 1 is always idle in main loop */
        if (c2_fx_pending && hl_c2_wait) {
            if (hl_c2_wait() < 0) {
                hl_cs_mode = 0;
                hl_cs_have = 0;
                hl_cs_lost++;
            }
        }
    }

    D.xf_lo_l = fm_flush(D.xf_lo_l);
    D.xf_lo_r = fm_flush(D.xf_lo_r);
    D.tilt_l  = fm_flush(D.tilt_l);
    D.tilt_r  = fm_flush(D.tilt_r);
    D.lim_env = fm_flush(D.lim_env);
    D.usb_in_ready = 0u;
    S.vu_l = S.vu_l * 0.78f + pk_l * 0.22f;
    S.vu_r = S.vu_r * 0.78f + pk_r * 0.22f;
    S.vu_peak_l = (pk_l > S.vu_peak_l) ? pk_l : (S.vu_peak_l * 0.985f);
    S.vu_peak_r = (pk_r > S.vu_peak_r) ? pk_r : (S.vu_peak_r * 0.985f);
}
