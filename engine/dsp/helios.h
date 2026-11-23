/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ENTROPY Helios contributors
 *
 * ENTROPY Helios: Standalone Dual-Core Component-Modeled Entropy Helios Polyphonic Ambient Synthesizer
 * & Expressive Velocity / Polyphonic-Aftertouch Performance Instrument for the M-VAVE FM-1
 * (Actions / JieLi AC79 Dual-Core SoC).
 *
 * Dual-Core Asymmetric SMP Architecture (Core 0 + Core 1 via plat_cpu2_job):
 *   - Stage A (s42_cs_go = 2, Parallel Voice & Analog Filter Bank Split):
 *       * Core 0 renders Left Drone/Poly Bank V1..V3 (15 CMOS 40106 relaxation saws) + Melodic Voice V4
 *         + Left Entropy Symbiosis Preamp / Envelope Follower / Comparator + Left Pink/Geiger/Sub Noise
 *         + Left Analog Op-Amp VCF.
 *       * Core 1 renders Right Drone/Poly Bank V6..V8 (15 CMOS 40106 relaxation saws) + Melodic Voice V5
 *         + Right Entropy Symbiosis Preamp + Right Pink/Geiger/Sub Noise
 *         + Right Analog Op-Amp VCF in parallel!
 *   - Stage B (s42_cs_go = 1, Pipelined Dual-Core Ensemble Chorus & 8-Line FDN Cartridge Multi-FX):
 *       * Core 1 processes the Stereo BBD Tri-Phase Analog Ensemble Chorus + Dual-Grain +12st Shimmer /
 *         -12st Sub-Octave Pitch Shifter + Stereo Space Tape Echo + 8-Line Householder FDN Cathedral Hall
 *         + 4-Stage Stereo Krautrock Phaser in parallel while Core 0 runs the Pultec <165 Hz Sub-Bass
 *         Foundation Shelf, Lookahead Soft-Knee Mastering Limiter, and 240x240 RGB565 Hardware Front-End UI.
 */
#pragma once
#include <stdint.h>

#define S42_NDRONE   6u   /* Drone/Poly voices: 0..2 = V1..V3 (Left Core 0), 3..5 = V6..V8 (Right Core 1) */
#define S42_NSAW     5u   /* 5 parallel analog sawtooth oscillators per drone/poly voice (30 total) */
#define S42_NMEL     2u   /* Melodic voices: 0 = V4 (Left Core 0), 1 = V5 (Right Core 1) */
#define S42_NSTEPS   5u   /* 5-step analog sequencer per melodic voice */
#define S42_NPLATES  12u  /* 12 copper capacitive touch plates */
#define S42_NPATCHES 36u  /* 36 curated Entropy Helios factory patches grouped by mood: 20 CALM (P01..P20), 8 LUSH (P21..P28), 8 EDGE (P29..P36) */
#define S42_NUSER    8u   /* 8 permanent writable user patch slots (U01..U08) saved in flash */
#define S42_NTOTAL_P (S42_NPATCHES + S42_NUSER)
#define S42_NCART    16u  /* 16 stackable magnetic stereo FX cartridges (full Entropy Helios + Tape Wash collection) */
#define S42_NMOD_SRC 8u   /* 8 modular CV sources in the 8x8 patch matrix */
#define S42_NMOD_DST 8u   /* 8 modular CV destinations in the 8x8 patch matrix */
#define S42_MAXN     256u /* frames per FM-1 audio block (5.8 ms at 44.1 kHz) */
#define S42_NKEYS    27u  /* 27 velocity & polyphonic-aftertouch keys on M-VAVE FM-1 (F3..G5) */

/* Performance Modes (GEN Drone <-> HYBRID Split <-> PLAY 8-Voice Polyphonic Instrument) */
enum {
    S42_PERF_GEN    = 0,  /* Generative Entropy Helios Drone + Dual 5-Step Sequencers + Touch Plates */
    S42_PERF_HYBRID = 1,  /* Split: Left Hand (F3..E4) Drone Chord Director + Right Hand (F4..G5) Solo V4/V5 Lead */
    S42_PERF_PLAY   = 2   /* 8-Voice Polyphonic 32-Oscillator Velocity & Poly-Aftertouch Keyboard Instrument */
};

/* Expressive Arpeggiator Modes (for HYBRID & PLAY modes) */
enum {
    S42_ARP_OFF   = 0,
    S42_ARP_UP    = 1,
    S42_ARP_DOWN  = 2,
    S42_ARP_UPDN  = 3,
    S42_ARP_RND   = 4,
    S42_ARP_CHORD = 5
};

/* Modular CV Sources (Rows 0..7 in Pin Matrix) */
enum {
    S42_SRC_LFO1   = 0,
    S42_SRC_LFO2   = 1,
    S42_SRC_SH     = 2,
    S42_SRC_SLEW   = 3,
    S42_SRC_ENVFOL = 4,
    S42_SRC_JOYX   = 5,
    S42_SRC_JOYY   = 6,
    S42_SRC_PRESS  = 7
};

/* Modular CV Destinations (Cols 0..7 in Pin Matrix) */
enum {
    S42_DST_DRNL   = 0,
    S42_DST_DRNR   = 1,
    S42_DST_V4VO   = 2,
    S42_DST_V5VO   = 3,
    S42_DST_VCFL   = 4,
    S42_DST_VCFR   = 5,
    S42_DST_VCA    = 6,
    S42_DST_CART   = 7
};

typedef struct {
    float note;           /* Base MIDI pitch (24..84) */
    float spread;         /* 0..1: 5-saw detune -> cluster -> fifth/chord */
    float wave_morph;     /* 0..1: 0=CMOS SAW, 0.33=TRI, 0.66=PULSE, 1.0=GLASS BELL */
    uint8_t sub;          /* 0 = unison, 1 = -1 oct sub-oscillator on center saw */
    uint8_t hold;         /* Capacitive hold latch (1 = sounding) */
    int8_t pan;           /* -1 = Left VCF, 0 = Both, +1 = Right VCF */
    float attack;         /* 0..1 (10 ms .. 12 s) */
    float release;        /* 0..1 (50 ms .. 15 s) */
    float level;          /* 0..1 */
    float env;            /* Current live envelope level (for UI LEDs) */
} s42_drone_cfg_t;

typedef struct {
    uint8_t wave;         /* 0 SAW+SUB, 1 PWM, 2 FOLD TRI, 3 FM BELL, 4 FORMANT, 5 HARMONIC */
    int8_t octave;        /* -2 .. +2 */
    float glide;          /* 0..1 (0 .. 1.8 s portamento) */
    float vibrato;        /* 0..1 */
    float attack;         /* 0..1 */
    float release;        /* 0..1 */
    float level;          /* 0..1 */
    uint8_t hold;         /* 1 = sustained */
    uint8_t seq_run;      /* 0 = manual touch plates, 1 = 5-step sequencer active */
    uint8_t seq_div;      /* clock division: 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=REV, 5=PEND, 6=RND */
    uint8_t seq_len;      /* polymetric step length: 2..5 steps */
    uint8_t steps[S42_NSTEPS];     /* plate index 0..11 for each of the 5 steps */
    uint8_t step_gate[S42_NSTEPS]; /* 1 = gate active, 0 = step muted/rest */
    uint8_t step_prob[S42_NSTEPS]; /* 1..10 = 10%..100% trigger probability per step (default 10 = 100%) */
    uint8_t step_acc[S42_NSTEPS];  /* 0 = normal, 1 = accented step (VCF pluck + louder) */
    uint8_t cur_step;     /* active step 0..4 (for UI LED) */
    uint8_t cur_plate;    /* active touch plate 0..11 */
    float press;          /* capacitive touch plate pressure / aftertouch envelope (0..1) */
    float env;            /* current amplitude envelope (for UI) */
    float cur_note;       /* current gliding MIDI note */
} s42_mel_cfg_t;

typedef struct {
    uint8_t patch;        /* 0 .. S42_NPATCHES - 1 */
    uint8_t playing;      /* 1 = running, 0 = muted */
    uint8_t evolve;       /* 0 = MANUAL, 1 = SLOW DRIFT, 2 = CINEMA, 3 = WILD */
    uint8_t root;         /* 0..11 (C .. B) */
    uint8_t scale;        /* 0 = MINOR, 1 = PHRYGIAN, 2 = DORIAN, 3 = LYDIAN, 4 = FIFTHS, 5 = HARM MIN */
    uint8_t bpm;          /* 30 .. 160 BPM */

    /* Interactive Instrument Mode & Velocity / Polyphonic Aftertouch Expression */
    uint8_t perf_mode;    /* 0 = GEN (Generative Drone), 1 = HYBRID (Split Drone+Lead), 2 = PLAY (8-Voice Poly) */
    uint8_t arp_mode;     /* 0 = OFF, 1 = UP, 2 = DOWN, 3 = UP/DN, 4 = RANDOM, 5 = STRUM CHORD */
    uint8_t sustain_latch;/* 0 = Momentary keys, 1 = Sustain / Chord Hold Latch */
    int8_t  play_oct;     /* -2 .. +2 keyboard octave shift in HYBRID / PLAY modes */
    float   vel_sens;     /* 0..1: Strike velocity -> Analog VCF Pluck + VCA + FM X-MOD bite */
    float   at_sens;      /* 0..1: Polyphonic Aftertouch -> VCF Cutoff/Res + 5-Saw Spread + Vibrato + Shimmer */

    /* 12 Capacitive Copper Touch Plates: Keypad Split Mode & Per-Plate Microtonal Trimpots */
    uint8_t key_mode;     /* 0 = SPLIT 6+6 (Plates 1-6 -> V4, Plates 7-12 -> V5), 1 = DUAL 12, 2 = POLY 2V */
    float plate_trim[S42_NPLATES]; /* -1.0 .. +1.0 semitone (-100..+100 cents) trimpot per copper plate */

    s42_drone_cfg_t drone[S42_NDRONE];
    s42_mel_cfg_t   mel[S42_NMEL];
    float v45_xmod;       /* 0..1: V4 <-> V5 audio-rate cross-FM & hard-sync bite */

    /* Dual Dual Analog VCF (0 = Left Bank, 1 = Right Bank) */
    float vcf_cut[2];     /* 0..1 (35 Hz .. 12 kHz) */
    float vcf_res[2];     /* 0..1 (Analog op-amp feedback growl) */
    uint8_t vcf_mode[2];  /* 0 = LP 12dB, 1 = BP Vocal, 2 = HP, 3 = LP+BP Morph */
    uint8_t vcf_link;     /* 1 = Stereo Linked L/R, 0 = Independent Left & Right Analog VCFs */
    float vcf_drive;      /* 0..1 (K140UD12 slew-rate & diode saturation) */
    float vcf_lfo;        /* 0..1 (slow filter breath modulation) */

    /* Physical-Modeled 3-Spring Analog Reverb Tank + Piezo Contact-Mic Kick */
    float spring_mix;     /* 0..1: Mechanical 3-spring dispersive reverb tank mix */
    float spring_decay;   /* 0..1: Spring tank decay & metallic chirp */
    float spring_kick_env;/* Live spring tank excitation energy (for UI flash) */

    /* 2-Axis Vector Joystick (-1..+1) + 8-Second Motion Recorder */
    float joy_x, joy_y;
    uint8_t joy_rec_state;/* 0 = MANUAL, 1 = REC (recording 8s gesture), 2 = LOOP (playing gesture) */

    /* Built-in Entropy Symbiosis External Preamp Generator + Live USB Audio In + Env Follower + Comparator */
    uint8_t ext_src;      /* 0 = SYMBIOSIS, 1 = USB AUDIO IN, 2 = BOTH */
    float ext_in_lvl;     /* 0..1 (0 = off, >0 feeds Symbiosis / Live USB Audio into Preamp & Analog VCFs) */
    float ext_env;        /* 0..1 live envelope follower level */
    uint8_t ext_comp;     /* 0/1 live comparator gate state */

    /* Studio v2.0: Tuning, Chord Memory, External MIDI Clock, Scene A/B Morph, Master Bus Shaper */
    uint8_t tuning;       /* 0 = 12-TET, 1 = JUST INT, 2 = PYTHAG, 3 = BOHLEN-P, 4 = MAQAM */
    uint8_t chord_mode;   /* 0 = OFF, 1 = 5TH, 2 = MIN7, 3 = MAJ9, 4 = SUS2, 5 = CLUSTER */
    uint8_t ext_clk_lock; /* 1 = locked to external MIDI Clock (0xF8) */
    uint8_t scene_b_set;  /* 1 = Scene B captured for A/B Morphing */
    uint8_t compare_on;   /* 1 = Instant A/B Compare active (auditioning saved Scene A) */
    float   morph_pos;    /* 0..1: Live A/B Scene Morph crossfader */
    float   pitch_bend;   /* -1..+1: External MIDI Pitch Bend (±2 semitones) */
    float   master_hpf;   /* 0..1: Sub-Tightener High-Pass Filter (20..180 Hz) */
    float   master_tilt;  /* -1..+1: Mastering Tilt EQ (warm dark <-> airy bright) */
    float   master_tape;  /* 0..1: Analog Tape Warmth / Soft-Clip Saturation */

    /* Noise / Geiger / Preamp texture & BBD Chorus */
    float noise_lvl;      /* 0..1 */
    uint8_t noise_mode;   /* 0 = PINK WIND, 1 = GEIGER CRACKLE, 2 = SUB RUMBLE */
    float chorus_mix;     /* 0..1 (BBD tri-phase analog ensemble) */

    /* Modular CV Generator Controls (Sample & Hold + Analog Slew Limiter + 8x8 Pin Matrix) */
    float sh_rate;        /* 0..1 (0.15 Hz .. 18 Hz S&H and LFO clock rate) */
    float slew_amt;       /* 0..1 (RC slew limiter glide on S&H and stepped CVs) */
    int8_t pin_matrix[S42_NMOD_SRC][S42_NMOD_DST]; /* 0=off, 1=+50% gold, 2=+100% copper, -1=-75% inv */
    float mod_src_val[S42_NMOD_SRC]; /* Live instantaneous -1..+1 CV readout for UI LEDs */
    uint8_t pin_preset;   /* 0..4 active modular pin matrix preset */

    /* Entropy Helios 16-Slot Stackable Magnetic Stereo Cartridge Multi-FX + Infinite Freeze + Helios LFO Speed */
    uint8_t cart_type;    /* 0..15: currently focused cartridge in UI */
    uint16_t cart_mask;   /* Bitmask of stacked ON/OFF cartridges (bit i = 1 -> cart i active in FX chain!) */
    uint8_t cart_freeze;  /* 1 = Infinite FDN Reverb Freeze active */
    uint8_t lfo_fast;     /* 0 = Slow Ambient LFO, 1 = Entropy Helios Audio-Rate LFO (16x speed) */
    float cart_mix;       /* 0..1: Master FX rack wet send */
    float cart_decay;     /* 0..1 */
    float cart_shimmer;   /* 0..1 */
    float cart_tone;      /* 0..1: Default/global tone fallback */
    float cart_lvl[S42_NCART];   /* 0..1: Independent per-cartridge wet/intensity level! */
    float cart_param[S42_NCART]; /* 0..1: Independent per-cartridge tone / rate / pitch param! */

    /* Live Telemetry for 240x240 ENTROPY Helios Hardware Front-End UI */
    uint32_t held_mask;            /* Bitmask of physical keys currently pressed (0..26) */
    uint32_t latch_mask;           /* Bitmask of active / sustain-latched keys (0..26) */
    uint8_t  key_vel[S42_NKEYS];   /* Strike velocity (0..127) per key */
    uint8_t  key_at[S42_NKEYS];    /* Continuous polyphonic aftertouch pressure (0..127) per key */
    float    last_vel;             /* Latest key strike velocity (0..1) */
    float    max_at;               /* Peak polyphonic aftertouch pressure across held keys (0..1) */
    float    vcf_pluck_env;        /* Live velocity filter pluck envelope (0..1) */
    float    vu_l, vu_r;
    float    vu_peak_l, vu_peak_r; /* Peak-hold stereo VU meters (0..1) */
    float    gr_db;                /* Mastering limiter gain reduction in dB (0..18) */
    float    spec[16];             /* 16-band real-time logarithmic spectrum analyzer (0..1) */
    float    scope_l[64], scope_r[64];
} solar42_state_t;

void solar42_init(float fs);
void solar42_load_patch(uint32_t idx);
void solar42_get_state(solar42_state_t *out);
void solar42_set_state(const solar42_state_t *in);
void solar42_panic(void);

/* Dual-Core SMP API (Core 0 + Core 1) */
void solar42_core2_mode(int mode); /* 0 = single-core, 1 = hardware Core 1, 2 = host dual-core sim */
void solar42_core2_work(void);     /* Polled on Core 1 via plat_cpu2_job(solar42_core2_work) */
void solar42_profile_clock(uint32_t (*clk)(void));
void solar42_core2_stats(uint32_t *v_jobs, uint32_t *fx_jobs, uint32_t *late, uint32_t *lost,
                         uint32_t *c1_pct, int *mode);

const char *solar42_patch_name(uint32_t idx);
uint8_t solar42_patch_vibe(uint32_t idx);
const char *solar42_cart_name(uint32_t idx);
const char *solar42_scale_name(uint32_t idx);
const char *solar42_wave_name(uint32_t idx);
const char *solar42_keymode_name(uint8_t mode);
const char *solar42_perfmode_name(uint8_t mode);
const char *solar42_arpmode_name(uint8_t mode);
const char *solar42_mod_src_name(unsigned src);
const char *solar42_mod_dst_name(unsigned dst);
const char *solar42_evolve_name(uint8_t ev);
const char *solar42_tuning_name(uint8_t tun);
const char *solar42_chord_name(uint8_t ch);
const char *solar42_extsrc_name(uint8_t src);
float s42_plate_to_note(uint8_t root, uint8_t scale, uint8_t plate, int8_t oct);

/* UI & Physical FM-1 Panel Interaction Hooks */
void solar42_step_patch(int dir);
void solar42_toggle_play(void);
void solar42_set_evolve(uint8_t ev);
void solar42_cycle_evolve(void);
void solar42_set_perf_mode(uint8_t mode);
void solar42_cycle_perf_mode(void);
void solar42_set_arp_mode(uint8_t mode);
void solar42_cycle_arp_mode(void);
void solar42_toggle_sustain_latch(void);
void solar42_step_play_octave(int delta);
void solar42_kick_spring(float intensity);
void solar42_toggle_cart_freeze(void);
void solar42_cycle_joy_rec(void);
void solar42_touch_plate(unsigned mel_idx, unsigned plate_idx, float vel, float press);
void solar42_fm1_key_on(unsigned key_0_to_26, float vel, float press);
void solar42_fm1_key_pressure(unsigned key_0_to_26, float press);
void solar42_fm1_key_off(unsigned key_0_to_26);
void solar42_retune_drones_to_root(uint8_t new_root);
void solar42_trigger_sh_burst(void);
void solar42_toggle_hold(unsigned col_idx);
void solar42_toggle_step_gate(unsigned mel_idx, unsigned step_idx);

/* 8x8 Modular Pin Matrix interaction hooks */
void solar42_pin_step(unsigned src, unsigned dst, int delta);
void solar42_pin_clear(unsigned src, unsigned dst, int clear_all);
void solar42_pin_preset_step(int dir);
void solar42_adjust_cv_gen(unsigned slot, int delta);

/* Studio v2.0 Hooks: USB Audio In, MIDI Clock / Expression, Scene A/B Morph & Compare */
int  solar42_wants_usb_in(void);
int  solar42_get_play_oct(void);
void solar42_set_spec_en(int on);
void solar42_feed_usb_audio(const int32_t *lr, uint32_t n);
void solar42_midi_clock(uint32_t now_ms);
void solar42_midi_start(void);
void solar42_midi_stop(void);
void solar42_midi_pitch_bend(int16_t bend_14b);
void solar42_midi_chan_at(uint8_t press);
void solar42_midi_poly_at(uint8_t note, uint8_t press);
void solar42_midi_sustain_pedal(uint8_t down);
void solar42_capture_scene_b(void);
void solar42_set_morph(float pos);
void solar42_toggle_compare(void);

/* Real-time stereo 24-bit render (n = 256 frames, gain = 0..1) */
void solar42_render(int32_t *lr, uint32_t n, float gain);
