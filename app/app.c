/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 ENTROPY HELIOS contributors
 *
 * ENTROPY HELIOS: Standalone Dual-Core Component-Modeled HELIOS Music Entropy Helios Polyphonic Ambient Synthesizer
 * for the M-VAVE FM-1 (240x240 RGB565 IPS LCD, 27 Capacitive Touch Keys with Pressure, 7 Encoders, 14 Buttons,
 * Dual-Core Asymmetric SMP on Core 0 + Core 1).
 */
#include "dsp.h"
#include "helios.h"

/* ---- Hardware Section Pages (Selected via FM-1 Illuminated Push-Buttons & OCT-/+) ---- */
enum {
    PG_HOME = 0,     /* B_HOME: Master Console (K1=VCF CUT, K2=VCF RES, K3=FX MIX, K4=JOYSTICK X/Y) */
    PG_DRN_1_3,      /* B_EDIT (1/3): Left Drone Bank V1..V3 (K1=PITCH, K2=SPREAD, K3=LEVEL, K4=SUB/PAN) */
    PG_DRN_6_8,      /* B_EDIT (2/3): Right Drone Bank V6..V8 (K1=PITCH, K2=SPREAD, K3=LEVEL, K4=SUB/PAN) */
    PG_DRN_ALL,      /* B_EDIT (3/3): All 6 Drones Macro (K1=ALL SPREAD, K2=ALL ATTACK, K3=ALL RELEASE, K4=ALL LEVEL) */
    PG_MEL_V4,       /* B_ARP  (1/2): Melodic Voice 4 Left (K1=WAVE, K2=PLATE TRIM, K3=GLIDE, K4=V4<>V5 X-MOD) */
    PG_MEL_V5,       /* B_ARP  (2/2): Melodic Voice 5 Right (K1=WAVE, K2=PLATE TRIM, K3=GLIDE, K4=VIBRATO) */
    PG_SEQ_L,        /* B_SEQ  (1/2): 5-Step Sequencer Left V4 (K1=STEP NOTE, K2=STEP GATE, K3=CLOCK DIV, K4=OCTAVE) */
    PG_SEQ_R,        /* B_SEQ  (2/2): 5-Step Sequencer Right V5 (K1=STEP NOTE, K2=STEP GATE, K3=CLOCK DIV, K4=OCTAVE) */
    PG_VCF,          /* B_FX   (1/2): Dual Russian Analog VCF (K1=CUTOFF L/R, K2=RESO L/R, K3=DRIVE, K4=MODE) */
    PG_CART,         /* B_FX   (2/2): Magnetic Stereo FX Cartridge (K1=CART TYPE, K2=FX MIX, K3=SHIMMER, K4=DECAY) */
    PG_PREAMP,       /* B_ENV (1/2):  Entropy Symbiosis Preamp & Noise (K1=EXT IN, K2=NOISE LVL, K3=NOISE MODE, K4=CHORUS) */
    PG_EQ,           /* B_ENV (2/2):  3-Band Parametric Master EQ (K1=LOW dB, K2=MID dB, K3=MID FRQ, K4=HIGH dB) */
    PG_MATRIX,       /* B_LFO:        8x8 Modular CV Pin Matrix (K1=SRC ROW, K2=DST COL, K3=PIN STATE, K4=S&H / SLEW) */
    PG_GLOBAL,       /* B_GLO  (1/2): Global Colorway, Tuning & Dual-Core SMP */
    PG_HELP,         /* B_GLO  (2/2): Built-In On-Screen Quick-Help Cheat Sheet */
    PG_COUNT
};

static const char *const PG_TITLE[PG_COUNT] = {
    "HOME // MASTER & JOY",
    "EDIT // DRONE BANK L",
    "EDIT // DRONE BANK R",
    "EDIT // 6-DRONE MACRO",
    "ARP // MELODIC V4 (L)",
    "ARP // MELODIC V5 (R)",
    "SEQ // 5-STEP SEQ L",
    "SEQ // 5-STEP SEQ R",
    "FX // DUAL ANALOG VCF",
    "FX // FX CARTRIDGE",
    "ENV // PREAMP & NOISE",
    "ENV // MASTER PARAM EQ",
    "LFO // 8x8 PIN MATRIX",
    "GLO // COLOR & GLOBAL",
    "GLO // QUICK HELP"
};

static const uint8_t PG_BTN_OF[PG_COUNT] = {
    B_HOME, B_EDIT, B_EDIT, B_EDIT, B_ARP, B_ARP, B_SEQ, B_SEQ, B_FX, B_FX, B_ENV, B_ENV, B_LFO, B_GLO, B_GLO
};

static const char *const NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/* ---- Runtime UI & Hardware Controller State ---- */
static uint8_t ui_page = PG_HOME;
static uint8_t ui_sel_drone_l = 0u; /* 0..2 = V1..V3 */
static uint8_t ui_sel_drone_r = 3u; /* 3..5 = V6..V8 */
static uint8_t ui_sel_step_l  = 0u; /* 0..4 = S1..S5 on V4 */
static uint8_t ui_sel_step_r  = 0u; /* 0..4 = S1..S5 on V5 */
static uint8_t ui_pin_r       = 0u; /* 0..7 = CV Source Row in 8x8 Matrix */
static uint8_t ui_pin_c       = 4u; /* 0..7 = CV Dest Col in 8x8 Matrix */
static uint8_t ui_sel_alt     = 0u; /* Toggled by B_SEL for secondary knob actions */
static uint8_t ui_scope_mode  = 0u; /* 0 = WAVEFORM, 1 = LISSAJOUS PHASE, 2 = 16-BAND SPECTRUM */
static uint8_t ui_midi_out    = 1u; /* 1 = Transmit FM-1 27-key Notes + Vel + Poly-AT over USB-MIDI */
static uint8_t ui_usb_full    = 0u; /* 1 = Studio Unity 0 dBFS USB Recording (independent of MASTER) */
static uint8_t ui_spk_mute    = 0u; /* 1 = Internal Speaker Muted (for silent USB studio recording) */
static uint32_t ui_zoom_t0    = 0u; /* Timestamp of last module zoom transition (360 ms expand/collapse anim) */
static uint8_t  ui_zoom_from  = 0u; /* Previous ui_page before zoom transition */
static uint8_t  ui_act_knob   = 0u; /* Most recently turned knob (0..3) for glow highlight */
static uint32_t ui_act_until  = 0u; /* Timestamp until active-knob glow highlight expires */
static uint32_t ui_splash_end = 0u; /* 1800ms live-animated P31 phosphor CRT boot splash timer */
static uint8_t  ui_studio_linked = 0u; /* 1 = WebMIDI Studio has connected/synced over USB */
static uint8_t  ui_sysex_dirty   = 0u; /* 1 = Physical FM-1 control changed; push live sync to Studio */
static uint32_t ui_sysex_last_ms = 0u; /* Throttle live FM-1 -> Studio SysEx push (~110 ms) */
static uint32_t ui_cpu_last_ms   = 0u; /* Periodic live CPU telemetry push (~750 ms when Studio linked) */
static uint32_t ui_usb_sess_last = 0u; /* Track USB session so cable unplug cleanly unlinks Studio */
static float    ui_master_vol    = 1.0f; /* Studio/CC7 software master volume trim (0..1, multiplied with HW wheel) */
static uint8_t  ui_master_mute   = 0u;   /* 1 = Studio soft audio mute (keeps sequencers/phase running) */
static uint8_t hl_cpu2_state = 0u; /* 0=not tried, 1=Core 1 UP, 2=no report, 3=skipped, 4=float fail */

static void hl_set_page(uint8_t next_pg)
{
    if (next_pg != ui_page) {
        ui_zoom_from = ui_page;
        ui_page = next_pg;
        ui_zoom_t0 = plat_ms();
    }
    app_say("PAGE", PG_TITLE[ui_page % PG_COUNT]);
}
static volatile float hl_probe_x, hl_probe_y;
static volatile uint32_t hl_probe_ok;
static uint32_t prev_buttons, prev_keys;
static uint32_t notice_until;
static char notice_a[20], notice_b[24];
static uint32_t overloads_cnt;

static void hl_probe_fn(void)
{
    hl_probe_y = hl_probe_x * 1.5f + 0.25f;
    hl_probe_ok = 1u;
}
#ifdef PLAT_TICKS_PER_US
static void hl_probe_u32(uint32_t arg)
{
    (void)arg;
    hl_probe_fn();
}
#endif
int app_spk_muted(void) { return ui_spk_mute != 0u; }

/* ---- Persistent Flash Save / Load (OBJ_SETTINGS + 8 Entropy Slots E01..E08 + 16 Pure HW Slots H01..H16) ---- */
#define HL_SAVE_MAGIC 0x484C3133u /* "HL13" (v1.3: 3-Band Parametric Master EQ + PURE/ENTROPY dual-mode) */
#define HL_BANK_MAGIC 0x48423133u /* "HB13" (8-slot Flash Patch Bank) */
typedef struct {
    uint32_t magic;
    uint8_t patch, evolve, root, scale, bpm, key_mode, cart_type, pin_preset;
    uint8_t perf_mode, arp_mode, sustain_latch, vcf_link, cart_freeze;
    uint8_t vcf_mode0, vcf_mode1, seq_len0, seq_len1;
    uint8_t ext_src, tuning, chord_mode, midi_out;
    uint16_t cart_mask;
    uint8_t lfo_fast, ui_theme, pure_mode;
    int8_t  play_oct;
    float   vel_sens, at_sens, spring_mix, spring_decay;
    float   master_hpf, master_tilt, master_tape;
    float vcf_cut0, vcf_cut1, vcf_res0, vcf_res1, vcf_drive;
    float cart_mix, cart_decay, cart_shimmer, chorus_mix, ext_in_lvl, v45_xmod, sh_rate, slew_amt;
    float d_morph[HL_NDRONE];
    uint8_t d_hold[HL_NDRONE];
    uint8_t d_note_q[HL_NDRONE];
    uint8_t d_spread_q[HL_NDRONE];
    uint8_t d_level_q[HL_NDRONE];
    uint8_t d_glide_q[HL_NDRONE];
    int8_t  d_pan[HL_NDRONE];
    uint8_t d_sub[HL_NDRONE];
    uint8_t m_wave[HL_NMEL], m_lvl_q[HL_NMEL], m_seq_run[HL_NMEL];
    uint8_t m_phase_q[HL_NMEL], m_drift[HL_NMEL];
    int8_t pin_matrix[HL_NMOD_SRC][HL_NMOD_DST];
    float plate_trim[HL_NPLATES];
    uint8_t cart_lvl_q[HL_NCART];
    uint8_t cart_par_q[HL_NCART];
    uint8_t eq_on, eq_lo_hz_q, eq_mid_hz_q, eq_mid_q_q, eq_hi_hz_q;
    int8_t  eq_lo_db_q, eq_mid_db_q, eq_hi_db_q; /* -24..+24 in 0.5 dB steps */
} hl_save_t;

typedef struct {
    uint32_t magic;
    uint8_t used[HL_NUSER];
    hl_save_t slot[HL_NUSER];
} hl_user_bank_t;

_Static_assert(sizeof(hl_user_bank_t) <= 3840u, "hl_user_bank_t must fit inside 3840-byte flash store sector");

static hl_user_bank_t user_bank;   /* E01..E08 (OBJ_USER_BANK: Entropy User Slots) */
static hl_user_bank_t pure_bank_a; /* H01..H08 (OBJ_PURE_A: Pure HW INIT Slots 1..8) */
static hl_user_bank_t pure_bank_b; /* H09..H16 (OBJ_PURE_B: Pure HW INIT Slots 9..16) */
static uint8_t ui_pure_mode = 0u;  /* 0 = ENTROPY Mode (default), 1 = PURE Hardware Mode */
static uint8_t cur_slot_idx = 0u;  /* ENTROPY: 0..35 = Factory P01..P36, 36..43 = User E01..E08 */
static uint8_t cur_pure_idx = 0u;  /* PURE: 0..15 = Hardware INIT Slots H01..H16 */
static uint8_t ui_theme = 0u;      /* 0=BLACK, 1=CREAM, 2=AMBER YEL, 3=COBALT, 4=OLIVE */
static uint32_t save_sig_last, save_dirty_at, save_quiet_at;
static uint8_t save_ready;

static uint32_t hl_calc_save_sig(const hl_save_t *sv)
{
    const uint8_t *p = (const uint8_t *)sv;
    uint32_t i, h = 2166136261u;
    for (i = 0; i < sizeof(*sv); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

static void hl_make_save(hl_save_t *sv)
{
    helios_state_t st;
    uint32_t i, j;
    helios_get_state(&st);
    sv->magic         = HL_SAVE_MAGIC;
    sv->patch         = st.patch;
    sv->evolve        = st.evolve;
    sv->root          = st.root;
    sv->scale         = st.scale;
    sv->bpm           = st.bpm;
    sv->key_mode      = st.key_mode;
    sv->cart_type     = st.cart_type;
    sv->pin_preset    = st.pin_preset;
    sv->perf_mode     = st.perf_mode;
    sv->arp_mode      = st.arp_mode;
    sv->sustain_latch = st.sustain_latch;
    sv->vcf_link      = st.vcf_link;
    sv->cart_freeze   = st.cart_freeze;
    sv->vcf_mode0     = st.vcf_mode[0];
    sv->vcf_mode1     = st.vcf_mode[1];
    sv->seq_len0      = st.mel[0].seq_len;
    sv->seq_len1      = st.mel[1].seq_len;
    sv->ext_src       = st.ext_src;
    sv->tuning        = st.tuning;
    sv->chord_mode    = st.chord_mode;
    sv->midi_out      = ui_midi_out;
    sv->cart_mask     = st.cart_mask;
    sv->lfo_fast      = st.lfo_fast;
    sv->ui_theme      = ui_theme;
    sv->pure_mode     = ui_pure_mode & 1u;
    sv->play_oct      = st.play_oct;
    sv->vel_sens      = st.vel_sens;
    sv->at_sens       = st.at_sens;
    sv->spring_mix    = st.spring_mix;
    sv->spring_decay  = st.spring_decay;
    sv->master_hpf    = st.master_hpf;
    sv->master_tilt   = st.master_tilt;
    sv->master_tape   = st.master_tape;
    sv->vcf_cut0      = st.vcf_cut[0];
    sv->vcf_cut1      = st.vcf_cut[1];
    sv->vcf_res0      = st.vcf_res[0];
    sv->vcf_res1      = st.vcf_res[1];
    sv->vcf_drive     = st.vcf_drive;
    sv->cart_mix      = st.cart_mix;
    sv->cart_decay    = st.cart_decay;
    sv->cart_shimmer  = st.cart_shimmer;
    sv->chorus_mix    = st.chorus_mix;
    sv->ext_in_lvl    = st.ext_in_lvl;
    sv->v45_xmod      = st.v45_xmod;
    sv->sh_rate       = st.sh_rate;
    sv->slew_amt      = st.slew_amt;
    for (i = 0; i < HL_NDRONE; i++) {
        sv->d_morph[i]    = st.drone[i].wave_morph;
        sv->d_hold[i]     = st.drone[i].hold;
        sv->d_note_q[i]   = (uint8_t)clamp((int32_t)(st.drone[i].note + 0.5f), 24, 84);
        sv->d_spread_q[i] = (uint8_t)(br_clampf(st.drone[i].spread, 0.0f, 1.0f) * 100.0f + 0.5f);
        sv->d_level_q[i]  = (uint8_t)(br_clampf(st.drone[i].level, 0.0f, 1.0f) * 100.0f + 0.5f);
        sv->d_glide_q[i]  = (uint8_t)(br_clampf(st.drone[i].glide, 0.0f, 1.0f) * 100.0f + 0.5f);
        sv->d_pan[i]      = st.drone[i].pan;
        sv->d_sub[i]      = st.drone[i].sub & 1u;
    }
    for (i = 0; i < HL_NMEL; i++) {
        sv->m_wave[i]    = st.mel[i].wave;
        sv->m_lvl_q[i]   = (uint8_t)(br_clampf(st.mel[i].level, 0.0f, 1.0f) * 100.0f + 0.5f);
        sv->m_seq_run[i] = st.mel[i].seq_run & 1u;
        sv->m_phase_q[i] = (uint8_t)(br_clampf(st.mel[i].seq_phase, 0.0f, 1.0f) * 120.0f + 0.5f);
        sv->m_drift[i]   = st.mel[i].seq_drift & 1u;
    }
    for (i = 0; i < HL_NMOD_SRC; i++)
        for (j = 0; j < HL_NMOD_DST; j++)
            sv->pin_matrix[i][j] = st.pin_matrix[i][j];
    for (i = 0; i < HL_NPLATES; i++)
        sv->plate_trim[i] = st.plate_trim[i];
    for (i = 0; i < HL_NCART; i++) {
        sv->cart_lvl_q[i] = (uint8_t)(br_clampf(st.cart_lvl[i], 0.0f, 1.0f) * 100.0f + 0.5f);
        sv->cart_par_q[i] = (uint8_t)(br_clampf(st.cart_param[i], 0.0f, 1.0f) * 100.0f + 0.5f);
    }
    sv->eq_on       = st.eq_on & 1u;
    sv->eq_lo_db_q  = (int8_t)clamp((int32_t)(st.eq_lo_db * 2.0f + (st.eq_lo_db >= 0.0f ? 0.5f : -0.5f)), -24, 24);
    sv->eq_mid_db_q = (int8_t)clamp((int32_t)(st.eq_mid_db * 2.0f + (st.eq_mid_db >= 0.0f ? 0.5f : -0.5f)), -24, 24);
    sv->eq_hi_db_q  = (int8_t)clamp((int32_t)(st.eq_hi_db * 2.0f + (st.eq_hi_db >= 0.0f ? 0.5f : -0.5f)), -24, 24);
    sv->eq_lo_hz_q  = (uint8_t)(br_clampf(st.eq_lo_hz, 0.0f, 1.0f) * 100.0f + 0.5f);
    sv->eq_mid_hz_q = (uint8_t)(br_clampf(st.eq_mid_hz, 0.0f, 1.0f) * 100.0f + 0.5f);
    sv->eq_mid_q_q  = (uint8_t)(br_clampf(st.eq_mid_q, 0.0f, 1.0f) * 100.0f + 0.5f);
    sv->eq_hi_hz_q  = (uint8_t)(br_clampf(st.eq_hi_hz, 0.0f, 1.0f) * 100.0f + 0.5f);
}

static void hl_apply_save(const hl_save_t *sv)
{
    helios_state_t st;
    uint32_t i, j;
    if (sv->pure_mode) helios_load_init(1u);
    else               helios_load_patch(sv->patch % HL_NPATCHES);
    helios_get_state(&st);
    st.pure_mode     = sv->pure_mode & 1u;
    st.evolve        = st.pure_mode ? 0u : (sv->evolve & 3u);
    st.root          = sv->root % 12u;
    st.scale         = sv->scale % 7u;
    st.bpm           = sv->bpm < 30u ? 30u : sv->bpm > 240u ? 240u : sv->bpm;
    st.key_mode      = sv->key_mode % 3u;
    st.cart_type     = sv->cart_type % HL_NCART;
    st.pin_preset    = sv->pin_preset % 5u;
    st.perf_mode     = sv->perf_mode % 3u;
    st.arp_mode      = sv->arp_mode % 6u;
    st.sustain_latch = sv->sustain_latch & 1u;
    st.vcf_link      = sv->vcf_link & 1u;
    st.cart_freeze   = sv->cart_freeze & 1u;
    st.vcf_mode[0]   = sv->vcf_mode0 & 3u;
    st.vcf_mode[1]   = sv->vcf_mode1 & 3u;
    st.mel[0].seq_len = (sv->seq_len0 >= 2u && sv->seq_len0 <= HL_NSTEPS) ? sv->seq_len0 : HL_NSTEPS;
    st.mel[1].seq_len = (sv->seq_len1 >= 2u && sv->seq_len1 <= HL_NSTEPS) ? sv->seq_len1 : HL_NSTEPS;
    st.ext_src       = sv->ext_src % 3u;
    st.tuning        = sv->tuning % 5u;
    st.chord_mode    = sv->chord_mode % 6u;
    ui_midi_out      = sv->midi_out & 1u;
    st.cart_mask     = sv->cart_mask;
    st.lfo_fast      = sv->lfo_fast & 1u;
    ui_theme         = sv->ui_theme % 5u;
    st.play_oct      = (int8_t)clamp((int32_t)sv->play_oct, -2, 2);
    st.vel_sens      = br_clampf(sv->vel_sens, 0.0f, 1.0f);
    st.at_sens       = br_clampf(sv->at_sens, 0.0f, 1.0f);
    st.master_hpf    = br_clampf(sv->master_hpf, 0.0f, 1.0f);
    st.master_tilt   = br_clampf(sv->master_tilt, -1.0f, 1.0f);
    st.master_tape   = br_clampf(sv->master_tape, 0.0f, 1.0f);
    st.spring_mix    = br_clampf(sv->spring_mix, 0.0f, 1.0f);
    st.spring_decay  = br_clampf(sv->spring_decay, 0.0f, 1.0f);
    st.vcf_cut[0]    = br_clampf(sv->vcf_cut0, 0.0f, 1.0f);
    st.vcf_cut[1]    = br_clampf(sv->vcf_cut1, 0.0f, 1.0f);
    st.vcf_res[0]    = br_clampf(sv->vcf_res0, 0.0f, 1.0f);
    st.vcf_res[1]    = br_clampf(sv->vcf_res1, 0.0f, 1.0f);
    st.vcf_drive     = br_clampf(sv->vcf_drive, 0.0f, 1.0f);
    st.cart_mix      = br_clampf(sv->cart_mix, 0.0f, 1.0f);
    st.cart_decay    = br_clampf(sv->cart_decay, 0.0f, 1.0f);
    st.cart_shimmer  = br_clampf(sv->cart_shimmer, 0.0f, 1.0f);
    st.chorus_mix    = br_clampf(sv->chorus_mix, 0.0f, 1.0f);
    st.ext_in_lvl    = br_clampf(sv->ext_in_lvl, 0.0f, 1.0f);
    st.v45_xmod      = br_clampf(sv->v45_xmod, 0.0f, 1.0f);
    st.sh_rate       = br_clampf(sv->sh_rate, 0.0f, 1.0f);
    st.slew_amt      = br_clampf(sv->slew_amt, 0.0f, 1.0f);
    for (i = 0; i < HL_NDRONE; i++) {
        st.drone[i].wave_morph = br_clampf(sv->d_morph[i], 0.0f, 1.0f);
        st.drone[i].hold       = sv->d_hold[i] & 1u;
        if (sv->d_note_q[i] >= 24u && sv->d_note_q[i] <= 84u)
            st.drone[i].note   = (float)sv->d_note_q[i];
        st.drone[i].spread     = br_clampf((float)sv->d_spread_q[i] * 0.01f, 0.0f, 1.0f);
        st.drone[i].level      = br_clampf((float)sv->d_level_q[i] * 0.01f, 0.0f, 1.0f);
        st.drone[i].glide      = br_clampf((float)sv->d_glide_q[i] * 0.01f, 0.0f, 1.0f);
        st.drone[i].pan        = (sv->d_pan[i] < 0) ? -1 : (sv->d_pan[i] > 0) ? 1 : 0;
        st.drone[i].sub        = sv->d_sub[i] & 1u;
    }
    for (i = 0; i < HL_NMEL; i++) {
        st.mel[i].wave      = sv->m_wave[i] % 6u;
        st.mel[i].level     = br_clampf((float)sv->m_lvl_q[i] * 0.01f, 0.0f, 1.0f);
        st.mel[i].seq_run   = sv->m_seq_run[i] & 1u;
        st.mel[i].seq_phase = br_clampf((float)sv->m_phase_q[i] * (1.0f / 120.0f), 0.0f, 1.0f);
        st.mel[i].seq_drift = sv->m_drift[i] & 1u;
    }
    for (i = 0; i < HL_NMOD_SRC; i++)
        for (j = 0; j < HL_NMOD_DST; j++)
            st.pin_matrix[i][j] = sv->pin_matrix[i][j];
    for (i = 0; i < HL_NPLATES; i++)
        st.plate_trim[i] = br_clampf(sv->plate_trim[i], -1.0f, 1.0f);
    for (i = 0; i < HL_NCART; i++) {
        st.cart_lvl[i]   = br_clampf((float)sv->cart_lvl_q[i] * 0.01f, 0.0f, 1.0f);
        st.cart_param[i] = br_clampf((float)sv->cart_par_q[i] * 0.01f, 0.0f, 1.0f);
    }
    st.eq_on     = sv->eq_on & 1u;
    st.eq_lo_db  = br_clampf((float)sv->eq_lo_db_q * 0.5f, -12.0f, 12.0f);
    st.eq_mid_db = br_clampf((float)sv->eq_mid_db_q * 0.5f, -12.0f, 12.0f);
    st.eq_hi_db  = br_clampf((float)sv->eq_hi_db_q * 0.5f, -12.0f, 12.0f);
    st.eq_lo_hz  = br_clampf((float)sv->eq_lo_hz_q * 0.01f, 0.0f, 1.0f);
    st.eq_mid_hz = br_clampf((float)sv->eq_mid_hz_q * 0.01f, 0.0f, 1.0f);
    st.eq_mid_q  = br_clampf((float)sv->eq_mid_q_q * 0.01f, 0.0f, 1.0f);
    st.eq_hi_hz  = br_clampf((float)sv->eq_hi_hz_q * 0.01f, 0.0f, 1.0f);
    helios_set_state(&st);
}

static void hl_init_bank_obj(uint32_t obj, hl_user_bank_t *bk)
{
    int blen = plat_store_load(obj, bk, sizeof(*bk));
    if (blen != (int)sizeof(*bk) || bk->magic != HL_BANK_MAGIC) {
        uint8_t *z = (uint8_t *)bk;
        uint32_t i;
        for (i = 0; i < sizeof(*bk); i++) z[i] = 0;
        bk->magic = HL_BANK_MAGIC;
    }
}

static void hl_load_saved(void)
{
    hl_save_t sv;
    hl_init_bank_obj(OBJ_USER_BANK, &user_bank);
    hl_init_bank_obj(OBJ_PURE_A, &pure_bank_a);
    hl_init_bank_obj(OBJ_PURE_B, &pure_bank_b);
    int len = plat_store_load(OBJ_SETTINGS, &sv, sizeof sv);
    if (len != (int)sizeof sv || sv.magic != HL_SAVE_MAGIC)
        return;
    /* Always boot cleanly in ENTROPY mode (never leak a PURE INIT state onto the boot preset!) */
    ui_pure_mode = 0u;
    if (sv.pure_mode) {
        ui_theme     = sv.ui_theme % 5u;
        ui_midi_out  = sv.midi_out & 1u;
        cur_slot_idx = 0u;
        helios_load_patch(0u);
        helios_set_pure_mode(0u);
    } else {
        hl_apply_save(&sv);
        cur_slot_idx = sv.patch % HL_NPATCHES;
    }
    save_sig_last = hl_calc_save_sig(&sv);
    save_ready = 1u;
}

static uint8_t hl_pure_slot_used(unsigned h_idx)
{
    h_idx %= HL_NPURE;
    return (h_idx < 8u) ? pure_bank_a.used[h_idx] : pure_bank_b.used[h_idx - 8u];
}

static void hl_save_pure_slot(unsigned h_idx)
{
    hl_user_bank_t *bk;
    uint32_t obj;
    unsigned sub;
    h_idx %= HL_NPURE;
    bk  = (h_idx < 8u) ? &pure_bank_a : &pure_bank_b;
    obj = (h_idx < 8u) ? OBJ_PURE_A   : OBJ_PURE_B;
    sub = h_idx & 7u;
    bk->magic = HL_BANK_MAGIC;
    bk->used[sub] = 1u;
    hl_make_save(&bk->slot[sub]);
    bk->slot[sub].pure_mode = 1u;
    bk->slot[sub].evolve    = 0u;
    cur_pure_idx = (uint8_t)h_idx;
    plat_store_save(obj, bk, sizeof(*bk));
}

static void hl_load_pure_slot(unsigned h_idx)
{
    const hl_user_bank_t *bk;
    unsigned sub;
    h_idx %= HL_NPURE;
    cur_pure_idx = (uint8_t)h_idx;
    ui_pure_mode = 1u;
    bk  = (h_idx < 8u) ? &pure_bank_a : &pure_bank_b;
    sub = h_idx & 7u;
    if (bk->used[sub] && bk->slot[sub].magic == HL_SAVE_MAGIC) {
        hl_apply_save(&bk->slot[sub]);
    } else {
        helios_load_init(1u);
    }
}

static void hl_save_user_slot(unsigned u_idx)
{
    u_idx %= HL_NUSER;
    user_bank.magic = HL_BANK_MAGIC;
    user_bank.used[u_idx] = 1u;
    hl_make_save(&user_bank.slot[u_idx]);
    user_bank.slot[u_idx].pure_mode = 0u;
    cur_slot_idx = (uint8_t)(HL_NPATCHES + u_idx);
    plat_store_save(OBJ_USER_BANK, &user_bank, sizeof user_bank);
}

static void hl_load_slot(unsigned idx)
{
    idx %= HL_NTOTAL_P;
    cur_slot_idx = (uint8_t)idx;
    ui_pure_mode = 0u;
    if (idx < HL_NPATCHES) {
        helios_load_patch(idx);
        helios_set_pure_mode(0u);
    } else {
        unsigned u = idx - HL_NPATCHES;
        if (user_bank.used[u] && user_bank.slot[u].magic == HL_SAVE_MAGIC) {
            hl_apply_save(&user_bank.slot[u]);
        } else {
            helios_load_init(0u);
        }
    }
}

static void hl_set_mode(uint8_t pure)
{
    ui_pure_mode = pure ? 1u : 0u;
    if (ui_pure_mode) {
        hl_load_pure_slot(cur_pure_idx);
        app_say("PURE HW MODE", "16 INIT SLOTS");
    } else {
        hl_load_slot(cur_slot_idx);
        app_say("ENTROPY MODE", "GENERATIVE ON");
    }
}

static void hl_save_tick(uint32_t now)
{
    hl_save_t sv;
    helios_state_t st;
    uint32_t sig;
    helios_get_state(&st);
    hl_make_save(&sv);
    sig = hl_calc_save_sig(&sv);
    if (st.playing) save_quiet_at = now;
    if (!save_ready) {
        save_ready = 1u;
        save_sig_last = sig;
        return;
    }
    if (sig != save_sig_last) {
        if (!save_dirty_at) save_dirty_at = now | 1u;
        /* Save to flash once stopped for 3 seconds (avoids XIP flash pause while audio plays) */
        if (!st.playing && now - save_quiet_at >= 3000u && now - save_dirty_at >= 1500u) {
            if (plat_store_save(OBJ_SETTINGS, &sv, sizeof sv) == 0) {
                save_sig_last = sig;
                save_dirty_at = 0u;
            }
        }
    }
}

/* ---- String & Number Formatting Helpers (No stdio/snprintf required!) ---- */
static void hl_cat(char *dst, const char *src)
{
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = 0;
}

static void hl_cat_u(char *dst, unsigned v)
{
    char tmp[12];
    unsigned n = 0;
    while (*dst) dst++;
    do {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    } while (v && n < 10u);
    while (n) *dst++ = tmp[--n];
    *dst = 0;
}

static void hl_cat_i(char *dst, int v)
{
    while (*dst) dst++;
    if (v < 0) {
        *dst++ = '-';
        *dst = 0;
        hl_cat_u(dst, (unsigned)(-v));
    } else {
        *dst++ = '+';
        *dst = 0;
        hl_cat_u(dst, (unsigned)v);
    }
}

static void hl_fmt_pct(char *buf, float v01)
{
    int p = (int)(v01 * 100.0f + 0.5f);
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    buf[0] = 0;
    hl_cat_u(buf, (unsigned)p);
    hl_cat(buf, "%");
}

static void hl_fmt_note(char *buf, float midi)
{
    int n = (int)(midi + 0.5f);
    int oct;
    if (n < 0) n = 0;
    if (n > 127) n = 127;
    oct = (n / 12) - 1;
    buf[0] = 0;
    hl_cat(buf, NOTE_NAMES[n % 12]);
    if (oct < 0) {
        hl_cat(buf, "-1");
    } else {
        hl_cat_u(buf, (unsigned)oct);
    }
}

void app_say(const char *a, const char *b)
{
    unsigned i;
    for (i = 0; i + 1u < sizeof notice_a && a && a[i]; i++) notice_a[i] = a[i];
    notice_a[i] = 0;
    for (i = 0; i + 1u < sizeof notice_b && b && b[i]; i++) notice_b[i] = b[i];
    notice_b[i] = 0;
    notice_until = plat_ms() + 1400u;
}

/* ---- Live 44.1 kHz Stereo USB Audio In -> Entropy Helios FX Processor Hooks (Zero-Stack IRQ-Safe!) ---- */
int app_usb_audio_wants_in(void)
{
    return helios_wants_usb_in();
}

void app_usb_audio_in(const int32_t *lr, uint32_t n)
{
    helios_feed_usb_audio(lr, n);
}

/* ---- WebMIDI Studio & Patch Librarian SysEx Protocol (F0 7D 48 4C <cmd> ... F7) ---- */
static uint8_t hl_q7(float x)
{
    int v = (int)(br_clampf(x, 0.0f, 1.0f) * 127.0f + 0.5f);
    return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

static void hl_sysex_send_state(void)
{
    helios_state_t st;
    uint8_t f[128];
    static uint8_t e[192];
    uint32_t p = 0u, q = 0u, r, c;
    helios_get_state(&st);

    /* Frame 0x11: Core state + 8x8 pin matrix (98 bytes) */
    f[p++] = 0xF0u; f[p++] = 0x7Du; f[p++] = 0x48u; f[p++] = 0x4Cu; f[p++] = 0x11u;
    f[p++] = ui_pure_mode ? (cur_pure_idx & 0x7Fu) : (cur_slot_idx & 0x7Fu);
    f[p++] = st.perf_mode & 0x7Fu;
    f[p++] = (uint8_t)((st.evolve & 3u) | ((ui_pure_mode & 1u) << 4) | ((st.playing & 1u) << 5) | ((st.lfo_fast & 1u) << 6));
    f[p++] = st.arp_mode & 0x7Fu;
    f[p++] = st.cart_type & 0x7Fu;
    f[p++] = (uint8_t)((st.cart_freeze & 1u) | ((st.key_mode & 3u) << 1) | ((ui_theme & 7u) << 3));
    f[p++] = hl_q7(st.vcf_cut[0]);
    f[p++] = hl_q7(st.vcf_cut[1]);
    f[p++] = hl_q7(st.vcf_res[0]);
    f[p++] = hl_q7(st.vcf_drive);
    f[p++] = hl_q7(st.spring_mix);
    f[p++] = hl_q7(st.spring_decay);
    f[p++] = hl_q7(st.cart_mix);
    f[p++] = hl_q7(st.cart_shimmer);
    f[p++] = hl_q7((st.joy_x + 1.0f) * 0.5f);
    f[p++] = hl_q7((st.joy_y + 1.0f) * 0.5f);
    f[p++] = hl_q7(st.morph_pos);
    f[p++] = hl_q7(st.master_hpf);
    f[p++] = hl_q7((st.master_tilt + 1.0f) * 0.5f);
    f[p++] = hl_q7(st.master_tape);
    f[p++] = st.ext_src & 0x7Fu;
    f[p++] = hl_q7(st.ext_in_lvl);
    f[p++] = st.tuning & 0x7Fu;
    f[p++] = st.chord_mode & 0x7Fu;
    f[p++] = (uint8_t)clamp(((int)(st.bpm < 30u ? 30u : st.bpm) - 30) * 127 / 210, 0, 127);
    f[p++] = (uint8_t)((st.vcf_link & 1u) | ((st.vcf_mode[0] & 3u) << 1) | ((st.vcf_mode[1] & 3u) << 3) |
                       ((ui_spk_mute ? 2u : ui_usb_full ? 1u : 0u) << 5));
    {
        uint8_t umask = 0u;
        for (r = 0; r < HL_NUSER; r++) if (user_bank.used[r]) umask |= (uint8_t)(1u << (r & 7u));
        f[p++] = umask & 0x7Fu;
        f[p++] = (umask >> 7) & 1u;
    }
    /* 64 pin matrix cells encoded as 0=empty, 1=+50, 2=+100, 3=-75 */
    for (r = 0; r < HL_NMOD_SRC; r++)
        for (c = 0; c < HL_NMOD_DST; c++)
            f[p++] = (st.pin_matrix[r][c] < 0) ? 3u : (uint8_t)(st.pin_matrix[r][c] & 3u);
    f[p++] = 0xF7u;
    plat_sysex_send(f, p);

    /* Frame 0x12: Extended full sync (global/bus, 16-cart stack, H01..H16 used, 6 drones, V4/V5 & dual sequencers) */
    e[q++] = 0xF0u; e[q++] = 0x7Du; e[q++] = 0x48u; e[q++] = 0x4Cu; e[q++] = 0x12u;
    e[q++] = st.root % 12u;
    e[q++] = st.scale % 7u;
    e[q++] = hl_q7(st.cart_decay);
    e[q++] = hl_q7(st.chorus_mix);
    e[q++] = hl_q7(st.noise_lvl);
    e[q++] = st.noise_mode % 3u;
    e[q++] = hl_q7(st.v45_xmod);
    e[q++] = hl_q7(st.sh_rate);
    e[q++] = hl_q7(st.slew_amt);
    e[q++] = hl_q7(st.vcf_lfo);
    e[q++] = hl_q7(st.vcf_res[1]);
    e[q++] = (uint8_t)clamp((int)st.play_oct + 2, 0, 4);
    e[q++] = (uint8_t)(st.cart_mask & 0x7Fu);
    e[q++] = (uint8_t)((st.cart_mask >> 7) & 0x7Fu);
    e[q++] = (uint8_t)((st.cart_mask >> 14) & 3u);
    {
        uint16_t pmask = 0u;
        for (r = 0; r < HL_NPURE; r++) if (hl_pure_slot_used(r)) pmask |= (uint16_t)(1u << r);
        e[q++] = (uint8_t)(pmask & 0x7Fu);
        e[q++] = (uint8_t)((pmask >> 7) & 0x7Fu);
        e[q++] = (uint8_t)((pmask >> 14) & 3u);
    }
    for (r = 0; r < HL_NCART; r++) e[q++] = hl_q7(st.cart_lvl[r]);
    for (r = 0; r < HL_NCART; r++) e[q++] = hl_q7(st.cart_param[r]);
    for (r = 0; r < HL_NDRONE; r++) {
        e[q++] = st.drone[r].hold & 1u;
        e[q++] = (uint8_t)clamp((int)(st.drone[r].note + 0.5f) - 24, 0, 60);
        e[q++] = hl_q7(st.drone[r].spread);
        e[q++] = hl_q7(st.drone[r].level);
        e[q++] = hl_q7(st.drone[r].wave_morph);
        e[q++] = st.drone[r].sub & 1u;
        e[q++] = (uint8_t)(st.drone[r].pan < 0 ? 0u : st.drone[r].pan > 0 ? 2u : 1u);
        e[q++] = hl_q7(st.drone[r].attack);
        e[q++] = hl_q7(st.drone[r].glide);
    }
    for (r = 0; r < HL_NMEL; r++) {
        e[q++] = st.mel[r].wave % 6u;
        e[q++] = (uint8_t)clamp((int)st.mel[r].octave + 2, 0, 4);
        e[q++] = st.mel[r].seq_run ? (uint8_t)((st.mel[r].seq_div % 7u) + 1u) : 0u;
        e[q++] = (uint8_t)clamp((int)(st.mel[r].seq_len ? st.mel[r].seq_len : HL_NSTEPS), 2, (int)HL_NSTEPS);
        e[q++] = hl_q7(st.mel[r].level);
        e[q++] = hl_q7(st.mel[r].glide);
        e[q++] = hl_q7(st.mel[r].attack);
        e[q++] = hl_q7(st.mel[r].release);
        e[q++] = hl_q7(st.mel[r].vibrato);
        for (c = 0; c < HL_NSTEPS; c++) e[q++] = st.mel[r].steps[c] % HL_NPLATES;
        for (c = 0; c < HL_NSTEPS; c++) e[q++] = st.mel[r].step_gate[c] & 1u;
        for (c = 0; c < HL_NSTEPS; c++) e[q++] = st.mel[r].step_acc[c] & 1u;
        for (c = 0; c < HL_NSTEPS; c++) e[q++] = (uint8_t)clamp((int)(st.mel[r].step_prob[c] ? st.mel[r].step_prob[c] : 10u), 1, 10);
    }
    /* Studio 3-Band Parametric Master EQ (8 bytes: on, lo_db, lo_hz, mid_db, mid_hz, mid_q, hi_db, hi_hz) */
    e[q++] = st.eq_on & 1u;
    e[q++] = hl_q7((st.eq_lo_db + 12.0f) * (1.0f / 24.0f));
    e[q++] = hl_q7(st.eq_lo_hz);
    e[q++] = hl_q7((st.eq_mid_db + 12.0f) * (1.0f / 24.0f));
    e[q++] = hl_q7(st.eq_mid_hz);
    e[q++] = hl_q7(st.eq_mid_q);
    e[q++] = hl_q7((st.eq_hi_db + 12.0f) * (1.0f / 24.0f));
    e[q++] = hl_q7(st.eq_hi_hz);
    /* Dual Sequencer Phase (0..120 in 3° steps = 0..360°) & Reich Drift (0/1) — 4 bytes */
    e[q++] = (uint8_t)clamp((int)(st.mel[0].seq_phase * 120.0f + 0.5f), 0, 120);
    e[q++] = (uint8_t)clamp((int)(st.mel[1].seq_phase * 120.0f + 0.5f), 0, 120);
    e[q++] = st.mel[0].seq_drift & 1u;
    e[q++] = st.mel[1].seq_drift & 1u;
    /* v1.3.2: Master Vol (7b), Master Mute (1b), Exact BPM-30 (lo7, hi1), CPU0 %, Core1 %, Overloads (7 bytes; total 187 B) */
    {
        uint32_t vj = 0u, fxj = 0u, lt = 0u, ls = 0u, c1p = 0u;
        int md = 0;
        int bpm_off = clamp((int)st.bpm - 30, 0, 210);
        helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
        e[q++] = hl_q7(ui_master_vol);
        e[q++] = ui_master_mute & 1u;
        e[q++] = (uint8_t)(bpm_off & 0x7Fu);
        e[q++] = (uint8_t)((bpm_off >> 7) & 1u);
        e[q++] = (uint8_t)clamp((int)plat_cpu_pct(), 0, 100);
        e[q++] = (uint8_t)clamp((int)c1p, 0, 100);
        e[q++] = (uint8_t)(overloads_cnt > 127u ? 127u : overloads_cnt);
    }
    e[q++] = 0xF7u;
    plat_sysex_send(e, q);
}

static void hl_sysex_send_cpu(void)
{
    uint8_t f[10];
    uint32_t vj = 0u, fxj = 0u, lt = 0u, ls = 0u, c1p = 0u;
    int md = 0;
    helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
    f[0] = 0xF0u; f[1] = 0x7Du; f[2] = 0x48u; f[3] = 0x4Cu; f[4] = 0x17u;
    f[5] = (uint8_t)clamp((int)plat_cpu_pct(), 0, 100);
    f[6] = (uint8_t)clamp((int)c1p, 0, 100);
    f[7] = (uint8_t)(overloads_cnt > 127u ? 127u : overloads_cnt);
    f[8] = 0xF7u;
    plat_sysex_send(f, 9u);
}

int app_sysex(const uint8_t *p, uint32_t n)
{
    helios_state_t st;
    /* fm1-base strips F0..F7 before calling app_sysex (p[0] = 0x7D); also tolerate raw F0..F7 frames */
    if (p && n >= 2u && p[0] == 0xF0u) {
        p++; n--;
        if (n > 0u && p[n - 1u] == 0xF7u) n--;
    }
    if (!p || n < 4u || p[0] != 0x7Du || p[1] != 0x48u || p[2] != 0x4Cu)
        return 0;
    ui_studio_linked = 1u;
#ifdef PLAT_TICKS_PER_US
    ui_usb_sess_last = plat_usb_session();
#endif
    {
        uint8_t cmd = p[3];
        if (cmd == 0x01u) {
            /* Request full state + 8x8 matrix dump */
            hl_sysex_send_state();
            return 1;
        }
        if (cmd == 0x02u && n >= 7u) {
            /* Set 8x8 Pin Matrix cell: [4]=src, [5]=dst, [6]=val(0..3) */
            unsigned r = p[4] & 7u, c = p[5] & 7u;
            int8_t v = (p[6] == 3u) ? -1 : (int8_t)(p[6] & 3u);
            helios_get_state(&st);
            st.pin_matrix[r][c] = v;
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x03u && n >= 6u) {
            /* Set live parameter: [4]=param_id, [5]=val7 */
            uint8_t pid = p[4], val = p[5] & 0x7Fu;
            float vn = (float)val * (1.0f / 127.0f);
            helios_get_state(&st);
            switch (pid) {
            case 0:  st.vcf_cut[0] = vn; if (st.vcf_link) st.vcf_cut[1] = vn; break;
            case 1:  st.vcf_cut[1] = vn; st.vcf_link = 0u; break;
            case 2:  st.vcf_res[0] = vn; if (st.vcf_link) st.vcf_res[1] = vn; break;
            case 3:  st.vcf_drive  = vn; break;
            case 4:  st.spring_mix = vn; break;
            case 5:  st.spring_decay = vn; break;
            case 6:  st.cart_type  = val % HL_NCART; break;
            case 7:  st.cart_mix   = vn; break;
            case 8:  st.cart_shimmer = vn; break;
            case 9:  st.cart_freeze = val & 1u; break;
            case 10: st.joy_x = vn * 2.0f - 1.0f; break;
            case 11: st.joy_y = vn * 2.0f - 1.0f; break;
            case 12: helios_set_morph(vn); return 1;
            case 13: helios_capture_scene_b(); return 1;
            case 14: helios_kick_spring(0.95f); return 1;
            case 15: st.master_hpf  = vn; break;
            case 16: st.master_tilt = vn * 2.0f - 1.0f; break;
            case 17: st.master_tape = vn; break;
            case 18: st.ext_src     = val % 3u; break;
            case 19: st.ext_in_lvl  = vn; break;
            case 20: st.tuning      = val % 5u; break;
            case 21: st.chord_mode  = val % 6u; break;
            case 22: helios_set_perf_mode(val % 3u); return 1;
            case 23: st.vcf_link    = val & 1u; break;
            case 24:
                st.playing = val & 1u;
                if (st.playing) { helios_set_state(&st); helios_seq_sync(); return 1; }
                break;
            case 25: st.evolve      = val & 3u; break;
            case 26: {
                static const char *const tn[5] = {"BLACK", "CREAM", "AMBER YEL", "COBALT", "OLIVE"};
                ui_theme = val % 5u;
                app_say("CHASSIS", tn[ui_theme]);
                return 1;
            }
            case 27: st.lfo_fast    = val & 1u; break;
            case 28: st.bpm         = (uint8_t)(30u + ((uint32_t)val * 210u) / 127u); break;
            case 29: helios_retune_drones_to_root(val % 12u); return 1;
            case 30: st.scale       = val % 7u; break;
            case 31: helios_set_arp_mode(val % 6u); return 1;
            case 32: st.cart_decay  = vn; break;
            case 33: st.chorus_mix  = vn; break;
            case 34: st.noise_lvl   = vn; break;
            case 35: st.noise_mode  = val % 3u; break;
            case 36: st.v45_xmod    = vn; break;
            case 37: st.sh_rate     = vn; break;
            case 38: st.slew_amt    = vn; break;
            case 39: st.vcf_lfo     = vn; break;
            case 40: st.vcf_mode[0] = val & 3u; if (st.vcf_link) st.vcf_mode[1] = val & 3u; break;
            case 41: for (uint32_t di = 0; di < HL_NDRONE; di++) st.drone[di].spread  = vn; break;
            case 42: for (uint32_t di = 0; di < HL_NDRONE; di++) st.drone[di].attack  = vn; break;
            case 43: for (uint32_t di = 0; di < HL_NDRONE; di++) st.drone[di].release = vn; break;
            case 44: for (uint32_t di = 0; di < HL_NDRONE; di++) st.drone[di].level   = vn; break;
            case 45: st.cart_mask   = (uint16_t)(1u << (st.cart_type & 15u)); break; /* Solo current cart */
            case 46: st.cart_mask   = 0u; break; /* Clear all stacked carts */
            case 47: hl_set_mode(val & 1u); hl_sysex_send_state(); return 1; /* 0=ENTROPY, 1=PURE HW */
            case 48: st.vcf_res[1]  = vn; st.vcf_link = 0u; break;
            case 49: st.vcf_mode[1] = val & 3u; st.vcf_link = 0u; break;
            case 50: helios_pin_preset_step(1); hl_sysex_send_state(); return 1;
            case 51:
                ui_usb_full = (val >= 1u) ? 1u : 0u;
                ui_spk_mute = (val == 2u) ? 1u : 0u;
                plat_usb_audio_full(ui_usb_full);
#ifndef PLAT_TICKS_PER_US
                plat_speaker(!ui_spk_mute);
#endif
                return 1;
            case 52: helios_cycle_joy_rec(); return 1;
            case 53: st.key_mode = val % 3u; break; /* 0=SPLIT 6+6, 1=DUAL 12, 2=POLY 2V */
            case 54: st.play_oct = (int8_t)clamp((int)val - 2, -2, 2); break;
            case 55: /* Reset active slot to clean baseline */
                if (ui_pure_mode) hl_load_pure_slot(cur_pure_idx);
                else              hl_load_slot(cur_slot_idx);
                hl_sysex_send_state();
                return 1;
            case 56: helios_toggle_compare(); return 1;
            case 57: helios_panic(); return 1;
            case 58: st.eq_lo_db  = vn * 24.0f - 12.0f; break;
            case 59: st.eq_mid_db = vn * 24.0f - 12.0f; break;
            case 60: st.eq_mid_hz = vn; break;
            case 61: st.eq_hi_db  = vn * 24.0f - 12.0f; break;
            case 62: st.eq_lo_hz  = vn; break;
            case 63: st.eq_mid_q  = vn; break;
            case 64: st.eq_hi_hz  = vn; break;
            case 65: st.eq_on     = val & 1u; break;
            case 66: helios_eq_reset(); hl_sysex_send_state(); return 1;
            case 67: helios_seq_sync(); return 1;
            case 70: ui_master_vol = vn; if (val > 0u) ui_master_mute = 0u; return 1;
            case 71: ui_master_mute = val & 1u; return 1;
            default: break;
            }
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x0Du && n >= 6u) {
            /* Exact 1-BPM Tempo Set: [4]=lo7, [5]=hi1 -> bpm = 30 + (lo7 | (hi1 << 7)) in 30..240 */
            int bpm = 30 + (int)((p[4] & 0x7Fu) | ((p[5] & 1u) << 7));
            helios_get_state(&st);
            st.bpm = (uint8_t)clamp(bpm, 30, 240);
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x0Au && n >= 7u) {
            /* Per-Drone Voice Control: [4]=di(0..5), [5]=sub(0..8), [6]=val7 */
            unsigned di = p[4] % HL_NDRONE;
            uint8_t sub = p[5] & 15u, val = p[6] & 0x7Fu;
            float vn = (float)val * (1.0f / 127.0f);
            helios_get_state(&st);
            switch (sub) {
            case 0: st.drone[di].hold       = val & 1u; break;
            case 1: st.drone[di].note       = (float)clamp(24 + (int)val, 24, 84); break;
            case 2: st.drone[di].spread     = vn; break;
            case 3: st.drone[di].level      = vn; break;
            case 4: st.drone[di].wave_morph = vn; break;
            case 5: st.drone[di].sub        = val & 1u; break;
            case 6: st.drone[di].pan        = (val == 0u) ? -1 : (val == 2u) ? 1 : 0; break;
            case 7: st.drone[di].attack     = vn; break;
            case 8: st.drone[di].glide      = vn; break;
            default: break;
            }
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x0Bu && n >= 7u) {
            /* Per-Melodic Voice (V4/V5) Control: [4]=mi(0..1), [5]=sub(0..7), [6]=val7 */
            unsigned mi = p[4] & 1u;
            uint8_t sub = p[5] & 7u, val = p[6] & 0x7Fu;
            float vn = (float)val * (1.0f / 127.0f);
            helios_get_state(&st);
            switch (sub) {
            case 0: st.mel[mi].wave    = val % 6u; break;
            case 1: st.mel[mi].level   = vn; break;
            case 2: st.mel[mi].glide   = vn; break;
            case 3: st.mel[mi].attack  = vn; break;
            case 4: st.mel[mi].release = vn; break;
            case 5: st.mel[mi].vibrato = vn; break;
            case 6: st.mel[mi].octave  = (int8_t)clamp((int)val - 2, -2, 2); break;
            case 7: st.mel[mi].hold    = val & 1u; break;
            default: break;
            }
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x0Cu && n >= 8u) {
            /* Dual 5-Step Sequencer Control: [4]=mi(0..1), [5]=step(0..4), [6]=sub(0..5), [7]=val7 */
            unsigned mi = p[4] & 1u, si = p[5] % HL_NSTEPS;
            uint8_t sub = p[6] & 7u, val = p[7] & 0x7Fu;
            helios_get_state(&st);
            switch (sub) {
            case 0: st.mel[mi].steps[si] = val % HL_NPLATES; break;
            case 1: st.mel[mi].step_gate[si] = val & 1u; break;
            case 2: st.mel[mi].step_prob[si] = (uint8_t)clamp((int)val, 1, 10); break;
            case 3: st.mel[mi].step_acc[si]  = val & 1u; break;
            case 4:
                if (val == 0u) { st.mel[mi].seq_run = 0u; st.mel[mi].hold = 0u; }
                else           { st.mel[mi].seq_run = 1u; st.mel[mi].hold = 1u; st.mel[mi].seq_div = (val - 1u) % 7u; }
                break;
            case 5: st.mel[mi].seq_len = (uint8_t)clamp((int)val, 2, (int)HL_NSTEPS); break;
            case 6: st.mel[mi].seq_phase = br_clampf((float)val * (1.0f / 120.0f), 0.0f, 1.0f); break;
            case 7: st.mel[mi].seq_drift = val & 1u; break;
            default: break;
            }
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x09u && n >= 6u) {
            /* Pure HW Bank Control: [4]=sub(0=LOAD H01..H16, 1=SAVE H01..H16), [5]=slot(0..15) */
            if (p[4] == 0u) hl_load_pure_slot(p[5] % HL_NPURE);
            else            hl_save_pure_slot(p[5] % HL_NPURE);
            hl_sysex_send_state();
            return 1;
        }
        if (cmd == 0x08u && n >= 7u) {
            /* Per-Cartridge Stack Control: [4]=cart_idx(0..15), [5]=subcmd(0=ON/OFF, 1=LVL, 2=TONE), [6]=val7 */
            uint8_t ci = p[4] & 15u, sub = p[5] & 3u, val = p[6] & 0x7Fu;
            helios_get_state(&st);
            st.cart_type = ci;
            if (sub == 0u) {
                if (val) st.cart_mask |= (uint16_t)(1u << ci);
                else     st.cart_mask &= (uint16_t)(~(1u << ci));
            } else if (sub == 1u) {
                st.cart_lvl[ci] = (float)val * (1.0f / 127.0f);
            } else if (sub == 2u) {
                st.cart_param[ci] = (float)val * (1.0f / 127.0f);
            }
            helios_set_state(&st);
            return 1;
        }
        if (cmd == 0x04u && n >= 5u) {
            /* Load Factory (0..35) or Entropy User (36..43) slot */
            hl_load_slot(p[4] % HL_NTOTAL_P);
            hl_sysex_send_state();
            return 1;
        }
        if (cmd == 0x05u && n >= 5u) {
            /* Save current sound into Entropy User Slot 0..7 in flash */
            hl_save_user_slot(p[4] % HL_NUSER);
            hl_sysex_send_state();
            return 1;
        }
        if (cmd == 0x06u && n >= 5u) {
            /* Dump any User/Pure Slot (0..7=E01..E08, 8..15=H01..H08, 16..23=H09..H16) for full JSON backup! */
            unsigned g = p[4] % 24u;
            const hl_user_bank_t *bk = (g < 8u) ? &user_bank : (g < 16u) ? &pure_bank_a : &pure_bank_b;
            unsigned sub = g & 7u;
            static uint8_t sbuf[8u + sizeof(hl_save_t) * 2u];
            const uint8_t *raw = (const uint8_t *)&bk->slot[sub];
            uint32_t i, q = 0u;
            sbuf[q++] = 0xF0u; sbuf[q++] = 0x7Du; sbuf[q++] = 0x48u; sbuf[q++] = 0x4Cu;
            sbuf[q++] = 0x16u; sbuf[q++] = (uint8_t)g; sbuf[q++] = bk->used[sub] & 1u;
            for (i = 0; i < sizeof(hl_save_t); i++) {
                sbuf[q++] = raw[i] & 0x0Fu;
                sbuf[q++] = (raw[i] >> 4) & 0x0Fu;
            }
            sbuf[q++] = 0xF7u;
            plat_sysex_send(sbuf, q);
            return 1;
        }
        if (cmd == 0x07u && n >= 5u + sizeof(hl_save_t) * 2u) {
            /* Restore any User/Pure Slot (0..7=E01..E08, 8..15=H01..H08, 16..23=H09..H16) into flash */
            unsigned g = p[4] % 24u;
            hl_user_bank_t *bk = (g < 8u) ? &user_bank : (g < 16u) ? &pure_bank_a : &pure_bank_b;
            uint32_t obj = (g < 8u) ? OBJ_USER_BANK : (g < 16u) ? OBJ_PURE_A : OBJ_PURE_B;
            unsigned sub = g & 7u;
            uint8_t *dst = (uint8_t *)&bk->slot[sub];
            uint32_t i;
            for (i = 0; i < sizeof(hl_save_t); i++)
                dst[i] = (uint8_t)((p[5u + i * 2u] & 0x0Fu) | ((p[6u + i * 2u] & 0x0Fu) << 4));
            if (bk->slot[sub].magic == HL_SAVE_MAGIC) {
                bk->magic = HL_BANK_MAGIC;
                bk->used[sub] = 1u;
                plat_store_save(obj, bk, sizeof(*bk));
                app_say("RESTORED", g < 8u ? "ENTROPY SLOT" : "PURE SLOT");
            }
            hl_sysex_send_state();
            return 1;
        }
    }
    return 0;
}

/* ---- USB / TRS MIDI Input Handler (Clock Sync, Full Expression, Studio CC Map) ---- */
static void hl_handle_midi(uint32_t pkt)
{
    uint8_t st_b = (uint8_t)((pkt >> 8) & 0xFFu);
    uint8_t d1   = (uint8_t)((pkt >> 16) & 0x7Fu);
    uint8_t d2   = (uint8_t)((pkt >> 24) & 0x7Fu);
    uint8_t cmd  = st_b & 0xF0u;
    uint8_t ch   = st_b & 0x0Fu;
    helios_state_t st;

    /* System Real-Time MIDI Clock Sync (0xF8 Clock, 0xFA Start, 0xFB Continue, 0xFC Stop) */
    if (st_b == 0xF8u) { helios_midi_clock(plat_ms()); return; }
    if (st_b == 0xFAu || st_b == 0xFBu) { helios_midi_start(); return; }
    if (st_b == 0xFCu) { helios_midi_stop(); return; }

    if (cmd == 0x90u && d2 > 0u) {
        float vel = (float)d2 * (1.0f / 127.0f);
        if (ch == 1u) {
            helios_touch_plate(0u, d1 % 12u, vel, vel);
        } else if (ch == 2u) {
            helios_touch_plate(1u, d1 % 12u, vel, vel);
        } else if (ch == 3u) {
            helios_retune_drones_to_root(d1 % 12u);
        } else {
            unsigned k = (d1 >= 53u) ? ((d1 - 53u) % HL_NKEYS) : (d1 % HL_NKEYS);
            helios_fm1_key_on(k, vel, vel);
        }
    } else if (cmd == 0x80u || (cmd == 0x90u && d2 == 0u)) {
        unsigned k = (d1 >= 53u) ? ((d1 - 53u) % HL_NKEYS) : (d1 % HL_NKEYS);
        helios_fm1_key_off(k);
    } else if (cmd == 0xA0u) {
        helios_midi_poly_at(d1, d2);
    } else if (cmd == 0xD0u) {
        helios_midi_chan_at(d1);
    } else if (cmd == 0xE0u) {
        int16_t pb = (int16_t)(((int)d2 << 7) | (int)d1) - 8192;
        helios_midi_pitch_bend(pb);
    } else if (cmd == 0xC0u) {
        hl_load_slot(d1 % HL_NTOTAL_P);
        helios_get_state(&st);
        app_say("PATCH", helios_patch_name(st.patch));
    } else if (cmd == 0xB0u) {
        float v = (float)d2 * (1.0f / 127.0f);
        helios_get_state(&st);
        switch (d1) {
        case 1:  if (st.scene_b_set) { helios_set_morph(v); return; } st.joy_y = v * 2.0f - 1.0f; break;
        case 7:  ui_master_vol = v; if (d2 > 0u) ui_master_mute = 0u; return;
        case 16: st.joy_x = v * 2.0f - 1.0f; break;
        case 17: st.joy_y = v * 2.0f - 1.0f; break;
        case 20: st.playing = (d2 >= 64u) ? 1u : 0u; break;
        case 21: helios_retune_drones_to_root((uint8_t)((d2 * 12u) / 128u)); return;
        case 22: hl_load_slot(((uint32_t)d2 * HL_NTOTAL_P) / 128u); return;
        case 24: st.mel[0].seq_phase = br_clampf((float)d2 * (1.0f / 120.0f), 0.0f, 1.0f); break;
        case 25: st.mel[1].seq_phase = br_clampf((float)d2 * (1.0f / 120.0f), 0.0f, 1.0f); break;
        case 30:
        case 74: st.vcf_cut[0] = v; if (st.vcf_link) st.vcf_cut[1] = v; break;
        case 75: st.vcf_cut[1] = v; st.vcf_link = 0u; break;
        case 31:
        case 71: st.vcf_res[0] = v; if (st.vcf_link) st.vcf_res[1] = v; break;
        case 72: st.vcf_drive  = v; break;
        case 36: st.evolve = (uint8_t)((d2 * 4u) / 128u); break;
        case 64: helios_midi_sustain_pedal(d2 >= 64u ? 1u : 0u); return;
        case 80: st.spring_mix = v; break;
        case 81: st.spring_decay = v; break;
        case 82: if (d2 >= 64u) helios_kick_spring(v); return;
        case 83: st.cart_freeze = (d2 >= 64u) ? 1u : 0u; break;
        case 84: st.cart_type = (uint8_t)((d2 * HL_NCART) / 128u); break;
        case 85: helios_set_morph(v); return;
        case 91: st.cart_mix = v; break;
        case 93: st.cart_shimmer = v; break;
        default: return;
        }
        helios_set_state(&st);
    }
}

/* ---- Boot & Audio Render Contract ---- */
void app_init(void)
{
    palette_set(APP_PALETTE);
    helios_init(44100.0f);
    touch_on(1);

#ifdef PLAT_TICKS_PER_US
#if FM1B_CPU2
    {
        int r = plat_cpu2_set(1);
        hl_cpu2_state = (r == 0 && plat_cpu2_on()) ? 1u : (plat_cpu2_barred() ? 3u : 2u);
    }
    if (hl_cpu2_state == 1u) {
        hl_probe_x = 2.0f;
        hl_probe_ok = 0u;
        if (plat_cpu2_run(hl_probe_u32, 0u) && plat_cpu2_wait() == 0 &&
            hl_probe_ok && hl_probe_y == 3.25f) {
            helios_profile_clock(plat_ticks24);
            helios_core2_hooks(plat_cpu2_run, plat_cpu2_wait);
            helios_core2_mode(1);
        } else {
            hl_cpu2_state = 4u;
            helios_core2_mode(0);
        }
    }
#else
    hl_cpu2_state = 3u;
    helios_profile_clock(plat_ticks24);
    helios_core2_mode(0);
#endif
#elif !defined(PLAT_HOST)
    {
        int r = plat_cpu2_start(500u);
        hl_cpu2_state = (r == 0) ? 1u : (r == -2) ? 3u : 2u;
    }
    if (hl_cpu2_state == 1u) {
        uint32_t t0 = plat_ticks();
        hl_probe_x = 2.0f;
        plat_cpu2_job(hl_probe_fn);
        while (!hl_probe_ok && plat_ticks() - t0 < 50000u * PLAT_TICKS_US)
            ;
        plat_cpu2_job(0);
        if (hl_probe_ok && hl_probe_y == 3.25f) {
            helios_profile_clock(plat_ticks);
            helios_core2_mode(1);
            plat_cpu2_job(helios_core2_work);
        } else {
            hl_cpu2_state = 4u;
        }
    }
#else
    /* On host simulator, run deterministic Dual-Core Mode 2 so both Core 0 & Core 1 code paths are verified */
    hl_cpu2_state = 1u;
    helios_profile_clock(plat_ticks);
    helios_core2_mode(2);
#endif

    hl_load_saved();
    app_say("ENTROPY HELIOS", "V" APP_VERSION " 2-CORE");
    notice_until = plat_ms() + 3000u;
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    float g = ui_master_mute ? 0.0f : ((float)master_q12 * (1.0f / 4096.0f) * ui_master_vol);
    helios_render(lr, n, g);
}

void app_panic(void)
{
    helios_panic();
}

void app_overload(void)
{
    overloads_cnt++;
}

uint32_t app_log_word(void)
{
    uint32_t vj, fxj, lt, ls, c1p;
    int md;
    helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
    return ((uint32_t)(md & 3) << 28) | ((c1p & 0xFFu) << 16) | (overloads_cnt & 0xFFFFu);
}

int app_log_line(unsigned k, char *b)
{
    uint32_t vj, fxj, lt, ls, c1p;
    int md;
    if (k > 1u) return 0;
    helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
    b[0] = 0;
    if (k == 0u) {
        hl_cat(b, hl_cpu2_state == 1u ? "cpu1 UP mode " : "cpu1 FALLBACK mode ");
        hl_cat_u(b, (unsigned)md);
        hl_cat(b, " load ");
        hl_cat_u(b, c1p);
#ifdef PLAT_TICKS_PER_US
        hl_cat(b, "% stk ");
#if FM1B_CPU2
        hl_cat_u(b, plat_cpu2_stack());
#else
        hl_cat_u(b, 0u);
#endif
#else
        hl_cat(b, "% beat ");
        hl_cat_u(b, plat_cpu2_beat());
#endif
        return 1;
    }
    hl_cat(b, "c1 vjobs ");
    hl_cat_u(b, vj);
    hl_cat(b, " fxjobs ");
    hl_cat_u(b, fxj);
    hl_cat(b, " late ");
    hl_cat_u(b, lt);
    hl_cat(b, " lost ");
    hl_cat_u(b, ls);
    return 1;
}

#ifdef PLAT_TICKS_PER_US
void app_log(void (*line)(const char *t))
{
    char b[48];
    if (app_log_line(0u, b)) line(b);
    if (app_log_line(1u, b)) line(b);
}
#endif

/* ---- Parameter Adjust & Readout for the 4 Physical Encoders (K1 .. K4) on Every Page ---- */
static void hl_knob_info(const helios_state_t *st, unsigned page, unsigned knob,
                          char *lbl, char *val, float *norm)
{
    static const char *const DIV_TXT[7] = {"1/1", "1/2", "1/4", "1/8", "REV", "PEND", "RND"};
    static const char *const VMODE_TXT[4] = {"LP 12", "BP VOC", "HP 12", "LP+BP"};
    static const char *const NMODE_TXT[3] = {"PINK", "GEIGER", "RUMBLE"};
    static const char *const PIN_PRE_TXT[5] = {"CLASSIC", "DRIFT", "S&H STP", "ENV FOL", "LYRA XM"};
    static const char *const JOY_REC_TXT[3] = {"MANUAL", "REC 8S", "LOOPING"};
    lbl[0] = 0;
    val[0] = 0;
    *norm = 0.0f;

    switch (page) {
    case PG_HOME:
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "A/B MRPH");
                if (st->scene_b_set) hl_fmt_pct(val, st->morph_pos);
                else                 hl_cat(val, "SET B K3");
                *norm = st->morph_pos;
            } else {
                hl_cat(lbl, "VCF CUT");
                hl_fmt_pct(val, st->vcf_cut[0]);
                *norm = st->vcf_cut[0];
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                static const char *const SCP_TXT[3] = {"WAVE", "LISSAJOU", "SPECTRUM"};
                hl_cat(lbl, "ANALYZER");
                hl_cat(val, SCP_TXT[ui_scope_mode % 3u]);
                *norm = (float)(ui_scope_mode % 3u) * 0.5f;
            } else {
                hl_cat(lbl, "VCF RES");
                hl_fmt_pct(val, st->vcf_res[0]);
                *norm = st->vcf_res[0];
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "SCENE B");
                hl_cat(val, st->compare_on ? "COMPARE" : st->scene_b_set ? "B READY" : "TURN=CAP");
                *norm = st->scene_b_set ? 1.0f : 0.0f;
            } else {
                hl_cat(lbl, "CART MIX");
                hl_fmt_pct(val, st->cart_mix);
                *norm = st->cart_mix;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "JOY REC");
                hl_cat(val, JOY_REC_TXT[st->joy_rec_state % 3u]);
                *norm = (float)(st->joy_rec_state % 3u) * 0.5f;
            } else {
                hl_cat(lbl, "JOY X");
                hl_cat_i(val, (int)(st->joy_x * 100.0f));
                *norm = (st->joy_x + 1.0f) * 0.5f;
            }
        }
        break;

    case PG_DRN_1_3:
    case PG_DRN_6_8: {
        unsigned di = (page == PG_DRN_1_3) ? (ui_sel_drone_l % 3u) : (3u + (ui_sel_drone_r % 3u));
        const hl_drone_cfg_t *d = &st->drone[di];
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "WAV MRPH");
                hl_fmt_pct(val, d->wave_morph);
                *norm = d->wave_morph;
            } else {
                hl_cat(lbl, "PITCH");
                hl_fmt_note(val, d->note);
                *norm = (d->note - 24.0f) * (1.0f / 60.0f);
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "DRN HOLD");
                hl_cat(val, d->hold ? "HOLD ON" : "MUTED");
                *norm = d->hold ? 1.0f : 0.0f;
            } else {
                hl_cat(lbl, "SPREAD");
                hl_fmt_pct(val, d->spread);
                *norm = d->spread;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "ATTACK");
                hl_fmt_pct(val, d->attack);
                *norm = d->attack;
            } else {
                hl_cat(lbl, "LEVEL");
                hl_fmt_pct(val, d->level);
                *norm = d->level;
            }
        } else {
            hl_cat(lbl, ui_sel_alt ? "PAN" : "SUB OCT");
            if (ui_sel_alt) {
                hl_cat(val, d->pan < 0 ? "LEFT" : d->pan > 0 ? "RIGHT" : "BOTH");
                *norm = (float)(d->pan + 1) * 0.5f;
            } else {
                hl_cat(val, d->sub ? "-1 OCT" : "UNISON");
                *norm = d->sub ? 1.0f : 0.0f;
            }
        }
        break;
    }

    case PG_DRN_ALL:
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "CART TON");
                hl_fmt_pct(val, st->cart_tone);
                *norm = st->cart_tone;
            } else {
                hl_cat(lbl, "ALL SPRD");
                hl_fmt_pct(val, st->drone[0].spread);
                *norm = st->drone[0].spread;
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "SPR DCY");
                hl_fmt_pct(val, st->spring_decay);
                *norm = st->spring_decay;
            } else {
                hl_cat(lbl, "ALL ATK");
                hl_fmt_pct(val, st->drone[0].attack);
                *norm = st->drone[0].attack;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "VCF LFO");
                hl_fmt_pct(val, st->vcf_lfo);
                *norm = st->vcf_lfo;
            } else {
                hl_cat(lbl, "ALL REL");
                hl_fmt_pct(val, st->drone[0].release);
                *norm = st->drone[0].release;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "JOY Y");
                hl_cat_i(val, (int)(st->joy_y * 100.0f));
                *norm = (st->joy_y + 1.0f) * 0.5f;
            } else {
                hl_cat(lbl, "ALL LVL");
                hl_fmt_pct(val, st->drone[0].level);
                *norm = st->drone[0].level;
            }
        }
        break;

    case PG_MEL_V4:
    case PG_MEL_V5: {
        unsigned mi = (page == PG_MEL_V5) ? 1u : 0u;
        const hl_mel_cfg_t *m = &st->mel[mi];
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "LEVEL");
                hl_fmt_pct(val, m->level);
                *norm = m->level;
            } else {
                hl_cat(lbl, "WAVE");
                hl_cat(val, helios_wave_name(m->wave));
                *norm = (float)m->wave * 0.2f;
            }
        } else if (knob == 1u) {
            if (st->perf_mode != HL_PERF_GEN && !ui_sel_alt) {
                hl_cat(lbl, "ARP MODE");
                hl_cat(val, helios_arpmode_name(st->arp_mode));
                *norm = (float)(st->arp_mode % 6u) * 0.2f;
            } else {
                int c = (int)(st->plate_trim[m->cur_plate % HL_NPLATES] * 100.0f);
                hl_cat(lbl, "PLT TRIM");
                hl_cat_i(val, c);
                hl_cat(val, "c");
                *norm = (st->plate_trim[m->cur_plate % HL_NPLATES] + 1.0f) * 0.5f;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "ATTACK");
                hl_fmt_pct(val, m->attack);
                *norm = m->attack;
            } else {
                hl_cat(lbl, "GLIDE");
                hl_fmt_pct(val, m->glide);
                *norm = m->glide;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "RELEASE");
                hl_fmt_pct(val, m->release);
                *norm = m->release;
            } else if (mi == 0u) {
                hl_cat(lbl, "V4<>5 XM");
                hl_fmt_pct(val, st->v45_xmod);
                *norm = st->v45_xmod;
            } else {
                hl_cat(lbl, "VIBRATO");
                hl_fmt_pct(val, m->vibrato);
                *norm = m->vibrato;
            }
        }
        break;
    }

    case PG_SEQ_L:
    case PG_SEQ_R: {
        unsigned mi = (page == PG_SEQ_R) ? 1u : 0u;
        unsigned si = (mi == 0u ? ui_sel_step_l : ui_sel_step_r) % HL_NSTEPS;
        const hl_mel_cfg_t *m = &st->mel[mi];
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "SEQ LEN");
                hl_cat_u(val, (unsigned)(m->seq_len ? m->seq_len : HL_NSTEPS));
                hl_cat(val, " STPS");
                *norm = (float)((m->seq_len ? m->seq_len : HL_NSTEPS) - 2u) * (1.0f / 3.0f);
            } else {
                float sn = hl_plate_to_note(st->root, st->scale, m->steps[si], m->octave);
                hl_cat(lbl, "STP NOTE");
                hl_fmt_note(val, sn);
                *norm = (float)m->steps[si] / 11.0f;
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                uint8_t pr = m->step_prob[si] ? m->step_prob[si] : 10u;
                hl_cat(lbl, "STP PROB");
                hl_cat_u(val, (unsigned)pr * 10u);
                hl_cat(val, "%");
                *norm = (float)pr * 0.1f;
            } else {
                hl_cat(lbl, "STP GATE");
                hl_cat(val, m->step_gate[si] ? "GATE ON" : "REST");
                *norm = m->step_gate[si] ? 1.0f : 0.0f;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "STP ACC");
                hl_cat(val, m->step_acc[si] ? "ACCENT" : "NORMAL");
                *norm = m->step_acc[si] ? 1.0f : 0.0f;
            } else {
                hl_cat(lbl, "CLK DIV");
                hl_cat(val, m->seq_run ? DIV_TXT[m->seq_div % 7u] : "MANUAL");
                *norm = m->seq_run ? ((float)((m->seq_div % 7u) + 1u) * (1.0f / 7.0f)) : 0.0f;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "SEQ PHS");
                if (m->seq_drift) {
                    hl_cat(val, "DRIFT");
                    *norm = 1.0f;
                } else {
                    unsigned deg = (unsigned)(m->seq_phase * 360.0f + 0.5f);
                    hl_cat_u(val, deg);
                    hl_cat(val, " DEG");
                    *norm = m->seq_phase;
                }
            } else {
                hl_cat(lbl, "OCTAVE");
                hl_cat_i(val, (int)m->octave);
                *norm = (float)(m->octave + 2) * 0.25f;
            }
        }
        break;
    }

    case PG_VCF:
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "CUT R");
                hl_fmt_pct(val, st->vcf_cut[1]);
                *norm = st->vcf_cut[1];
            } else {
                hl_cat(lbl, st->vcf_link ? "CUT L/R" : "CUT L");
                hl_fmt_pct(val, st->vcf_cut[0]);
                *norm = st->vcf_cut[0];
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "RES R");
                hl_fmt_pct(val, st->vcf_res[1]);
                *norm = st->vcf_res[1];
            } else {
                hl_cat(lbl, st->vcf_link ? "RES L/R" : "RES L");
                hl_fmt_pct(val, st->vcf_res[0]);
                *norm = st->vcf_res[0];
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "L/R LINK");
                hl_cat(val, st->vcf_link ? "LINKED" : "INDEP");
                *norm = st->vcf_link ? 1.0f : 0.0f;
            } else {
                hl_cat(lbl, "OP DRIVE");
                hl_fmt_pct(val, st->vcf_drive);
                *norm = st->vcf_drive;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "MODE R");
                hl_cat(val, VMODE_TXT[st->vcf_mode[1] & 3u]);
                *norm = (float)(st->vcf_mode[1] & 3u) * (1.0f / 3.0f);
            } else {
                hl_cat(lbl, st->vcf_link ? "VCF MODE" : "MODE L");
                hl_cat(val, VMODE_TXT[st->vcf_mode[0] & 3u]);
                *norm = (float)(st->vcf_mode[0] & 3u) * (1.0f / 3.0f);
            }
        }
        break;

    case PG_CART:
        {
            unsigned ci = st->cart_type % HL_NCART;
            if (knob == 0u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "SPRING");
                    hl_fmt_pct(val, st->spring_mix);
                    *norm = st->spring_mix;
                } else {
                    hl_cat(lbl, "CART SEL");
                    hl_cat(val, helios_cart_name(ci));
                    *norm = (float)ci / (float)(HL_NCART - 1u);
                }
            } else if (knob == 1u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "SHIMMER");
                    hl_fmt_pct(val, st->cart_shimmer);
                    *norm = st->cart_shimmer;
                } else {
                    int loaded = (st->cart_mask & (1u << (ci & 15u))) != 0u;
                    hl_cat(lbl, "CART ON");
                    hl_cat(val, loaded ? "LOADED" : "OFF");
                    *norm = loaded ? 1.0f : 0.0f;
                }
            } else if (knob == 2u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "DECAY");
                    hl_fmt_pct(val, st->cart_decay);
                    *norm = st->cart_decay;
                } else {
                    hl_cat(lbl, "CART LVL");
                    hl_fmt_pct(val, st->cart_lvl[ci]);
                    *norm = st->cart_lvl[ci];
                }
            } else {
                if (ui_sel_alt) {
                    hl_cat(lbl, "FREEZE");
                    hl_cat(val, st->cart_freeze ? "FROZEN" : "LIVE");
                    *norm = st->cart_freeze ? 1.0f : 0.0f;
                } else {
                    hl_cat(lbl, "CART TON");
                    hl_fmt_pct(val, st->cart_param[ci]);
                    *norm = st->cart_param[ci];
                }
            }
        }
        break;

    case PG_PREAMP:
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "EXT SRC");
                hl_cat(val, helios_extsrc_name(st->ext_src));
                *norm = (float)(st->ext_src % 3u) * 0.5f;
            } else {
                hl_cat(lbl, "EXT IN");
                hl_fmt_pct(val, st->ext_in_lvl);
                *norm = st->ext_in_lvl;
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "SUB HPF");
                hl_fmt_pct(val, st->master_hpf);
                *norm = st->master_hpf;
            } else {
                hl_cat(lbl, "NOISE");
                hl_fmt_pct(val, st->noise_lvl);
                *norm = st->noise_lvl;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "TILT EQ");
                hl_cat_i(val, (int)(st->master_tilt * 100.0f));
                *norm = (st->master_tilt + 1.0f) * 0.5f;
            } else {
                hl_cat(lbl, "NS MODE");
                hl_cat(val, NMODE_TXT[st->noise_mode % 3u]);
                *norm = (float)(st->noise_mode % 3u) * 0.5f;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "TAPE SAT");
                hl_fmt_pct(val, st->master_tape);
                *norm = st->master_tape;
            } else {
                hl_cat(lbl, "BBD CHOR");
                hl_fmt_pct(val, st->chorus_mix);
                *norm = st->chorus_mix;
            }
        }
        break;

    case PG_EQ:
        if (knob == 0u) {
            if (ui_sel_alt) {
                uint32_t hz = (uint32_t)(50.0f * fm_powf(6.0f, br_clampf(st->eq_lo_hz, 0.0f, 1.0f)) + 0.5f);
                hl_cat(lbl, "LOW FRQ");
                hl_cat_u(val, hz); hl_cat(val, "Hz");
                *norm = st->eq_lo_hz;
            } else {
                int db = (int)(st->eq_lo_db + (st->eq_lo_db >= 0.0f ? 0.5f : -0.5f));
                hl_cat(lbl, "LOW GAIN");
                hl_cat_i(val, db); hl_cat(val, "dB");
                *norm = (st->eq_lo_db + 12.0f) * (1.0f / 24.0f);
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                uint32_t q10 = (uint32_t)(5.0f * fm_powf(8.0f, br_clampf(st->eq_mid_q, 0.0f, 1.0f)) + 0.5f);
                hl_cat(lbl, "MID Q");
                hl_cat_u(val, q10 / 10u); hl_cat(val, "."); hl_cat_u(val, q10 % 10u);
                *norm = st->eq_mid_q;
            } else {
                int db = (int)(st->eq_mid_db + (st->eq_mid_db >= 0.0f ? 0.5f : -0.5f));
                hl_cat(lbl, "MID GAIN");
                hl_cat_i(val, db); hl_cat(val, "dB");
                *norm = (st->eq_mid_db + 12.0f) * (1.0f / 24.0f);
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                uint32_t hz = (uint32_t)(2500.0f * fm_powf(4.0f, br_clampf(st->eq_hi_hz, 0.0f, 1.0f)) + 0.5f);
                hl_cat(lbl, "HI FRQ");
                hl_cat_u(val, hz / 1000u); hl_cat(val, "."); hl_cat_u(val, (hz % 1000u) / 100u); hl_cat(val, "k");
                *norm = st->eq_hi_hz;
            } else {
                uint32_t hz = (uint32_t)(180.0f * fm_powf(25.0f, br_clampf(st->eq_mid_hz, 0.0f, 1.0f)) + 0.5f);
                hl_cat(lbl, "MID FRQ");
                if (hz >= 1000u) { hl_cat_u(val, hz / 1000u); hl_cat(val, "."); hl_cat_u(val, (hz % 1000u) / 100u); hl_cat(val, "k"); }
                else             { hl_cat_u(val, hz); hl_cat(val, "Hz"); }
                *norm = st->eq_mid_hz;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "EQ STATE");
                hl_cat(val, st->eq_on ? "ACTIVE" : "BYPASS");
                *norm = st->eq_on ? 1.0f : 0.0f;
            } else {
                int db = (int)(st->eq_hi_db + (st->eq_hi_db >= 0.0f ? 0.5f : -0.5f));
                hl_cat(lbl, "HI GAIN");
                hl_cat_i(val, db); hl_cat(val, "dB");
                *norm = (st->eq_hi_db + 12.0f) * (1.0f / 24.0f);
            }
        }
        break;

    case PG_MATRIX:
        if (knob == 0u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "SLEW AMT");
                hl_fmt_pct(val, st->slew_amt);
                *norm = st->slew_amt;
            } else {
                hl_cat(lbl, "SRC ROW");
                hl_cat(val, helios_mod_src_name(ui_pin_r));
                *norm = (float)ui_pin_r / 7.0f;
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "VCF LFO");
                hl_fmt_pct(val, st->vcf_lfo);
                *norm = st->vcf_lfo;
            } else {
                hl_cat(lbl, "DST COL");
                hl_cat(val, helios_mod_dst_name(ui_pin_c));
                *norm = (float)ui_pin_c / 7.0f;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                hl_cat(lbl, "JOY Y");
                hl_cat_i(val, (int)(st->joy_y * 100.0f));
                *norm = (st->joy_y + 1.0f) * 0.5f;
            } else {
                int8_t pin = st->pin_matrix[ui_pin_r % 8u][ui_pin_c % 8u];
                hl_cat(lbl, "PIN JACK");
                hl_cat(val, pin == 1 ? "+50 GOLD" : pin == 2 ? "+100 COP" : pin < 0 ? "-75 CYAN" : "EMPTY");
                *norm = pin == 0 ? 0.0f : pin == 1 ? 0.5f : pin == 2 ? 1.0f : 0.75f;
            }
        } else {
            if (ui_sel_alt) {
                hl_cat(lbl, "MAT PRES");
                hl_cat(val, PIN_PRE_TXT[st->pin_preset % 5u]);
                *norm = (float)(st->pin_preset % 5u) * 0.25f;
            } else {
                hl_cat(lbl, "S&H RATE");
                hl_fmt_pct(val, st->sh_rate);
                *norm = st->sh_rate;
            }
        }
        break;

    case PG_GLOBAL:
    default:
        {
            static const char *const TH_NAME[5] = {"BLACK", "CREAM", "AMBER YEL", "COBALT", "OLIVE"};
            if (knob == 0u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "TUNING");
                    hl_cat(val, helios_tuning_name(st->tuning));
                    *norm = (float)(st->tuning % 5u) * 0.25f;
                } else {
                    hl_cat(lbl, "CHASSIS");
                    hl_cat(val, TH_NAME[ui_theme % 5u]);
                    *norm = (float)(ui_theme % 5u) * 0.25f;
                }
            } else if (knob == 1u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "CHORD M");
                    hl_cat(val, helios_chord_name(st->chord_mode));
                    *norm = (float)(st->chord_mode % 6u) * 0.2f;
                } else {
                    hl_cat(lbl, "SCALE");
                    hl_cat(val, helios_scale_name(st->scale));
                    *norm = (float)(st->scale % 7u) * (1.0f / 6.0f);
                }
            } else if (knob == 2u) {
                if (ui_sel_alt) {
                    hl_cat(lbl, "LFO SPD");
                    hl_cat(val, st->lfo_fast ? "AUDIO RT" : "SLOW AMB");
                    *norm = st->lfo_fast ? 1.0f : 0.0f;
                } else {
                    hl_cat(lbl, "ROOT KEY");
                    hl_cat(val, NOTE_NAMES[st->root % 12u]);
                    *norm = (float)(st->root % 12u) / 11.0f;
                }
            } else {
                if (ui_sel_alt) {
                    hl_cat(lbl, "USB REC");
                    hl_cat(val, ui_spk_mute ? "SPK MUTE" : ui_usb_full ? "0dB FULL" : "NORMAL");
                    *norm = ui_spk_mute ? 1.0f : ui_usb_full ? 0.5f : 0.0f;
                } else {
                    hl_cat(lbl, st->ext_clk_lock ? "EXT CLK" : "TEMPO");
                    hl_cat_u(val, (unsigned)st->bpm);
                    hl_cat(val, " BPM");
                    *norm = (float)(st->bpm - 30u) * (1.0f / 210.0f);
                }
            }
        }
        break;
    }
}

static void hl_turn_knob(unsigned page, unsigned knob, int32_t d)
{
    helios_state_t st;
    float step = (float)d * 0.04f;
    uint32_t i;
    char lbl[16], val[20];
    float norm;

    helios_get_state(&st);
    ui_act_knob  = (uint8_t)(knob & 3u);
    ui_act_until = plat_ms() + 900u;
    switch (page) {
    case PG_HOME:
        if (knob == 0u) {
            if (ui_sel_alt) {
                helios_set_morph(st.morph_pos + step);
                helios_get_state(&st);
            } else {
                st.vcf_cut[0] = br_clampf(st.vcf_cut[0] + step, 0.0f, 1.0f);
                if (st.vcf_link) st.vcf_cut[1] = st.vcf_cut[0];
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                ui_scope_mode = (uint8_t)(((int)ui_scope_mode + 30 + (int)d) % 3);
                helios_set_spec_en(ui_scope_mode == 2u);
            } else {
                st.vcf_res[0] = br_clampf(st.vcf_res[0] + step, 0.0f, 1.0f);
                if (st.vcf_link) st.vcf_res[1] = st.vcf_res[0];
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                if (d > 0) helios_capture_scene_b();
                else       helios_toggle_compare();
                helios_get_state(&st);
            } else {
                st.cart_mix = br_clampf(st.cart_mix + step, 0.0f, 1.0f);
            }
        } else {
            if (ui_sel_alt) {
                helios_cycle_joy_rec();
                helios_get_state(&st);
            } else {
                st.joy_x = br_clampf(st.joy_x + step * 1.5f, -1.0f, 1.0f);
            }
        }
        helios_set_state(&st);
        break;

    case PG_DRN_1_3:
    case PG_DRN_6_8: {
        unsigned di = (page == PG_DRN_1_3) ? (ui_sel_drone_l % 3u) : (3u + (ui_sel_drone_r % 3u));
        hl_drone_cfg_t *dr = &st.drone[di];
        if (knob == 0u) {
            if (ui_sel_alt) dr->wave_morph = br_clampf(dr->wave_morph + step, 0.0f, 1.0f);
            else            dr->note = br_clampf(dr->note + (float)d, 24.0f, 84.0f);
        } else if (knob == 1u) {
            if (ui_sel_alt) dr->hold ^= 1u;
            else            dr->spread = br_clampf(dr->spread + step, 0.0f, 1.0f);
        } else if (knob == 2u) {
            if (ui_sel_alt) dr->attack = br_clampf(dr->attack + step, 0.0f, 1.0f);
            else            dr->level  = br_clampf(dr->level + step, 0.0f, 1.0f);
        } else {
            if (ui_sel_alt) dr->pan = (int8_t)clamp((int32_t)dr->pan + d, -1, 1);
            else            dr->sub = (uint8_t)((dr->sub + 1u) & 1u);
        }
        helios_set_state(&st);
        break;
    }

    case PG_DRN_ALL:
        if (ui_sel_alt) {
            if (knob == 0u)      st.cart_tone    = br_clampf(st.cart_tone + step, 0.0f, 1.0f);
            else if (knob == 1u) st.spring_decay = br_clampf(st.spring_decay + step, 0.0f, 1.0f);
            else if (knob == 2u) st.vcf_lfo      = br_clampf(st.vcf_lfo + step, 0.0f, 1.0f);
            else                 st.joy_y        = br_clampf(st.joy_y + step * 1.5f, -1.0f, 1.0f);
        } else {
            for (i = 0; i < HL_NDRONE; i++) {
                if (knob == 0u)      st.drone[i].spread  = br_clampf(st.drone[i].spread + step, 0.0f, 1.0f);
                else if (knob == 1u) st.drone[i].attack  = br_clampf(st.drone[i].attack + step, 0.0f, 1.0f);
                else if (knob == 2u) st.drone[i].release = br_clampf(st.drone[i].release + step, 0.0f, 1.0f);
                else                 st.drone[i].level   = br_clampf(st.drone[i].level + step, 0.0f, 1.0f);
            }
        }
        helios_set_state(&st);
        break;

    case PG_MEL_V4:
    case PG_MEL_V5: {
        unsigned mi = (page == PG_MEL_V5) ? 1u : 0u;
        hl_mel_cfg_t *m = &st.mel[mi];
        if (knob == 0u) {
            if (ui_sel_alt) m->level = br_clampf(m->level + step, 0.0f, 1.0f);
            else            m->wave  = (uint8_t)(((int)m->wave + 60 + (int)d) % 6);
        } else if (knob == 1u) {
            if (st.perf_mode != HL_PERF_GEN && !ui_sel_alt) {
                helios_set_arp_mode((uint8_t)(((int)st.arp_mode + 60 + (int)d) % 6));
                helios_get_state(&st);
            } else {
                unsigned pl = m->cur_plate % HL_NPLATES;
                st.plate_trim[pl] = br_clampf(st.plate_trim[pl] + (float)d * 0.04f, -1.0f, 1.0f);
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) m->attack = br_clampf(m->attack + step, 0.0f, 1.0f);
            else            m->glide  = br_clampf(m->glide + step, 0.0f, 1.0f);
        } else {
            if (ui_sel_alt) m->release = br_clampf(m->release + step, 0.0f, 1.0f);
            else if (mi == 0u) st.v45_xmod = br_clampf(st.v45_xmod + step, 0.0f, 1.0f);
            else               m->vibrato  = br_clampf(m->vibrato + step, 0.0f, 1.0f);
        }
        helios_set_state(&st);
        break;
    }

    case PG_SEQ_L:
    case PG_SEQ_R: {
        unsigned mi = (page == PG_SEQ_R) ? 1u : 0u;
        unsigned si = (mi == 0u ? ui_sel_step_l : ui_sel_step_r) % HL_NSTEPS;
        hl_mel_cfg_t *m = &st.mel[mi];
        if (knob == 0u) {
            if (ui_sel_alt) {
                int nl = (int)(m->seq_len ? m->seq_len : HL_NSTEPS) + (int)d;
                if (nl < 2) nl = 2;
                if (nl > (int)HL_NSTEPS) nl = (int)HL_NSTEPS;
                m->seq_len = (uint8_t)nl;
            } else {
                m->steps[si] = (uint8_t)(((int)m->steps[si] + 120 + (int)d) % 12);
                if (m->cur_step == si && m->step_gate[si])
                    m->cur_plate = m->steps[si];
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                int np = (int)(m->step_prob[si] ? m->step_prob[si] : 10u) + (int)d;
                if (np < 1) np = 1;
                if (np > 10) np = 10;
                m->step_prob[si] = (uint8_t)np;
            } else {
                m->step_gate[si] ^= 1u;
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                m->step_acc[si] ^= 1u;
            } else {
                int cur = m->seq_run ? ((int)(m->seq_div % 7u) + 1) : 0;
                int nxt = (cur + 80 + (int)d) % 8;
                if (nxt == 0) {
                    m->seq_run = 0u;
                    m->hold    = 0u;
                } else {
                    m->seq_run = 1u;
                    m->hold    = 1u;
                    m->seq_div = (uint8_t)(nxt - 1);
                }
            }
        } else {
            if (ui_sel_alt) {
                int cur_s = m->seq_drift ? 24 : (int)(m->seq_phase * 24.0f + 0.5f);
                int nxt_s = cur_s + (int)d;
                if (nxt_s < 0) nxt_s = 0;
                if (nxt_s > 24) nxt_s = 24;
                if (nxt_s == 24) {
                    m->seq_drift = 1u;
                } else {
                    m->seq_drift = 0u;
                    m->seq_phase = (float)nxt_s * (1.0f / 24.0f);
                }
            } else {
                m->octave = (int8_t)clamp((int32_t)m->octave + d, -2, 2);
            }
        }
        helios_set_state(&st);
        break;
    }

    case PG_VCF:
        if (knob == 0u) {
            if (ui_sel_alt) {
                st.vcf_cut[1] = br_clampf(st.vcf_cut[1] + step, 0.0f, 1.0f);
                st.vcf_link = 0u;
            } else {
                st.vcf_cut[0] = br_clampf(st.vcf_cut[0] + step, 0.0f, 1.0f);
                if (st.vcf_link) st.vcf_cut[1] = st.vcf_cut[0];
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                st.vcf_res[1] = br_clampf(st.vcf_res[1] + step, 0.0f, 1.0f);
                st.vcf_link = 0u;
            } else {
                st.vcf_res[0] = br_clampf(st.vcf_res[0] + step, 0.0f, 1.0f);
                if (st.vcf_link) st.vcf_res[1] = st.vcf_res[0];
            }
        } else if (knob == 2u) {
            if (ui_sel_alt) st.vcf_link ^= 1u;
            else            st.vcf_drive = br_clampf(st.vcf_drive + step, 0.0f, 1.0f);
        } else {
            if (ui_sel_alt) {
                st.vcf_mode[1] = (uint8_t)(((int)st.vcf_mode[1] + 40 + (int)d) & 3);
                st.vcf_link = 0u;
            } else {
                uint8_t md = (uint8_t)(((int)st.vcf_mode[0] + 40 + (int)d) & 3);
                st.vcf_mode[0] = md;
                if (st.vcf_link) st.vcf_mode[1] = md;
            }
        }
        helios_set_state(&st);
        break;

    case PG_CART:
        {
            unsigned ci = st.cart_type % HL_NCART;
            if (knob == 0u) {
                if (ui_sel_alt) st.spring_mix = br_clampf(st.spring_mix + step, 0.0f, 1.0f);
                else            st.cart_type  = (uint8_t)(((int)st.cart_type + (int)HL_NCART * 16 + (int)d) % (int)HL_NCART);
            } else if (knob == 1u) {
                if (ui_sel_alt) {
                    st.cart_shimmer = br_clampf(st.cart_shimmer + step, 0.0f, 1.0f);
                } else {
                    uint16_t bit = (uint16_t)(1u << (ci & 15u));
                    if (d > 0) st.cart_mask |= bit;
                    else       st.cart_mask &= (uint16_t)(~bit);
                }
            } else if (knob == 2u) {
                if (ui_sel_alt) st.cart_decay   = br_clampf(st.cart_decay + step, 0.0f, 1.0f);
                else            st.cart_lvl[ci] = br_clampf(st.cart_lvl[ci] + step, 0.0f, 1.0f);
            } else {
                if (ui_sel_alt) st.cart_freeze   ^= 1u;
                else            st.cart_param[ci] = br_clampf(st.cart_param[ci] + step, 0.0f, 1.0f);
            }
        }
        helios_set_state(&st);
        break;

    case PG_PREAMP:
        if (knob == 0u) {
            if (ui_sel_alt) st.ext_src    = (uint8_t)(((int)st.ext_src + 30 + (int)d) % 3);
            else            st.ext_in_lvl = br_clampf(st.ext_in_lvl + step, 0.0f, 1.0f);
        } else if (knob == 1u) {
            if (ui_sel_alt) st.master_hpf = br_clampf(st.master_hpf + step, 0.0f, 1.0f);
            else            st.noise_lvl  = br_clampf(st.noise_lvl + step, 0.0f, 1.0f);
        } else if (knob == 2u) {
            if (ui_sel_alt) st.master_tilt = br_clampf(st.master_tilt + step * 1.5f, -1.0f, 1.0f);
            else            st.noise_mode  = (uint8_t)(((int)st.noise_mode + 30 + (int)d) % 3);
        } else {
            if (ui_sel_alt) st.master_tape = br_clampf(st.master_tape + step, 0.0f, 1.0f);
            else            st.chorus_mix  = br_clampf(st.chorus_mix + step, 0.0f, 1.0f);
        }
        helios_set_state(&st);
        break;

    case PG_EQ:
        if (knob == 0u) {
            if (ui_sel_alt) st.eq_lo_hz = br_clampf(st.eq_lo_hz + step, 0.0f, 1.0f);
            else            st.eq_lo_db = br_clampf(st.eq_lo_db + (float)d * 0.5f, -12.0f, 12.0f);
        } else if (knob == 1u) {
            if (ui_sel_alt) st.eq_mid_q  = br_clampf(st.eq_mid_q + step, 0.0f, 1.0f);
            else            st.eq_mid_db = br_clampf(st.eq_mid_db + (float)d * 0.5f, -12.0f, 12.0f);
        } else if (knob == 2u) {
            if (ui_sel_alt) st.eq_hi_hz  = br_clampf(st.eq_hi_hz + step, 0.0f, 1.0f);
            else            st.eq_mid_hz = br_clampf(st.eq_mid_hz + step, 0.0f, 1.0f);
        } else {
            if (ui_sel_alt) st.eq_on    = (d > 0) ? 1u : 0u;
            else            st.eq_hi_db = br_clampf(st.eq_hi_db + (float)d * 0.5f, -12.0f, 12.0f);
        }
        helios_set_state(&st);
        break;

    case PG_MATRIX:
        if (knob == 0u) {
            if (ui_sel_alt) { st.slew_amt = br_clampf(st.slew_amt + step, 0.0f, 1.0f); helios_set_state(&st); }
            else            ui_pin_r = (uint8_t)(((int)ui_pin_r + 64 + (int)d) & 7);
        } else if (knob == 1u) {
            if (ui_sel_alt) { st.vcf_lfo = br_clampf(st.vcf_lfo + step, 0.0f, 1.0f); helios_set_state(&st); }
            else            ui_pin_c = (uint8_t)(((int)ui_pin_c + 64 + (int)d) & 7);
        } else if (knob == 2u) {
            if (ui_sel_alt) { st.joy_y = br_clampf(st.joy_y + step * 1.5f, -1.0f, 1.0f); helios_set_state(&st); }
            else            helios_pin_step(ui_pin_r, ui_pin_c, (int)d);
        } else {
            if (ui_sel_alt) helios_pin_preset_step((int)d);
            else            helios_adjust_cv_gen(0u, (int)d);
        }
        break;

    case PG_GLOBAL:
    default:
        if (knob == 0u) {
            if (ui_sel_alt) {
                st.tuning = (uint8_t)(((int)st.tuning + 50 + (int)d) % 5);
                helios_set_state(&st);
            } else {
                ui_theme = (uint8_t)(((int)ui_theme + 50 + (int)d) % 5);
            }
        } else if (knob == 1u) {
            if (ui_sel_alt) {
                st.chord_mode = (uint8_t)(((int)st.chord_mode + 60 + (int)d) % 6);
            } else {
                st.scale = (uint8_t)(((int)st.scale + 70 + (int)d) % 7);
            }
            helios_set_state(&st);
        } else if (knob == 2u) {
            if (ui_sel_alt) {
                st.lfo_fast = (d > 0) ? 1u : 0u;
                helios_set_state(&st);
            } else {
                helios_retune_drones_to_root((uint8_t)(((int)st.root + 120 + (int)d) % 12));
            }
        } else {
            if (ui_sel_alt) {
                int m = (ui_spk_mute ? 2 : ui_usb_full ? 1 : 0);
                m = (m + 30 + (int)d) % 3;
                ui_usb_full = (m >= 1) ? 1u : 0u;
                ui_spk_mute = (m == 2) ? 1u : 0u;
                plat_usb_audio_full(ui_usb_full);
#ifndef PLAT_TICKS_PER_US
                plat_speaker(!ui_spk_mute);
#endif
            } else {
                st.bpm = (uint8_t)clamp((int32_t)st.bpm + d, 30, 240);
                helios_set_state(&st);
            }
        }
        break;
    }

    helios_get_state(&st);
    hl_knob_info(&st, page, knob, lbl, val, &norm);
    app_say(lbl, val);
}

/* ---- Brand-New 240x240 RGB565 HELIOS Music ENTROPY HELIOS Hardware Front-End Renderer ---- */
#define C42_BG_PANEL   RGB(10, 11, 14)
#define C42_BG_CARD    RGB(18, 19, 24)
#define C42_BG_SEL     RGB(36, 30, 18)
#define C42_GOLD_BRT   RGB(248, 198, 80)
#define C42_GOLD_MID   RGB(185, 142, 56)
#define C42_GOLD_DIM   RGB(86, 66, 32)
#define C42_COPPER     RGB(220, 126, 62)
#define C42_COPPER_DK  RGB(88, 46, 22)
#define C42_CYAN       RGB(82, 232, 255)
#define C42_GREEN      RGB(88, 255, 155)
#define C42_MAGENTA    RGB(255, 82, 185)
#define C42_LED_ON     RGB(255, 155, 35)
#define C42_LED_OFF    RGB(38, 28, 18)

static void hl_draw_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t fill, uint16_t bdr)
{
    cv_rect(x, y, w, h, bdr);
    if (w > 2 && h > 2)
        cv_rect(x + 1, y + 1, w - 2, h - 2, fill);
}

static void hl_draw_hbar(int32_t x, int32_t y, int32_t w, int32_t h, float norm, uint16_t fill_c, uint16_t tip_c)
{
    int32_t fw;
    norm = br_clampf(norm, 0.0f, 1.0f);
    fw = (int32_t)(norm * (float)w + 0.5f);
    cv_rect(x, y, w, h, RGB(10, 11, 15));
    if (fw > 0) {
        cv_rect(x, y, fw, h, fill_c);
        if (fw >= 2)
            cv_rect(x + fw - 2, y, 2, h, tip_c);
    }
}

/* ---- 5 Official HELIOS Music Entropy Helios Hardware Colorways (Selectable in GLO -> K1) ---- */
typedef struct {
    const char *name;
    int32_t pr0, pg0, pb0, pr1, pg1, pb1; /* Main anodized faceplate top -> bottom */
    int32_t sr0, sg0, sb0, sr1, sg1, sb1; /* Sub-module card strip top -> bottom */
    uint16_t rim;        /* 3D chamfered top/left metallic rim */
    uint16_t text_pri;   /* Primary silkscreen label color */
    uint16_t text_sec;   /* Secondary silkscreen accent */
    uint16_t tick_dim;   /* Unlit silkscreen tick mark */
    uint16_t outer_bg;   /* Outer chassis border */
    uint8_t  is_light;   /* 1 for White Cream & Maize Yellow */
} hl_theme_t;

static const hl_theme_t HL_THEMES[5] = {
    /* 0: BLACK — Classic HELIOS Matte Black Anodized Aluminum */
    {"BLACK",     34, 34, 36,  18, 18, 20,   46, 44, 42,  26, 25, 24,
     RGB(185, 165, 118), RGB(238, 224, 186), RGB(180, 150, 92), RGB(72, 62, 42),  RGB(10, 10, 11), 0},
    /* 1: WHITE CREAM — Official HELIOS RAL 9001 Warm Cream Aluminum + Charcoal Silkscreen */
    {"CREAM", 236, 230, 214, 212, 204, 186, 224, 216, 198, 198, 190, 172,
     RGB(252, 248, 240), RGB(24, 24, 26),    RGB(145, 76, 26),  RGB(128, 120, 104), RGB(176, 170, 154), 1},
    /* 2: AMBER YEL — Anodized RAL 1006 Amber Maize Yellow + Jet-Black Silkscreen */
    {"AMBER YEL", 236, 184, 38,  204, 150, 22,  222, 170, 32,  188, 136, 18,
     RGB(254, 226, 118), RGB(18, 16, 12),    RGB(110, 36, 16),  RGB(118, 86, 16), RGB(158, 116, 16), 1},
    /* 3: COBALT — Anodized Cobalt/Cerulean Blue + Crisp Ivory Silkscreen */
    {"COBALT", 44, 98, 168,   24, 60, 116,   54, 112, 184,  30, 72, 132,
     RGB(148, 202, 250), RGB(242, 246, 252), RGB(248, 206, 84), RGB(24, 52, 92),  RGB(14, 34, 68), 0},
    /* 4: OLIVE — Official HELIOS RAL 8000 Soviet Olive Green-Brown + Warm Cream Silkscreen */
    {"OLIVE", 128, 118, 72,  86, 78, 46,    142, 132, 84,  98, 90, 54,
     RGB(196, 186, 134), RGB(246, 238, 208), RGB(248, 184, 62), RGB(62, 56, 32),  RGB(54, 48, 28), 0}
};

/* Authentic HELIOS Entropy Helios Functional Knob Cap Color by Section:
 *   Drones = Cobalt Blue, Melodic/Seq = Emerald Green, Analog VCF = Crimson Red,
 *   FX Cart = Amber Yellow, Home/Preamp/Global = Ivory White (or Charcoal on light panels) */
static void hl_page_knob_cap(uint8_t pg, int32_t *cr, int32_t *cg, int32_t *cb, uint16_t *ptr_c)
{
    const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
    if (pg == PG_DRN_1_3 || pg == PG_DRN_6_8 || pg == PG_DRN_ALL) {
        *cr = 44; *cg = 122; *cb = 220; *ptr_c = C_WHITE;
    } else if (pg == PG_MEL_V4 || pg == PG_MEL_V5 || pg == PG_SEQ_L || pg == PG_SEQ_R) {
        *cr = 42; *cg = 172; *cb = 92;  *ptr_c = C_WHITE;
    } else if (pg == PG_VCF) {
        *cr = 212; *cg = 56; *cb = 44;  *ptr_c = C_WHITE;
    } else if (pg == PG_CART) {
        *cr = 232; *cg = 178; *cb = 36; *ptr_c = RGB(18, 16, 14);
    } else if (th->is_light) {
        *cr = 54; *cg = 52; *cb = 50;   *ptr_c = C_WHITE;
    } else {
        *cr = 216; *cg = 212; *cb = 202; *ptr_c = RGB(18, 16, 14);
    }
}

/* ---- HELIOS Entropy Helios Photorealistic 3D Hardware Graphics Engine (0 extra RAM, seamless across all 4 bands) ---- */
static void hl_srect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t by, uint16_t c)
{
    int32_t y0 = y - by, y1 = y + h - by;
    if (w <= 0 || h <= 0 || y1 <= 0 || y0 >= 48 || x >= 240 || x + w <= 0) return;
    if (y0 < 0) y0 = 0;
    if (y1 > 48) y1 = 48;
    if (x < 0) { w += x; x = 0; }
    if (x + w > 240) w = 240 - x;
    if (w > 0 && y1 > y0) cv_rect(x, y0, w, y1 - y0, c);
}

static void hl_stext(int32_t x, int32_t y, int32_t by, const felucca_font_t *f, const char *s, uint16_t c)
{
    int32_t ly = y - by;
    if (ly > -28 && ly < 48) cv_text(x, ly, f, s, c);
}

static void hl_sdisc(int32_t cx, int32_t cy, int32_t r, int32_t by, uint16_t c)
{
    int32_t dy, r2 = r * r;
    if (cy + r < by || cy - r >= by + 48) return;
    for (dy = -r; dy <= r; dy++) {
        int32_t sy = cy + dy;
        if (sy < by || sy >= by + 48) continue;
        {
            int32_t dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= r2) dx++;
            hl_srect(cx - dx, sy, dx * 2 + 1, 1, by, c);
        }
    }
}

static void hl_hw_screw(int32_t cx, int32_t cy, int32_t by)
{
    if (cy + 5 < by || cy - 5 >= by + 48) return;
    hl_sdisc(cx, cy, 4, by, RGB(14, 12, 10));
    hl_sdisc(cx, cy, 3, by, RGB(148, 122, 74));
    hl_srect(cx - 2, cy - 2, 2, 1, by, RGB(230, 205, 145));
    /* Diagonal recessed brass screw slot */
    hl_srect(cx - 2, cy - 1, 2, 1, by, RGB(34, 26, 16));
    hl_srect(cx - 1, cy,     3, 1, by, RGB(26, 20, 12));
    hl_srect(cx + 1, cy + 1, 2, 1, by, RGB(34, 26, 16));
}

static void hl_hw_panel(int32_t x, int32_t y, int32_t w, int32_t h, int32_t by,
                         int32_t r0, int32_t g0, int32_t b0,
                         int32_t r1, int32_t g1, int32_t b1,
                         uint16_t rim_hi, int screws)
{
    int32_t sy, y_start = y > by ? y : by;
    int32_t y_end = (y + h) < (by + 48) ? (y + h) : (by + 48);
    if (h <= 0 || w <= 0 || y_end <= y_start) return;
    /* Brushed anodized metallic gradient with fine horizontal aluminum micro-grain */
    for (sy = y_start; sy < y_end; sy++) {
        int32_t t = (sy - y) * 255 / (h > 1 ? (h - 1) : 1);
        int32_t grain = (((sy * 13 + 7) & 3) == 0) ? 4 : (((sy * 7) & 3) == 0 ? -3 : 0);
        int32_t rr = r0 + ((r1 - r0) * t >> 8) + grain;
        int32_t gg = g0 + ((g1 - g0) * t >> 8) + grain;
        int32_t bb = b0 + ((b1 - b0) * t >> 8) + grain;
        if (rr < 0) rr = 0; else if (rr > 255) rr = 255;
        if (gg < 0) gg = 0; else if (gg > 255) gg = 255;
        if (bb < 0) bb = 0; else if (bb > 255) bb = 255;
        hl_srect(x, sy, w, 1, by, RGB((uint32_t)rr, (uint32_t)gg, (uint32_t)bb));
    }
    /* 3D Chamfered Chassis Bevel (bright champagne specular top/left, obsidian shadow bottom/right) */
    hl_srect(x, y, w, 1, by, rim_hi);
    hl_srect(x, y, 1, h, by, rim_hi);
    hl_srect(x + 1, y + 1, w - 2, 1, by, RGB(96, 82, 54));
    hl_srect(x + 1, y + h - 2, w - 2, 1, by, RGB(14, 12, 10));
    hl_srect(x, y + h - 1, w, 1, by, RGB(6, 5, 5));
    hl_srect(x + w - 1, y, 1, h, by, RGB(6, 5, 5));
    if (screws && w >= 52 && h >= 36) {
        hl_hw_screw(x + 7, y + 7, by);
        hl_hw_screw(x + w - 8, y + 7, by);
        hl_hw_screw(x + 7, y + h - 8, by);
        hl_hw_screw(x + w - 8, y + h - 8, by);
    }
}

static void hl_hw_glass(int32_t x, int32_t y, int32_t w, int32_t h, int32_t by)
{
    int32_t sy, y_start = y > by ? y : by;
    int32_t y_end = (y + h) < (by + 48) ? (y + h) : (by + 48);
    if (y_end <= y_start) return;
    /* Deep recessed CRT/VFD phosphor glass with subtle scanlines + subtle top glass sheen */
    for (sy = y_start; sy < y_end; sy++) {
        int32_t rel = sy - y;
        uint16_t gc = (rel < 5) ? RGB(14, 24, 28)
                    : ((rel & 1) ? RGB(6, 13, 16) : RGB(9, 17, 21));
        hl_srect(x + 1, sy, w - 2, 1, by, gc);
    }
    /* Inset shadow top/left, faint metallic highlight bottom/right */
    hl_srect(x, y, w, 1, by, RGB(4, 4, 5));
    hl_srect(x, y, 1, h, by, RGB(4, 4, 5));
    hl_srect(x + 1, y + 1, w - 2, 1, by, RGB(10, 12, 14));
    hl_srect(x, y + h - 1, w, 1, by, RGB(82, 74, 56));
    hl_srect(x + w - 1, y, 1, h, by, RGB(64, 58, 44));
}

static void hl_hw_led(int32_t cx, int32_t cy, int32_t by, int on, uint16_t col)
{
    if (cy + 7 < by || cy - 7 >= by + 48) return;
    if (on) {
        /* Soft outer radial bloom halo */
        hl_sdisc(cx, cy, 6, by, RGB(58, 44, 20));
    }
    /* Chrome bezel collar */
    hl_sdisc(cx, cy, 4, by, RGB(112, 108, 100));
    hl_sdisc(cx, cy, 3, by, on ? col : RGB(28, 22, 18));
    if (on) {
        hl_srect(cx - 1, cy - 2, 2, 2, by, C_WHITE);
    } else {
        hl_srect(cx - 1, cy - 1, 1, 1, by, RGB(64, 56, 48));
    }
}

static void hl_hw_toggle(int32_t cx, int32_t cy, int32_t by, int state, uint16_t col)
{
    if (cy + 14 < by || cy - 14 >= by + 48) return;
    /* Cast shadow + Silver Hex Nut + Recessed dark bushing */
    hl_sdisc(cx + 1, cy + 2, 6, by, RGB(12, 11, 10));
    hl_sdisc(cx, cy, 6, by, RGB(140, 138, 134));
    hl_srect(cx - 4, cy - 5, 9, 1, by, RGB(215, 215, 220));
    hl_sdisc(cx, cy, 3, by, RGB(18, 18, 22));
    if (state > 0) {
        /* Chrome Bat Lever UP with bright specular left ridge */
        hl_srect(cx - 2, cy - 11, 5, 11, by, RGB(120, 124, 132));
        hl_srect(cx - 1, cy - 11, 2, 11, by, RGB(238, 240, 246));
        hl_sdisc(cx, cy - 11, 3, by, col);
        hl_srect(cx - 1, cy - 12, 2, 2, by, C_WHITE);
    } else if (state < 0) {
        /* Chrome Bat Lever DOWN */
        hl_srect(cx - 2, cy, 5, 11, by, RGB(105, 108, 118));
        hl_srect(cx - 1, cy, 2, 11, by, RGB(215, 220, 228));
        hl_sdisc(cx, cy + 11, 3, by, col);
        hl_srect(cx - 1, cy + 10, 2, 1, by, C_WHITE);
    } else {
        hl_sdisc(cx, cy, 3, by, col);
    }
}

static void hl_hw_knob_3d(int32_t cx, int32_t cy, int32_t r, int32_t by, float norm, uint16_t accent,
                           int32_t cap_r_c, int32_t cap_g_c, int32_t cap_b_c, uint16_t ptr_col)
{
    const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
    int32_t t, dy;
    int32_t cap_r = r - 3;
    if (cap_r < 4) cap_r = r - 2;
    if (cy + r + 8 < by || cy - r - 8 >= by + 48) return;
    norm = br_clampf(norm, 0.0f, 1.0f);

    /* 1. Soft Cast Drop Shadow down-right onto the anodized faceplate */
    hl_sdisc(cx + 2, cy + 3, r + 1, by, th->is_light ? RGB(88, 82, 68) : RGB(10, 10, 10));

    /* 2. Calibrated 11-Tick Silkscreen + Glowing Active Arc (270 deg sweep) */
    {
        int32_t nticks = (r >= 12) ? 11 : 9;
        int32_t tr = r + (r >= 12 ? 4 : 3);
        for (t = 0; t < nticks; t++) {
            float frac = (float)t / (float)(nticks - 1);
            float ang = -0.375f + frac * 0.75f;
            int32_t tx = cx + (int32_t)(fm_sin_turns(ang) * (float)tr);
            int32_t ty = cy - (int32_t)(fm_sin_turns(ang + 0.25f) * (float)tr);
            int lit = (frac <= norm + 0.03f);
            if (r >= 12) {
                hl_srect(tx - 1, ty - 1, 3, 3, by, lit ? accent : th->tick_dim);
                if (lit && !th->is_light) hl_srect(tx, ty, 1, 1, by, C_WHITE);
            } else {
                hl_srect(tx, ty, 2, 2, by, lit ? accent : th->tick_dim);
            }
        }
    }

    /* 3. Authentic HELIOS Fluted Matte-Black / Graphite Outer Skirt (12 knurled ridges!) */
    hl_sdisc(cx, cy, r, by, RGB(24, 24, 28));
    if (r >= 10) {
        for (t = 0; t < 12; t++) {
            float ang = (float)t * (1.0f / 12.0f);
            int32_t fx = cx + (int32_t)(fm_sin_turns(ang) * (float)(r - 1));
            int32_t fy = cy - (int32_t)(fm_sin_turns(ang + 0.25f) * (float)(r - 1));
            hl_srect(fx, fy, 2, 2, by, (t & 1) ? RGB(78, 78, 84) : RGB(12, 12, 14));
        }
    }

    /* 4. Authentic HELIOS Color-Coded 3D Radially-Lit Domed Top Cap */
    {
        int32_t r2 = cap_r * cap_r;
        for (dy = -cap_r; dy <= cap_r; dy++) {
            int32_t sy = cy + dy;
            if (sy < by || sy >= by + 48) continue;
            {
                int32_t dx = 0;
                int32_t dl = -(dy * 46) / (cap_r > 0 ? cap_r : 1);
                int32_t rr = cap_r_c + dl, gg = cap_g_c + dl, bb = cap_b_c + dl;
                while ((dx + 1) * (dx + 1) + dy * dy <= r2) dx++;
                if (rr < 10) rr = 10; else if (rr > 255) rr = 255;
                if (gg < 10) gg = 10; else if (gg > 255) gg = 255;
                if (bb < 10) bb = 10; else if (bb > 255) bb = 255;
                hl_srect(cx - dx, sy, dx * 2 + 1, 1, by, RGB((uint32_t)rr, (uint32_t)gg, (uint32_t)bb));
                /* Top-left specular highlight crescent on domed cap */
                if (dy < -cap_r / 3 && dx >= 3) {
                    int32_t hr = rr + 34, hg = gg + 34, hb = bb + 34;
                    if (hr > 255) hr = 255;
                    if (hg > 255) hg = 255;
                    if (hb > 255) hb = 255;
                    hl_srect(cx - dx + 1, sy, dx, 1, by, RGB((uint32_t)hr, (uint32_t)hg, (uint32_t)hb));
                }
            }
        }
    }

    /* 5. Crisp High-Contrast Rotating Hardware Pointer Bar */
    {
        float ang = -0.375f + norm * 0.75f;
        float sx = fm_sin_turns(ang), sy = -fm_sin_turns(ang + 0.25f);
        int32_t s;
        for (s = 1; s <= cap_r; s++) {
            int32_t px = cx + (int32_t)(sx * (float)s);
            int32_t py = cy + (int32_t)(sy * (float)s);
            hl_srect(px - 1, py - 1, (r >= 12) ? 3 : 2, (r >= 12) ? 3 : 2, by, ptr_col);
        }
    }
}

/* Smooth 360ms Cubic Ease-Out 3D Hardware Expand / Collapse Zoom Animation across ALL 4 Bands (y = 48..239) */
static void hl_paint_zoom_anim(int32_t by, uint32_t now)
{
    uint32_t dt = now - ui_zoom_t0;
    int expanding = (ui_page != PG_HOME);
    uint8_t pg = expanding ? ui_page : ui_zoom_from;
    float t = br_clampf((float)dt * (1.0f / 360.0f), 0.0f, 1.0f);
    float inv = 1.0f - t;
    float ease = 1.0f - inv * inv * inv; /* cubic ease-out */
    float f = expanding ? ease : (1.0f - ease);
    /* Origin module rect on the HOME Entropy Helios faceplate */
    int32_t ox = (pg == PG_DRN_1_3 || pg == PG_MEL_V4 || pg == PG_SEQ_L) ? 4
               : (pg == PG_DRN_6_8 || pg == PG_MEL_V5 || pg == PG_SEQ_R) ? 160 : 82;
    int32_t oy = (pg == PG_DRN_1_3 || pg == PG_DRN_6_8 || pg == PG_DRN_ALL) ? 48 : 96;
    int32_t ow = 76, oh = 46;
    /* Smoothly expand all the way to the 236x190 Full-Screen Hardware Stage (y = 48..238)! */
    int32_t rx = ox + (int32_t)((float)(2 - ox) * f);
    int32_t ry = oy + (int32_t)((float)(48 - oy) * f);
    int32_t rw = ow + (int32_t)((float)(236 - ow) * f);
    int32_t rh = oh + (int32_t)((float)(190 - oh) * f);

    /* Subtle dark metallic background grid behind expanding hardware module */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_srect(0, by, 240, 48, by, th->outer_bg);
        hl_srect(rx - 2, ry - 2, rw + 4, rh + 4, by, th->rim);
        hl_hw_panel(rx, ry, rw, rh, by, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1, th->rim, rw > 110 && rh > 70);
        if (rw > 90 && rh > 42) {
            hl_stext(rx + 16, ry + 6, by, &FONT_T, PG_TITLE[pg % PG_COUNT], th->text_pri);
        }
        if (rw > 120 && rh > 90) {
            hl_hw_glass(rx + 12, ry + 22, rw - 24, (rh - 38) / 2, by);
        }
    }
}

/* FULL-SCREEN 240x192 ZOOMED HELIOS ENTROPY HELIOS HARDWARE MODULE RENDERER (y = 48..239) */
static void hl_paint_zoomed_module(const helios_state_t *st, int32_t by, uint32_t now)
{
    const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
    uint32_t i, s, k;
    char hdr[40], buf[20];
    uint16_t accent = ui_sel_alt ? C42_CYAN : (th->is_light ? RGB(204, 52, 32) : C42_GOLD_BRT);
    int32_t cr, cg, cb;
    uint16_t ptr_c;
    hl_page_knob_cap(ui_page, &cr, &cg, &cb, &ptr_c);

    /* 1. Full-Screen Brushed Anodized HELIOS Entropy Helios Faceplate in Active Colorway (y = 48..222) */
    hl_hw_panel(2, 48, 236, 174, by, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1,
                 ui_sel_alt ? C42_COPPER : th->rim, 1);

    /* 2. Engraved Module Brass Nameplate Strip (y = 52..65) */
    hl_srect(14, 52, 212, 13, by, RGB(22, 19, 15));
    hl_srect(14, 65, 212, 1, by, C42_GOLD_MID);
    hl_hw_led(22, 58, by, st->playing, ui_sel_alt ? C42_CYAN : C42_GREEN);
    hdr[0] = 0;
    hl_cat(hdr, PG_TITLE[ui_page % PG_COUNT]);
    if (ui_page == PG_DRN_1_3) { hl_cat(hdr, " [V"); hl_cat_u(hdr, (unsigned)(ui_sel_drone_l + 1u)); hl_cat(hdr, "]"); }
    else if (ui_page == PG_DRN_6_8) { hl_cat(hdr, " [V"); hl_cat_u(hdr, (unsigned)(ui_sel_drone_r + 3u)); hl_cat(hdr, "]"); }
    else if (ui_page == PG_SEQ_L) { hl_cat(hdr, " [S"); hl_cat_u(hdr, (unsigned)(ui_sel_step_l + 1u)); hl_cat(hdr, "]"); }
    else if (ui_page == PG_SEQ_R) { hl_cat(hdr, " [S"); hl_cat_u(hdr, (unsigned)(ui_sel_step_r + 1u)); hl_cat(hdr, "]"); }
    hl_stext(30, 53, by, &FONT_T, hdr, C42_GOLD_BRT);
    if (ui_sel_alt) {
        hl_srect(190, 53, 34, 11, by, C42_COPPER);
        hl_stext(194, 53, by, &FONT_T, "SEL*", C_WHITE);
    }

    /* 3. Top Hardware Display & Tactile Controls Bay (y = 68..134, 66 px tall!) */
    if (ui_page == PG_DRN_1_3 || ui_page == PG_DRN_6_8 || ui_page == PG_DRN_ALL) {
        unsigned base_v = (ui_page == PG_DRN_6_8) ? 3u : 0u;
        unsigned sel_v  = (ui_page == PG_DRN_1_3) ? ui_sel_drone_l : (ui_page == PG_DRN_6_8) ? (ui_sel_drone_r % 3u) : 99u;
        const hl_drone_cfg_t *dr_sel = &st->drone[base_v + (sel_v < 3u ? sel_v : 0u)];
        /* 3 Brushed Channel Strips (y = 69..104) */
        for (i = 0; i < 3u; i++) {
            int32_t bx = 8 + (int32_t)i * 76;
            const hl_drone_cfg_t *dr = &st->drone[base_v + i];
            int sel = (ui_page == PG_DRN_ALL) || (i == sel_v);
            hl_hw_panel(bx, 69, 73, 35, by,
                         sel ? 58 : 34, sel ? 50 : 30, sel ? 36 : 26,
                         sel ? 38 : 22, sel ? 32 : 20, sel ? 24 : 18,
                         sel ? C42_GOLD_BRT : C42_GOLD_DIM, 0);
            hl_hw_led(bx + 10, 77, by, dr->hold, C42_LED_ON);
            buf[0] = 'V'; buf[1] = (char)('1' + (base_v == 0u ? i : (5u + i))); buf[2] = 0;
            hl_stext(bx + 18, 72, by, &FONT_T, buf, sel ? C_WHITE : C42_GOLD_BRT);
            hl_hw_toggle(bx + 58, 86, by, dr->sub ? 1 : -1, C42_CYAN);
            hl_fmt_note(buf, dr->note);
            hl_stext(bx + 8, 84, by, &FONT_S, buf, sel ? C_WHITE : C42_GOLD_BRT);
        }
        /* Recessed Phosphor Glass: 5-Saw Spread + Live Wave Morph Scope (y = 107..134) */
        hl_hw_glass(8, 107, 110, 27, by);
        hl_stext(12, 109, by, &FONT_T, "5-SAW SPREAD", C42_GOLD_MID);
        for (s = 0; s < 5u; s++) {
            float ph = (float)((now * (9u + s * 4u)) % 1000u) * 0.001f;
            int32_t sx = 60 + (int32_t)(fm_sin_turns(ph) * dr_sel->spread * 42.0f);
            hl_srect(sx - 5, 118 + (int32_t)s * 3, 10, 2, by, (s == 0u && dr_sel->sub) ? C42_CYAN : C42_GOLD_BRT);
        }
        hl_hw_glass(122, 107, 110, 27, by);
        hl_stext(126, 109, by, &FONT_T, "WAVE MORPH", C42_CYAN);
        for (i = 0; i < 98u; i += 2u) {
            float ph = (float)i * (2.0f / 98.0f);
            ph -= (float)(int)ph;
            float saw = 2.0f * ph - 1.0f;
            float tri = (ph < 0.5f) ? (4.0f * ph - 1.0f) : (3.0f - 4.0f * ph);
            float yv = saw * (1.0f - dr_sel->wave_morph) + tri * dr_sel->wave_morph;
            int32_t py = 123 - (int32_t)(yv * 8.0f);
            hl_srect(128 + (int32_t)i, py, 2, 2, by, C42_GOLD_BRT);
        }
    } else if (ui_page == PG_SEQ_L || ui_page == PG_SEQ_R) {
        unsigned mi = (ui_page == PG_SEQ_R) ? 1u : 0u;
        unsigned sel_s = (mi == 0u ? ui_sel_step_l : ui_sel_step_r) % HL_NSTEPS;
        const hl_mel_cfg_t *m = &st->mel[mi];
        uint8_t slen = (m->seq_len >= 2u && m->seq_len <= HL_NSTEPS) ? m->seq_len : HL_NSTEPS;
        for (s = 0; s < HL_NSTEPS; s++) {
            int32_t sx = 8 + (int32_t)s * 45;
            int act = m->seq_run && (m->cur_step == s);
            int sel = (s == sel_s);
            int in_len = (s < slen);
            hl_hw_panel(sx, 69, 43, 65, by,
                         sel ? 56 : 34, sel ? 48 : 30, sel ? 34 : 26,
                         sel ? 34 : 20, sel ? 28 : 18, sel ? 22 : 16,
                         act ? C42_GREEN : sel ? C42_CYAN : in_len ? C42_GOLD_MID : C42_GOLD_DIM, 0);
            hl_hw_led(sx + 10, 77, by, act || m->step_gate[s], act ? C42_GREEN : C42_LED_ON);
            buf[0] = 'S'; buf[1] = (char)('1' + s); buf[2] = m->step_acc[s] ? '*' : 0; buf[3] = 0;
            hl_stext(sx + 18, 72, by, &FONT_T, buf, m->step_acc[s] ? C42_MAGENTA : C42_GOLD_BRT);
            /* Physical 3D Step Pitch Knob (Emerald Green Cap!) + Chrome Gate Toggle! */
            hl_hw_knob_3d(sx + 21, 96, 8, by, (float)m->steps[s] * (1.0f / 11.0f), act ? C42_GREEN : C42_GOLD_BRT, 42, 172, 92, C_WHITE);
            hl_hw_toggle(sx + 21, 121, by, m->step_gate[s] ? 1 : -1, m->step_gate[s] ? C42_GREEN : C42_GOLD_DIM);
        }
    } else if (ui_page == PG_VCF) {
        /* Huge Recessed Phosphor CRT Analog Dual Filter Curve (y = 69..134) */
        hl_hw_glass(8, 69, 166, 65, by);
        hl_srect(10, 124, 162, 1, by, RGB(24, 42, 46));
        for (i = 0; i < 160u; i += 2u) {
            float fx = (float)i * (1.0f / 160.0f);
            uint32_t ch;
            for (ch = 0; ch < 2u; ch++) {
                float c = st->vcf_cut[ch], r = st->vcf_res[ch];
                uint8_t md = st->vcf_mode[ch] & 3u;
                float d = fx - c;
                float resp = (md == 0u) ? (fx < c ? 0.62f : 0.62f / (1.0f + 16.0f * d * d))
                           : (md == 2u) ? (fx > c ? 0.62f : 0.62f / (1.0f + 16.0f * d * d))
                                        : (0.68f / (1.0f + 22.0f * d * d));
                float peak = (r * 0.65f) / (1.0f + 95.0f * d * d);
                float y = br_clampf(resp + peak, 0.0f, 1.0f);
                int32_t sy = 124 - (int32_t)(y * 44.0f);
                if (ch == 0u && sy < 124) {
                    hl_srect(11 + (int32_t)i, sy + 2, 2, 124 - sy - 2, by, RGB(34, 28, 14));
                }
                hl_srect(11 + (int32_t)i, sy, 2, 3, by, ch == 0u ? C42_GOLD_BRT : C42_CYAN);
            }
        }
        hl_stext(13, 72, by, &FONT_T, "DUAL OP-AMP VCF L/R", C42_GOLD_BRT);
        /* Right Control Pod: Chrome LINK Bat Toggle + Mode LEDs */
        hl_hw_panel(178, 69, 54, 65, by, 38, 34, 28, 22, 20, 17, C42_GOLD_MID, 0);
        hl_stext(187, 73, by, &FONT_T, "L/R LNK", C42_GOLD_MID);
        hl_hw_toggle(205, 98, by, st->vcf_link ? 1 : -1, st->vcf_link ? C42_GREEN : C42_MAGENTA);
        hl_stext(184, 118, by, &FONT_T, st->vcf_link ? "LINKED" : "INDEP", st->vcf_link ? C42_GREEN : C42_MAGENTA);
    } else if (ui_page == PG_CART) {
        int cur_on = (st->cart_mask & (1u << (st->cart_type & 15u))) != 0u;
        /* Realistic HELIOS Magnetic FX Cartridge with Gold Edge Pins + 16-Slot Active Stack Rack! */
        hl_hw_panel(8, 69, 170, 34, by, cur_on ? 86 : 42, cur_on ? 58 : 38, cur_on ? 28 : 34,
                     cur_on ? 48 : 26, cur_on ? 32 : 24, cur_on ? 16 : 22,
                     cur_on ? C42_GOLD_BRT : C42_GOLD_DIM, 0);
        hl_stext(14, 72, by, &FONT_T, cur_on ? "STACKED CART (ON)" : "CARTRIDGE (OFF)", cur_on ? C42_GREEN : C42_GOLD_MID);
        hl_stext(14, 83, by, &FONT_S, helios_cart_name(st->cart_type), C_WHITE);
        /* 12 Gold PCB Edge-Connector Fingers at bottom of cartridge */
        for (i = 0; i < 12u; i++) {
            hl_srect(16 + (int32_t)i * 13, 99, 7, 4, by, cur_on ? C42_GOLD_BRT : RGB(96, 84, 52));
        }
        /* Chrome PER-CARTRIDGE LOAD ON/OFF Bat Toggle Pod */
        hl_hw_panel(182, 69, 50, 65, by, 34, 34, 38, 20, 20, 24, cur_on ? C42_GREEN : C42_GOLD_DIM, 0);
        hl_hw_led(207, 78, by, cur_on, C42_GREEN);
        hl_hw_toggle(207, 102, by, cur_on ? 1 : -1, cur_on ? C42_GREEN : C42_MAGENTA);
        hl_stext(196, 120, by, &FONT_T, cur_on ? "ON" : "OFF", cur_on ? C42_GREEN : C42_MAGENTA);
        /* 16-Slot Glowing Active Stack Rack + Live 3-Spring Tank (y = 106..134) */
        hl_hw_glass(8, 106, 170, 28, by);
        hl_stext(13, 108, by, &FONT_T, "STACK RACK (16)", C42_CYAN);
        if (st->cart_freeze) hl_stext(136, 108, by, &FONT_T, "FRZ*", C42_CYAN);
        for (i = 0; i < HL_NCART; i++) {
            int32_t rx = 13 + (int32_t)i * 10;
            int on_i = (st->cart_mask & (1u << i)) != 0u;
            int sel_i = (i == (st->cart_type % HL_NCART));
            hl_srect(rx, 120, 8, 10, by, sel_i ? C_WHITE : (on_i ? C42_GREEN : RGB(28, 30, 34)));
            hl_srect(rx + 1, 121, 6, 8, by, on_i ? (sel_i ? C42_CYAN : C42_GREEN) : RGB(14, 16, 20));
        }
    } else if (ui_page == PG_MEL_V4 || ui_page == PG_MEL_V5) {
        unsigned mi = (ui_page == PG_MEL_V5) ? 1u : 0u;
        const hl_mel_cfg_t *m = &st->mel[mi];
        hl_hw_glass(8, 69, 146, 65, by);
        hl_srect(10, 101, 142, 1, by, RGB(24, 42, 46));
        for (i = 0; i < 140u; i += 2u) {
            float ph = (float)i * (2.0f / 140.0f);
            ph -= (float)(int)ph;
            float yv = (m->wave == 0u) ? (2.0f * ph - 1.0f)
                     : (m->wave == 1u) ? (ph < 0.35f ? 0.85f : -0.85f)
                     : (m->wave == 2u) ? fm_sin_turns(ph + 0.25f * fm_sin_turns(ph * 2.0f))
                     : (m->wave == 3u) ? fm_sin_turns(ph + 0.45f * fm_sin_turns(ph * 3.0f))
                     : (m->wave == 4u) ? (0.6f * fm_sin_turns(ph) + 0.4f * fm_sin_turns(ph * 4.0f))
                                       : (0.65f * fm_sin_turns(ph) + 0.35f * fm_sin_turns(ph * 2.0f));
            int32_t sy = 101 - (int32_t)(yv * 24.0f);
            hl_srect(11 + (int32_t)i, sy, 2, 3, by, mi ? C42_GOLD_BRT : C42_CYAN);
        }
        hl_stext(13, 72, by, &FONT_T, helios_wave_name(m->wave), C42_CYAN);
        hl_hw_panel(158, 69, 74, 65, by, 40, 36, 30, 22, 20, 18, C42_GOLD_MID, 0);
        hl_hw_led(170, 79, by, m->hold, C42_GREEN);
        hl_stext(179, 74, by, &FONT_T, m->hold ? "HOLD" : "TRIG", C42_GOLD_MID);
        hl_fmt_note(buf, m->cur_note);
        hl_stext(168, 92, by, &FONT_M, buf, C_WHITE);
    } else if (ui_page == PG_PREAMP) {
        hl_hw_glass(8, 69, 224, 65, by);
        hl_hw_led(20, 80, by, st->ext_comp, C42_MAGENTA);
        hl_stext(30, 75, by, &FONT_T, "PREAMP INPUT:", C42_GOLD_MID);
        hl_stext(108, 75, by, &FONT_T, helios_extsrc_name(st->ext_src), C42_CYAN);
        hl_stext(16, 94, by, &FONT_T, "ENV FOLLOWER", C42_GREEN);
        hl_srect(98, 96, 124, 8, by, RGB(14, 18, 22));
        hl_srect(98, 96, (int32_t)(br_clampf(st->ext_env, 0.0f, 1.0f) * 124.0f), 8, by, C42_GREEN);
        hl_stext(16, 112, by, &FONT_T, "BBD CHORUS", C42_CYAN);
        hl_srect(98, 114, 124, 8, by, RGB(14, 18, 22));
        hl_srect(98, 114, (int32_t)(br_clampf(st->chorus_mix, 0.0f, 1.0f) * 124.0f), 8, by, C42_CYAN);
    } else if (ui_page == PG_EQ) {
        /* Live Graphical 3-Band Parametric Master EQ Response Curve (20 Hz .. 20 kHz log scale) */
        float fl = 50.0f * fm_powf(6.0f, br_clampf(st->eq_lo_hz, 0.0f, 1.0f));
        float fm = 180.0f * fm_powf(25.0f, br_clampf(st->eq_mid_hz, 0.0f, 1.0f));
        float fh = 2500.0f * fm_powf(4.0f, br_clampf(st->eq_hi_hz, 0.0f, 1.0f));
        float qm = 0.5f * fm_powf(8.0f, br_clampf(st->eq_mid_q, 0.0f, 1.0f));
        float lfl = fm_log2f(fl * (1.0f / 20.0f)) * (1.0f / 9.96578f);
        float lfm = fm_log2f(fm * (1.0f / 20.0f)) * (1.0f / 9.96578f);
        float lfh = fm_log2f(fh * (1.0f / 20.0f)) * (1.0f / 9.96578f);
        hl_hw_glass(8, 69, 224, 65, by);
        /* 0 dB center line + 100Hz / 1kHz / 10kHz log grid lines */
        hl_srect(10, 102, 220, 1, by, RGB(34, 48, 54));
        hl_srect(10, 86, 220, 1, by, RGB(18, 24, 28));
        hl_srect(10, 118, 220, 1, by, RGB(18, 24, 28));
        hl_srect(61, 71, 1, 61, by, RGB(18, 24, 28));   /* ~100 Hz */
        hl_srect(134, 71, 1, 61, by, RGB(18, 24, 28));  /* ~1 kHz */
        hl_srect(207, 71, 1, 61, by, RGB(18, 24, 28));  /* ~10 kHz */
        hl_stext(13, 72, by, &FONT_T, "3-BAND PARAM EQ", st->eq_on ? C42_GOLD_BRT : C42_GOLD_DIM);
        hl_stext(150, 72, by, &FONT_T, st->eq_on ? "ON [REC=0]" : "BYPASSED", st->eq_on ? C42_GREEN : C42_MAGENTA);
        for (i = 0; i < 218u; i += 2u) {
            float u = (float)i * (1.0f / 218.0f);
            float db = 0.0f;
            if (st->eq_on) {
                float d_lo = st->eq_lo_db / (1.0f + fm_exp2f((u - lfl) * 14.0f));
                float dm = (u - lfm) * (3.2f + qm * 3.8f);
                float d_mid = st->eq_mid_db / (1.0f + dm * dm);
                float d_hi = st->eq_hi_db / (1.0f + fm_exp2f((lfh - u) * 14.0f));
                db = br_clampf(d_lo + d_mid + d_hi, -14.0f, 14.0f);
            }
            int32_t sy = 102 - (int32_t)(db * 1.95f);
            if (sy < 74) sy = 74;
            if (sy > 130) sy = 130;
            if (sy < 102) hl_srect(11 + (int32_t)i, sy + 2, 2, 102 - sy - 2, by, st->eq_on ? RGB(16, 44, 48) : RGB(22, 22, 24));
            else if (sy > 102) hl_srect(11 + (int32_t)i, 103, 2, sy - 103, by, st->eq_on ? RGB(44, 24, 18) : RGB(22, 22, 24));
            hl_srect(11 + (int32_t)i, sy, 2, 3, by, st->eq_on ? C42_CYAN : C42_GOLD_DIM);
        }
        /* Glowing band nodes (Low = Gold, Mid = Green, High = Cyan) */
        if (st->eq_on) {
            int32_t lx = 11 + (int32_t)(lfl * 216.0f), ly = 102 - (int32_t)(st->eq_lo_db * 1.95f);
            int32_t mx = 11 + (int32_t)(lfm * 216.0f), my = 102 - (int32_t)(st->eq_mid_db * 1.95f);
            int32_t hx = 11 + (int32_t)(lfh * 216.0f), hy = 102 - (int32_t)(st->eq_hi_db * 1.95f);
            hl_srect(lx - 2, clamp(ly, 75, 129) - 2, 5, 5, by, C42_GOLD_BRT);
            hl_srect(mx - 2, clamp(my, 75, 129) - 2, 5, 5, by, C42_GREEN);
            hl_srect(hx - 2, clamp(hy, 75, 129) - 2, 5, 5, by, C_WHITE);
        }
    } else if (ui_page == PG_HELP) {
        /* Built-In On-Screen Quick-Help Cheat Sheet (GLO 2nd Press) */
        hl_hw_glass(8, 69, 224, 147, by);
        hl_stext(14, 74,  by, &FONT_T, "ENTROPY HELIOS v1.3 // QUICK GUIDE", C42_GOLD_BRT);
        hl_stext(14, 90,  by, &FONT_T, "SEL       : Toggle Alt Layer (SEL*)", C_WHITE);
        hl_stext(14, 104, by, &FONT_T, "FX x2     : 16 Stackable Carts", C42_CYAN);
        hl_stext(14, 118, by, &FONT_T, "  K2=ON/OFF, REC=Solo Current Cart", C42_GOLD_MID);
        hl_stext(14, 132, by, &FONT_T, "GLO -> K1 : 5 Chassis Finishes", C42_GREEN);
        hl_stext(14, 146, by, &FONT_T, "SHORT SAVE: Reset Current Preset", C_WHITE);
        hl_stext(14, 160, by, &FONT_T, "SEL+SAVE  : Save to User Slot U01-U08", C42_GOLD_BRT);
        hl_stext(14, 174, by, &FONT_T, "HOLD SAVE3s: Full Factory Reset All", C42_MAGENTA);
        hl_stext(14, 190, by, &FONT_T, "END OF BTN CYCLE OR [HOME] = EXIT", C42_CYAN);
    } else {
        /* PG_GLOBAL: Dual-Core Telemetry & Studio Master Bus */
        uint32_t vj, fxj, lt, ls, c1p;
        int md;
        helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
        hl_hw_glass(8, 69, 224, 65, by);
        hl_hw_led(20, 80, by, st->ext_clk_lock, C42_GREEN);
        hl_stext(30, 75, by, &FONT_T, st->ext_clk_lock ? "EXT MIDI CLOCK" : "INTERNAL CLOCK", st->ext_clk_lock ? C42_GREEN : C42_GOLD_BRT);
        buf[0] = 0; hl_cat_u(buf, (unsigned)st->bpm); hl_cat(buf, " BPM");
        hl_stext(154, 73, by, &FONT_S, buf, C_WHITE);
        hl_stext(16, 95, by, &FONT_T, "CORE0", C42_GREEN);
        hl_srect(56, 97, 74, 7, by, RGB(14, 18, 22));
        hl_srect(56, 97, (int32_t)(br_clampf((float)plat_cpu_pct() * 0.01f, 0.0f, 1.0f) * 74.0f), 7, by, C42_GREEN);
        hl_stext(138, 95, by, &FONT_T, "CORE1", C42_CYAN);
        hl_srect(178, 97, 46, 7, by, RGB(14, 18, 22));
        hl_srect(178, 97, (int32_t)(br_clampf((float)c1p * 0.01f, 0.0f, 1.0f) * 46.0f), 7, by, C42_CYAN);
        hl_stext(16, 114, by, &FONT_T, helios_tuning_name(st->tuning), C42_GOLD_BRT);
        hl_stext(122, 114, by, &FONT_T, helios_chord_name(st->chord_mode), C42_CYAN);
    }

    /* 4. GIANT 4-KNOB HELIOS HARDWARE CONTROL DECK (y = 138..218, 80 px tall!) */
    if (ui_page != PG_HELP) {
        hl_srect(8, 137, 224, 1, by, th->tick_dim);
        for (k = 0; k < 4u; k++) {
            int32_t cx = 32 + (int32_t)k * 58;
            int act_k = (k == ui_act_knob && (int32_t)(ui_act_until - now) > 0);
            char lbl[16], val[20];
            float norm = 0.0f;
            hl_knob_info(st, ui_page, k, lbl, val, &norm);
            /* Engraved Silkscreen Parameter Title ABOVE knob */
            hl_stext(cx - 24, 140, by, &FONT_T, lbl, act_k ? (th->is_light ? RGB(204, 42, 24) : C42_CYAN) : th->text_pri);
            /* GIANT 3D-SHADED HELIOS COLOR-CODED KNURLED ROTARY KNOB (r = 15 px!) */
            hl_hw_knob_3d(cx, 174, 15, by, norm, act_k ? C42_CYAN : accent, cr, cg, cb, ptr_c);
            /* Recessed Dark-Glass Glowing Value Pill BELOW knob (glows bright when active!) */
            hl_srect(cx - 26, 201, 52, 15, by, act_k ? RGB(18, 36, 44) : RGB(12, 14, 16));
            hl_srect(cx - 26, 201, 52, 1, by, act_k ? C42_CYAN : RGB(4, 4, 5));
            hl_srect(cx - 26, 215, 52, 1, by, act_k ? C42_CYAN : th->rim);
            hl_stext(cx - 22, 203, by, &FONT_T, val, act_k ? C42_CYAN : C_WHITE);
        }
    }

    /* 5. Bottom Stereo VU & Return Rail (y = 224..239) */
    if ((int32_t)(notice_until - now) > 0 && notice_a[0]) {
        hl_srect(2, 224, 236, 15, by, RGB(42, 30, 16));
        hl_srect(2, 224, 236, 1, by, C42_GOLD_BRT);
        hl_stext(6, 226, by, &FONT_T, notice_a, C42_GOLD_BRT);
        hl_stext(94, 226, by, &FONT_T, notice_b, C_WHITE);
    } else {
        hl_srect(2, 224, 236, 15, by, RGB(18, 16, 14));
        hl_srect(2, 224, 236, 1, by, C42_GOLD_DIM);
        hl_stext(6, 226, by, &FONT_T, "L", C42_GREEN);
        hl_srect(16, 229, 76, 5, by, RGB(8, 10, 12));
        hl_srect(16, 229, (int32_t)(br_clampf(st->vu_l * 1.15f, 0.0f, 1.0f) * 76.0f), 5, by, C42_GOLD_BRT);
        hl_stext(98, 226, by, &FONT_T, "R", C42_CYAN);
        hl_srect(108, 229, 76, 5, by, RGB(8, 10, 12));
        hl_srect(108, 229, (int32_t)(br_clampf(st->vu_r * 1.15f, 0.0f, 1.0f) * 76.0f), 5, by, C42_CYAN);
        hl_srect(190, 226, 45, 11, by, RGB(44, 36, 22));
        hl_stext(194, 226, by, &FONT_T, "[HOME]", C42_GOLD_BRT);
    }
}

/* Band 0 (y = 0..47): Anodized Gold & Copper Header Console + Patch & Vibe Card */
static void hl_paint_band0(const helios_state_t *st)
{
    char buf[40], num[8];
    uint32_t vj, fxj, lt, ls, c1p;
    int md;
    uint8_t vibe = helios_patch_vibe(st->patch);
    uint16_t vc = (vibe == 1u) ? C42_GREEN : (vibe == 2u) ? C42_GOLD_BRT : C42_MAGENTA;

    cv_begin(240u, 48u, C42_BG_PANEL);

    /* Top Brushed HELIOS Nameplate Strip in Active Entropy Helios Colorway (y = 1..16) */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(2, 1, 236, 16, 0, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1, th->rim, 0);
        cv_rect(4, 3, 2, 12, th->text_sec);
        cv_text(9, 3, &FONT_T, "HELIOS 1.3", th->text_pri);
    }

    /* Dual-Core SMP Status Pill (CPU0 + CPU1 Load) */
    helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);
    buf[0] = 0;
    hl_cat(buf, md ? "2C:" : "1C:");
    hl_cat_u(buf, plat_cpu_pct());
    hl_cat(buf, "/");
    hl_cat_u(buf, c1p);
    hl_cat(buf, "%");
    cv_rect(92, 3, 66, 12, RGB(14, 24, 20));
    cv_text(95, 3, &FONT_T, buf, C42_GREEN);

    /* Operating Mode & Evolve Pill (Unmistakable PURE HW vs ENTROPY badging!) */
    buf[0] = 0;
    if (ui_pure_mode) {
        hl_cat(buf, "PURE HW");
    } else if (st->perf_mode == HL_PERF_GEN) {
        hl_cat(buf, "ENT:");
        hl_cat(buf, helios_evolve_name(st->evolve));
    } else if (st->perf_mode == HL_PERF_HYBRID) {
        hl_cat(buf, "HYB:");
        hl_cat(buf, st->arp_mode ? helios_arpmode_name(st->arp_mode) : "SPLIT");
    } else {
        hl_cat(buf, "PLAY:");
        hl_cat(buf, st->arp_mode ? helios_arpmode_name(st->arp_mode) : "8V POLY");
    }
    cv_rect(160, 3, 75, 12, ui_pure_mode ? C42_COPPER_DK : RGB(14, 28, 30));
    cv_rect(160, 3, 3, 12, ui_pure_mode ? C42_GOLD_BRT : C42_CYAN);
    cv_text(166, 3, &FONT_T, buf,
            ui_pure_mode ? C_WHITE :
            st->perf_mode == HL_PERF_PLAY ? C42_GREEN :
            st->perf_mode == HL_PERF_HYBRID ? C42_CYAN :
            (st->evolve == 0u ? C42_GOLD_DIM : st->evolve == 1u ? C42_GREEN : st->evolve == 2u ? C42_GOLD_BRT : C42_MAGENTA));

    /* Recessed OLED Patch & Tuning Console Bay (y = 19..46) */
    hl_hw_glass(2, 19, 236, 28, 0);

    /* Patch Number Badge: PURE H01..H16, ENTROPY P01..P36, or ENTROPY User E01..E08 */
    if (ui_pure_mode) {
        unsigned h = cur_pure_idx % HL_NPURE;
        uint8_t used = hl_pure_slot_used(h);
        cv_rect(4, 21, 30, 12, C42_GOLD_BRT);
        num[0] = 'H';
        num[1] = (char)('0' + ((h + 1u) / 10u) % 10u);
        num[2] = (char)('0' + (h + 1u) % 10u);
        num[3] = 0;
        cv_text(7, 21, &FONT_T, num, C42_BG_PANEL);
        cv_text(38, 20, &FONT_XS, used ? "PURE USER PATCH" : "PURE INIT PANEL", used ? C_WHITE : C42_GOLD_BRT);
    } else if (cur_slot_idx >= HL_NPATCHES) {
        unsigned u = cur_slot_idx - HL_NPATCHES;
        cv_rect(4, 21, 30, 12, C42_CYAN);
        num[0] = 'E';
        num[1] = '0';
        num[2] = (char)('1' + u);
        num[3] = 0;
        cv_text(7, 21, &FONT_T, num, C42_BG_PANEL);
        cv_text(38, 20, &FONT_XS, user_bank.used[u] ? "ENTROPY USER" : "ENTROPY INIT", C_WHITE);
    } else {
        cv_rect(4, 21, 30, 12, C42_COPPER_DK);
        num[0] = 'P';
        num[1] = (char)('0' + ((st->patch + 1u) / 10u) % 10u);
        num[2] = (char)('0' + (st->patch + 1u) % 10u);
        num[3] = 0;
        cv_text(7, 21, &FONT_T, num, C_WHITE);
        cv_text(38, 20, &FONT_XS, helios_patch_name(st->patch), C_WHITE);
    }

    /* [PURE] / [CALM] / [LUSH] / [EDGE] or [FRZ] / [USER] Pill */
    cv_rect(194, 21, 41, 12, ui_pure_mode ? C42_COPPER_DK : RGB(22, 22, 28));
    cv_rect(194, 21, 2, 12, st->cart_freeze ? C42_CYAN : ui_pure_mode ? C42_GOLD_BRT : vc);
    cv_text(199, 21, &FONT_T,
            st->cart_freeze ? "FRZ" : (ui_pure_mode ? "PURE" : cur_slot_idx >= HL_NPATCHES ? "USER" : (vibe == 1u ? "CALM" : vibe == 2u ? "LUSH" : "EDGE")),
            st->cart_freeze ? C42_CYAN : ui_pure_mode ? C_WHITE : vc);

    /* Bottom row of Patch Card: Root + Scale / Oct | Perf/Keypad Mode | BPM | Hold/EXT status */
    buf[0] = 0;
    hl_cat(buf, NOTE_NAMES[st->root % 12u]);
    hl_cat(buf, " ");
    if (st->perf_mode != HL_PERF_GEN && st->play_oct != 0) {
        hl_cat(buf, "OCT");
        hl_cat_i(buf, (int)st->play_oct);
    } else {
        hl_cat(buf, helios_scale_name(st->scale));
    }
    cv_text(6, 34, &FONT_T, buf, C42_CYAN);

    cv_text(78, 34, &FONT_T,
            st->perf_mode == HL_PERF_GEN ? helios_keymode_name(st->key_mode) : helios_perfmode_name(st->perf_mode),
            st->perf_mode == HL_PERF_PLAY ? C42_GREEN : st->perf_mode == HL_PERF_HYBRID ? C42_CYAN : C42_GOLD_BRT);

    buf[0] = 0;
    if (st->ext_clk_lock) hl_cat(buf, "CLK:");
    hl_cat_u(buf, (unsigned)st->bpm);
    if (!st->ext_clk_lock) hl_cat(buf, "BPM");
    cv_text(146, 34, &FONT_T, buf, st->ext_clk_lock ? C42_GREEN : C42_COPPER);

    cv_text(196, 34, &FONT_T,
            st->compare_on ? "CMP" :
            (st->ext_src != 0u && st->ext_in_lvl > 0.005f) ? "USB:FX" :
            st->sustain_latch ? "HOLD" :
            (st->ext_in_lvl > 0.005f ? "EXT:ON" : (st->playing ? "PLAY" : "MUTE")),
            st->compare_on ? C42_MAGENTA :
            (st->ext_src != 0u && st->ext_in_lvl > 0.005f) ? C42_CYAN :
            st->sustain_latch ? C42_CYAN :
            (st->ext_in_lvl > 0.005f ? C42_GREEN : (st->playing ? C42_GOLD_MID : C42_MAGENTA)));

    cv_blit(0u, 0u);
}

/* Band 1 (y = 48..95): 6-Drone 30-Saw Constellation Radar + Center Analog Scope & 2D Vector Joystick
 * (OR Top Half of 8x8 Modular CV Pin Matrix when ui_page == PG_MATRIX) */
static void hl_paint_band1(const helios_state_t *st, uint32_t now)
{
    uint32_t i, s, d;
    char buf[16];
    cv_begin(240u, 48u, C42_BG_PANEL);

    if (ui_page == PG_MATRIX) {
        /* ---- TOP HALF OF 8x8 MODULAR CV PIN MATRIX (Column Headers + Rows 0..2) ---- */
        static const char *const DST_SHORT[8] = {"DRNL", "DRNR", "V4VO", "V5VO", "VCFL", "VCFR", "VCA", "CART"};
        hl_draw_box(2, 0, 236, 48, C42_BG_CARD, C42_GOLD_MID);
        cv_text(5, 1, &FONT_T, "8x8 MATRIX", C42_GOLD_BRT);
        for (d = 0; d < HL_NMOD_DST; d++) {
            int32_t dx = 56 + (int32_t)d * 22;
            cv_rect(dx, 1, 21, 10, (d == ui_pin_c) ? C42_COPPER_DK : RGB(14, 15, 20));
            cv_text(dx + 1, 1, &FONT_T, DST_SHORT[d], (d == ui_pin_c) ? C_WHITE : C42_GOLD_BRT);
        }
        for (s = 0; s < 3u; s++) {
            int32_t sy = 12 + (int32_t)s * 12;
            int is_r = (s == ui_pin_r);
            float cv = br_clampf((st->mod_src_val[s] + 1.0f) * 0.5f, 0.0f, 1.0f);
            cv_rect(4, sy, 50, 11, is_r ? C42_BG_SEL : RGB(14, 15, 20));
            cv_text(6, sy, &FONT_T, helios_mod_src_name(s), is_r ? C_WHITE : C42_GOLD_MID);
            hl_draw_hbar(36, sy + 3, 16, 4, cv, C42_GREEN, C42_GOLD_BRT);
            for (d = 0; d < HL_NMOD_DST; d++) {
                int32_t dx = 56 + (int32_t)d * 22;
                int sel = is_r && (d == ui_pin_c);
                int8_t pin = st->pin_matrix[s][d];
                uint16_t pcol = (pin == 1) ? C42_GOLD_BRT : (pin == 2) ? C42_COPPER : (pin < 0) ? C42_CYAN : C42_GOLD_DIM;
                hl_draw_box(dx, sy, 21, 11, sel ? C42_BG_SEL : RGB(12, 13, 18), sel ? C_WHITE : C42_GOLD_DIM);
                if (pin == 0) {
                    cv_rect(dx + 9, sy + 4, 3, 3, C42_GOLD_DIM);
                } else {
                    cv_rect(dx + 2, sy + 2, 17, 7, pcol);
                    cv_text(dx + 3, sy + 1, &FONT_T, pin == 1 ? "+5" : pin == 2 ? "10" : "-7", C42_BG_PANEL);
                }
            }
        }
        cv_blit(0u, 48u);
        return;
    }

    /* ---- LEFT DRONE BANK V1, V2, V3 (x = 2..81) ---- */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(2, 0, 80, 47, 0, th->sr0, th->sg0, th->sb0, th->sr1, th->sg1, th->sb1, th->rim, 0);
    }
    for (i = 0; i < 3u; i++) {
        int32_t ry = 2 + (int32_t)i * 15;
        const hl_drone_cfg_t *dr = &st->drone[i];
        int sel = (ui_page == PG_DRN_1_3 && ui_sel_drone_l == i);
        cv_rect(4, ry, 76, 13, sel ? RGB(48, 38, 20) : RGB(13, 14, 19));
        cv_rect(6, ry + 3, 4, 7, dr->hold ? C42_LED_ON : C42_LED_OFF);
        hl_fmt_note(buf, dr->note);
        cv_text(13, ry + 1, &FONT_T, buf, sel ? C_WHITE : C42_GOLD_BRT);
        /* 5-Saw Animated Phase-Spread Constellation */
        cv_rect(36, ry + 2, 42, 9, RGB(8, 9, 12));
        for (s = 0; s < 5u; s++) {
            float ph = (float)((now * (11u + s * 4u + i * 7u)) % 1000u) * 0.001f;
            int32_t sx = 56 + (int32_t)(fm_sin_turns(ph) * dr->spread * 18.0f);
            cv_rect(sx, ry + 3 + (int32_t)s, 3, 2, (s == 0u && dr->sub) ? C42_CYAN : C42_GOLD_BRT);
        }
    }
    /* Keycap Badge: [EDT] */
    hl_draw_box(58, 1, 22, 11, RGB(20, 22, 28), C42_GOLD_BRT);
    cv_text(60, 1, &FONT_T, "EDT", C42_GOLD_BRT);

    /* ---- CENTER MASTER CONSOLE: 3-MODE PRO STUDIO ANALYZER + 2D JOYSTICK + ENV/GR (x = 84..155) ---- */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(84, 0, 72, 47, 0, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1, th->rim, 0);
    }
    hl_hw_glass(86, 3, 41, 31, 0);
    if (ui_scope_mode == 1u) {
        /* STEREO LISSAJOUS PHASE SCOPE (L vs R) */
        cv_rect(106, 4, 1, 29, RGB(24, 28, 36));
        cv_rect(87, 18, 39, 1, RGB(24, 28, 36));
        for (i = 0; i < 64u; i++) {
            int32_t lx = 106 + (int32_t)(st->scope_l[i] * 17.0f);
            int32_t ly = 18  - (int32_t)(st->scope_r[i] * 13.0f);
            if (lx < 87) lx = 87; else if (lx > 125) lx = 125;
            if (ly < 4)  ly = 4;  else if (ly > 32)  ly = 32;
            cv_rect(lx, ly, 2, 2, (i > 40u) ? C42_CYAN : C42_GOLD_BRT);
        }
    } else if (ui_scope_mode == 2u) {
        /* 16-BAND REAL-TIME SPECTRUM ANALYZER */
        for (i = 0; i < 16u; i++) {
            int32_t bh = (int32_t)(st->spec[i] * 28.0f + 0.5f);
            if (bh < 1) bh = 1; else if (bh > 29) bh = 29;
            cv_rect(87 + (int32_t)i * 2 + (int32_t)(i >> 1), 33 - bh, 2, bh,
                    (bh > 22) ? C42_MAGENTA : (bh > 13) ? C42_GOLD_BRT : C42_GREEN);
        }
    } else {
        /* CLASSIC STEREO WAVEFORM SCOPE */
        cv_rect(86, 18, 41, 1, RGB(24, 28, 36));
        for (i = 0; i < 39u; i++) {
            float sv = st->scope_l[(i * 64u) / 39u];
            int32_t sy = 18 - (int32_t)(sv * 13.0f);
            if (sy < 4) sy = 4;
            if (sy > 32) sy = 32;
            cv_rect(87 + (int32_t)i, sy, 1, 2, C42_GOLD_BRT);
        }
    }
    /* 2D Vector Joystick Pad (x = 129..153, y = 3..33) with REC/LOOP motion border */
    hl_draw_box(129, 3, 25, 31, RGB(6, 8, 12),
                 st->joy_rec_state == 1u ? C42_MAGENTA : st->joy_rec_state == 2u ? C42_GREEN : RGB(24, 24, 30));
    cv_rect(141, 4, 1, 29, C42_GOLD_DIM);
    cv_rect(130, 18, 23, 1, C42_GOLD_DIM);
    {
        int32_t jx = 141 + (int32_t)(st->joy_x * 9.0f);
        int32_t jy = 18 - (int32_t)(st->joy_y * 11.0f);
        cv_rect(jx - 2, jy - 2, 5, 5, st->joy_rec_state == 1u ? C42_MAGENTA : C42_COPPER);
        cv_rect(jx - 1, jy - 1, 3, 3, C_WHITE);
    }
    /* EXT IN Envelope Follower / Spring Tank Kick Flash bar & Comparator Gate LED (y = 35..44) */
    cv_text(87, 35, &FONT_T, st->spring_kick_env > 0.05f ? "SPR" : "ENV",
            st->spring_kick_env > 0.05f ? C42_CYAN : C42_GOLD_MID);
    hl_draw_hbar(107, 38, 36, 5, st->spring_kick_env > 0.05f ? st->spring_kick_env : st->ext_env,
                  st->spring_kick_env > 0.05f ? C42_CYAN : C42_GREEN, C42_GOLD_BRT);
    cv_rect(146, 37, 7, 7, st->ext_comp ? C42_MAGENTA : C42_LED_OFF);

    /* ---- RIGHT DRONE BANK V6, V7, V8 (x = 158..237) ---- */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(158, 0, 80, 47, 0, th->sr0, th->sg0, th->sb0, th->sr1, th->sg1, th->sb1, th->rim, 0);
    }
    for (i = 0; i < 3u; i++) {
        int32_t ry = 2 + (int32_t)i * 15;
        const hl_drone_cfg_t *dr = &st->drone[3u + i];
        int sel = (ui_page == PG_DRN_6_8 && (ui_sel_drone_r % 3u) == i);
        cv_rect(160, ry, 76, 13, sel ? RGB(48, 38, 20) : RGB(13, 14, 19));
        cv_rect(162, ry + 3, 4, 7, dr->hold ? C42_LED_ON : C42_LED_OFF);
        hl_fmt_note(buf, dr->note);
        cv_text(169, ry + 1, &FONT_T, buf, sel ? C_WHITE : C42_GOLD_BRT);
        cv_rect(192, ry + 2, 42, 9, RGB(8, 9, 12));
        for (s = 0; s < 5u; s++) {
            float ph = (float)((now * (13u + s * 3u + i * 5u)) % 1000u) * 0.001f;
            int32_t sx = 212 + (int32_t)(fm_sin_turns(ph) * dr->spread * 18.0f);
            cv_rect(sx, ry + 3 + (int32_t)s, 3, 2, (s == 0u && dr->sub) ? C42_CYAN : C42_GOLD_BRT);
        }
    }
    /* Keycap Badge: [EDT] */
    hl_draw_box(214, 1, 22, 11, RGB(20, 22, 28), C42_GOLD_BRT);
    cv_text(216, 1, &FONT_T, "EDT", C42_GOLD_BRT);

    cv_blit(0u, 48u);
}

/* Band 2 (y = 96..143): Melodic V4 + 5-Step Seq L | Dual Analog VCF & Cartridge FX | Melodic V5 + 5-Step Seq R
 * (OR Bottom Half of 8x8 Modular CV Pin Matrix Rows 3..7 when ui_page == PG_MATRIX) */
static void hl_paint_band2(const helios_state_t *st)
{
    uint32_t s, d;
    char buf[24];
    cv_begin(240u, 48u, C42_BG_PANEL);

    if (ui_page == PG_MATRIX) {
        /* ---- BOTTOM HALF OF 8x8 MODULAR CV PIN MATRIX (Rows 3..6 + Row 7) ---- */
        hl_draw_box(2, 0, 236, 48, C42_BG_CARD, C42_GOLD_MID);
        for (s = 3u; s < HL_NMOD_SRC; s++) {
            int32_t sy = 1 + (int32_t)(s - 3u) * 9;
            int is_r = (s == ui_pin_r);
            float cv = br_clampf((st->mod_src_val[s] + 1.0f) * 0.5f, 0.0f, 1.0f);
            cv_rect(4, sy, 50, 8, is_r ? C42_BG_SEL : RGB(14, 15, 20));
            cv_text(6, sy - 1, &FONT_T, helios_mod_src_name(s), is_r ? C_WHITE : C42_GOLD_MID);
            hl_draw_hbar(38, sy + 2, 14, 4, cv, C42_GREEN, C42_GOLD_BRT);
            for (d = 0; d < HL_NMOD_DST; d++) {
                int32_t dx = 56 + (int32_t)d * 22;
                int sel = is_r && (d == ui_pin_c);
                int8_t pin = st->pin_matrix[s][d];
                uint16_t pcol = (pin == 1) ? C42_GOLD_BRT : (pin == 2) ? C42_COPPER : (pin < 0) ? C42_CYAN : C42_GOLD_DIM;
                hl_draw_box(dx, sy, 21, 8, sel ? C42_BG_SEL : RGB(12, 13, 18), sel ? C_WHITE : C42_GOLD_DIM);
                if (pin == 0) {
                    cv_rect(dx + 9, sy + 3, 2, 2, C42_GOLD_DIM);
                } else {
                    cv_rect(dx + 2, sy + 1, 17, 6, pcol);
                }
            }
        }
        cv_blit(0u, 96u);
        return;
    }

    /* ---- LEFT MELODIC V4 + 5-STEP ANALOG SEQUENCER L (x = 2..78) ---- */
    {
        const hl_mel_cfg_t *m = &st->mel[0];
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(2, 0, 77, 47, 0, th->sr0, th->sg0, th->sb0, th->sr1, th->sg1, th->sb1, th->rim, 0);
        cv_text(5, 2, &FONT_T, "V4:", th->is_light ? RGB(20, 96, 148) : C42_CYAN);
        cv_text(22, 2, &FONT_T, helios_wave_name(m->wave), th->text_pri);

        hl_fmt_note(buf, m->cur_note);
        hl_cat(buf, " ");
        hl_cat_i(buf, (int)(st->plate_trim[m->cur_plate % 12u] * 100.0f));
        hl_cat(buf, "c");
        cv_text(5, 13, &FONT_T, buf, th->text_sec);
        /* Keycap Badge: [SEQ] */
        hl_draw_box(55, 13, 22, 11, RGB(20, 22, 28), C42_GREEN);
        cv_text(57, 13, &FONT_T, "SEQ", C42_GREEN);

        /* 5-Step Sequencer L LEDs & Step Bars (y = 25..44) */
        cv_rect(5, 25, 71, 19, RGB(8, 9, 12));
        for (s = 0; s < HL_NSTEPS; s++) {
            int32_t sx = 8 + (int32_t)s * 13;
            int act = m->seq_run && (m->cur_step == s);
            int sel = (ui_page == PG_SEQ_L && ui_sel_step_l == s);
            int g_on = m->step_gate[s] != 0u;
            cv_rect(sx, 27, 10, 5, act ? (g_on ? C42_LED_ON : C42_MAGENTA) : (sel ? C42_CYAN : C42_LED_OFF));
            {
                int32_t bh = 2 + ((int32_t)m->steps[s] * 8) / 11;
                cv_rect(sx + 1, 42 - bh, 8, bh, g_on ? (act ? C_WHITE : C42_GOLD_MID) : C42_GOLD_DIM);
            }
        }
    }

    /* ---- CENTER DUAL DUAL ANALOG VCF & MAGNETIC CARTRIDGE FX (x = 81..158) ---- */
    {
        static const char *const VMD[4] = {"LP", "BP", "HP", "L+B"};
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(81, 0, 78, 47, 0, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1, th->rim, 0);
        buf[0] = 0;
        hl_cat(buf, "DUAL VCF ");
        hl_cat(buf, VMD[st->vcf_mode[0] & 3u]);
        cv_text(85, 2, &FONT_T, buf, th->text_pri);
        /* Keycap Badge: [FX] */
        hl_draw_box(141, 1, 16, 11, RGB(20, 22, 28), C42_COPPER);
        cv_text(143, 1, &FONT_T, "FX", C42_COPPER);

        cv_text(85, 13, &FONT_T, "CUT", th->text_sec);
        hl_draw_hbar(106, 15, 48, 5, st->vcf_cut[0], C42_COPPER, C42_GOLD_BRT);
        cv_text(85, 23, &FONT_T, "RES", th->text_sec);
        hl_draw_hbar(106, 25, 48, 5, st->vcf_res[0], C42_MAGENTA, C42_GOLD_BRT);

        /* Magnetic FX Cartridge Slot */
        cv_rect(84, 33, 72, 12, RGB(24, 20, 14));
        cv_text(86, 34, &FONT_T, helios_cart_name(st->cart_type), C42_CYAN);
    }

    /* ---- RIGHT MELODIC V5 + 5-STEP ANALOG SEQUENCER R (x = 161..237) ---- */
    {
        const hl_mel_cfg_t *m = &st->mel[1];
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        hl_hw_panel(161, 0, 77, 47, 0, th->sr0, th->sg0, th->sb0, th->sr1, th->sg1, th->sb1, th->rim, 0);
        cv_text(164, 2, &FONT_T, "V5:", th->text_sec);
        cv_text(181, 2, &FONT_T, helios_wave_name(m->wave), th->text_pri);

        hl_fmt_note(buf, m->cur_note);
        hl_cat(buf, " ");
        hl_cat_i(buf, (int)(st->plate_trim[m->cur_plate % 12u] * 100.0f));
        hl_cat(buf, "c");
        cv_text(164, 13, &FONT_T, buf, th->is_light ? RGB(20, 96, 148) : C42_CYAN);
        /* Keycap Badge: [ARP] */
        hl_draw_box(214, 13, 22, 11, RGB(20, 22, 28), C42_CYAN);
        cv_text(216, 13, &FONT_T, "ARP", C42_CYAN);

        /* 5-Step Sequencer R LEDs & Step Bars (y = 25..44) */
        cv_rect(164, 25, 71, 19, RGB(8, 9, 12));
        for (s = 0; s < HL_NSTEPS; s++) {
            int32_t sx = 167 + (int32_t)s * 13;
            int act = m->seq_run && (m->cur_step == s);
            int sel = (ui_page == PG_SEQ_R && ui_sel_step_r == s);
            int g_on = m->step_gate[s] != 0u;
            cv_rect(sx, 27, 10, 5, act ? (g_on ? C42_LED_ON : C42_MAGENTA) : (sel ? C42_GOLD_BRT : C42_LED_OFF));
            {
                int32_t bh = 2 + ((int32_t)m->steps[s] * 8) / 11;
                cv_rect(sx + 1, 42 - bh, 8, bh, g_on ? (act ? C_WHITE : C42_GOLD_MID) : C42_GOLD_DIM);
            }
        }
    }

    cv_blit(0u, 96u);
}

/* Band 3 (y = 144..191): Active Hardware Module Header & 4 Physical Encoder Knobs (K1 .. K4) */
static void hl_paint_band3(const helios_state_t *st)
{
    uint32_t k;
    char hdr[40];
    cv_begin(240u, 48u, C42_BG_PANEL);

    /* Module Strip Title Bar (y = 0..13) */
    hl_draw_box(2, 0, 236, 14, RGB(24, 22, 16), C42_GOLD_MID);
    hdr[0] = 0;
    hl_cat(hdr, PG_TITLE[ui_page % PG_COUNT]);
    if (ui_page == PG_DRN_1_3) {
        hl_cat(hdr, " [V");
        hl_cat_u(hdr, (unsigned)(ui_sel_drone_l + 1u));
        hl_cat(hdr, "]");
    } else if (ui_page == PG_DRN_6_8) {
        hl_cat(hdr, " [V");
        hl_cat_u(hdr, (unsigned)(ui_sel_drone_r + 3u));
        hl_cat(hdr, "]");
    } else if (ui_page == PG_SEQ_L) {
        hl_cat(hdr, " [S");
        hl_cat_u(hdr, (unsigned)(ui_sel_step_l + 1u));
        hl_cat(hdr, "]");
    } else if (ui_page == PG_SEQ_R) {
        hl_cat(hdr, " [S");
        hl_cat_u(hdr, (unsigned)(ui_sel_step_r + 1u));
        hl_cat(hdr, "]");
    }
    cv_text(6, 2, &FONT_T, hdr, C42_GOLD_BRT);
    if (ui_sel_alt) {
        cv_rect(202, 2, 34, 10, C42_COPPER);
        cv_text(206, 2, &FONT_T, "SEL*", C_WHITE);
    }

    /* 4 Physical HELIOS Color-Coded Rotary Encoder Knobs (K1, K2, K3, K4) (y = 15..47) */
    {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        int32_t cr, cg, cb;
        uint16_t ptr_c;
        hl_page_knob_cap(ui_page, &cr, &cg, &cb, &ptr_c);
        for (k = 0; k < 4u; k++) {
            int32_t kx = 2 + (int32_t)k * 59;
            char lbl[16], val[20];
            float norm = 0.0f;
            hl_knob_info(st, ui_page, k, lbl, val, &norm);
            hl_hw_panel(kx, 15, 58, 32, 0, th->sr0, th->sg0, th->sb0, th->sr1, th->sg1, th->sb1,
                         ui_sel_alt ? C42_COPPER : th->rim, 0);
            hl_hw_knob_3d(kx + 11, 31, 7, 0, norm, ui_sel_alt ? C42_CYAN : (th->is_light ? RGB(204, 52, 32) : C42_GOLD_BRT),
                           cr, cg, cb, ptr_c);
            cv_text(kx + 21, 17, &FONT_T, lbl, th->text_pri);
            cv_text(kx + 21, 30, &FONT_T, val, th->is_light ? RGB(20, 20, 22) : C_WHITE);
        }
    }

    cv_blit(0u, 144u);
}

/* Band 4 (y = 192..239):
 *   - GEN mode: 12 Etched Copper Capacitive Touch Plates + Microtonal Trimpots
 *   - HYBRID & PLAY 8V modes: 27-Key Live Velocity & Polyphonic Aftertouch Keyboard Visualizer + Expression Meters */
static void hl_paint_band4(const helios_state_t *st, uint32_t now)
{
    uint32_t p;
    char buf[16];
    cv_begin(240u, 48u, C42_BG_PANEL);

    if (st->perf_mode != HL_PERF_GEN) {
        /* ---- 27-KEY VELOCITY & POLYPHONIC AFTERTOUCH KEYBOARD VISUALIZER (x = 4..235, y = 1..32) ---- */
        static const uint8_t IS_BLACK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
        hl_draw_box(2, 0, 236, 33, C42_BG_CARD, st->perf_mode == HL_PERF_PLAY ? C42_GREEN : C42_CYAN);
        for (p = 0; p < HL_NKEYS; p++) {
            int32_t kx = 6 + (int32_t)p * 8 + (p >= 12u && st->perf_mode == HL_PERF_HYBRID ? 3 : 0);
            uint32_t pitch_cls = (5u + p) % 12u; /* Key 0 = F3 (pitch class 5) */
            int blk = IS_BLACK[pitch_cls];
            int held = (st->held_mask & (1u << p)) != 0u;
            int latched = (st->latch_mask & (1u << p)) != 0u;
            float v_norm = (float)st->key_vel[p] * (1.0f / 127.0f);
            float a_norm = (float)st->key_at[p] * (1.0f / 127.0f);
            uint16_t k_bg = blk ? RGB(22, 20, 26) : RGB(44, 38, 30);
            uint16_t k_bdr = (st->perf_mode == HL_PERF_HYBRID && p < 12u) ? C42_GOLD_MID : C42_GOLD_DIM;
            if (latched) k_bg = C42_COPPER_DK;
            if (held)    k_bg = (a_norm > 0.25f) ? C42_CYAN : C42_COPPER;

            /* Split divider at Key 12 (F4) in HYBRID mode */
            if (p == 12u && st->perf_mode == HL_PERF_HYBRID) {
                cv_rect(kx - 2, 2, 1, 29, C42_CYAN);
            }

            hl_draw_box(kx, 3, 7, blk ? 20 : 28, k_bg, held ? C_WHITE : k_bdr);

            /* Live Velocity + Polyphonic Aftertouch Bar inside Key */
            if (held || latched) {
                int32_t vh = (int32_t)(v_norm * 18.0f) + 2;
                int32_t ah = (int32_t)(a_norm * 22.0f);
                if (vh > 24) vh = 24;
                if (ah > 24) ah = 24;
                cv_rect(kx + 1, 29 - vh, 5, vh, C42_GOLD_BRT);
                if (ah > 0)
                    cv_rect(kx + 2, 29 - ah, 3, ah, C42_GREEN);
            }
        }
    } else {
        /* ---- 12 ETCHED COPPER CAPACITIVE TOUCH PLATES (x = 4..232, 19px per plate, y = 1..32) ---- */
        for (p = 0; p < HL_NPLATES; p++) {
            int32_t px = 4 + (int32_t)p * 19;
            int act4 = (st->mel[0].cur_plate == p);
            int act5 = (st->mel[1].cur_plate == p);
            uint16_t fill = (act4 || act5) ? C42_COPPER : C42_COPPER_DK;
            uint16_t bdr  = act4 ? C42_CYAN : (act5 ? C42_GOLD_BRT : C42_GOLD_MID);
            if ((act4 && st->mel[0].press > 0.45f) || (act5 && st->mel[1].press > 0.45f))
                fill = C42_GOLD_MID;

            /* Split 6+6 Cyan Divider Line between Plate 6 (V4) and Plate 7 (V5) */
            if (p == 6u && st->key_mode == 0u) {
                cv_rect(px - 1, 0, 1, 33, C42_CYAN);
            }

            /* 3D Brushed Etched Copper Capacitive Touch Plate */
            hl_hw_panel(px, 1, 18, 31, 0,
                         (act4 || act5) ? 235 : 138, (act4 || act5) ? 162 : 82, (act4 || act5) ? 84 : 42,
                         (act4 || act5) ? 165 : 74,  (act4 || act5) ? 94 : 40,  (act4 || act5) ? 44 : 20,
                         bdr, 0);
            (void)fill;

            /* Per-Plate Gold Microtonal Trimpot Pip at Top of Plate */
            {
                int32_t tx = px + 8 + (int32_t)(st->plate_trim[p] * 5.0f);
                cv_rect(px + 3, 3, 12, 3, RGB(14, 12, 10));
                cv_rect(tx, 2, 2, 5, st->plate_trim[p] != 0.0f ? C42_CYAN : C42_GOLD_BRT);
            }

            /* Etched Copper Circuit Trace */
            cv_rect(px + 2, 19, 14, 1, C42_GOLD_DIM);

            /* Plate Note Label */
            {
                float p_note = hl_plate_to_note(st->root, st->scale, (uint8_t)p, 0);
                int mn = (int)(p_note + 0.5f);
                cv_text(px + 2, 9, &FONT_T, NOTE_NAMES[mn % 12], (act4 || act5) ? C_WHITE : C42_GOLD_BRT);
            }
            if (act4) cv_text(px + 2, 21, &FONT_T, "4", C42_CYAN);
            if (act5) cv_text(px + 10, 21, &FONT_T, "5", C_WHITE);
        }
    }

    /* Bottom Bar (y = 34..47): Notice Toast OR Velocity/Aftertouch + Stereo VU Meters */
    if ((int32_t)(notice_until - now) > 0 && notice_a[0]) {
        hl_draw_box(2, 34, 236, 13, RGB(34, 26, 16), C42_GOLD_BRT);
        cv_text(6, 36, &FONT_T, notice_a, C42_GOLD_BRT);
        cv_text(88, 36, &FONT_T, notice_b, C_WHITE);
    } else if (st->perf_mode != HL_PERF_GEN) {
        hl_draw_box(2, 34, 236, 13, C42_BG_CARD, C42_GOLD_DIM);
        cv_text(5, 36, &FONT_T, "VEL", C42_GOLD_BRT);
        hl_draw_hbar(26, 38, 46, 5, st->last_vel, C42_COPPER, C42_GOLD_BRT);
        cv_text(76, 36, &FONT_T, "AT", C42_CYAN);
        hl_draw_hbar(92, 38, 52, 5, st->max_at, C42_CYAN, C42_GREEN);
        cv_text(148, 36, &FONT_T, "OUT", st->playing ? C42_GREEN : C42_MAGENTA);
        hl_draw_hbar(170, 38, 62, 5, (st->vu_l + st->vu_r) * 0.65f, C42_COPPER, C42_GOLD_BRT);
    } else {
        hl_draw_box(2, 34, 236, 13, C42_BG_CARD, C42_GOLD_DIM);
        cv_text(6, 36, &FONT_T, st->playing ? "OUT L" : "MUTE", st->playing ? C42_GREEN : C42_MAGENTA);
        hl_draw_hbar(38, 38, 76, 5, st->vu_l * 1.15f, C42_COPPER, C42_GOLD_BRT);
        cv_text(122, 36, &FONT_T, "OUT R", st->playing ? C42_GREEN : C42_MAGENTA);
        hl_draw_hbar(154, 38, 78, 5, st->vu_r * 1.15f, C42_COPPER, C42_GOLD_BRT);
    }
    (void)buf;
    cv_blit(0u, 192u);
}

/* LIVE-ANIMATED P31 PHOSPHOR-GREEN CRT OSCILLOSCOPE BOOT SPLASH (y = 0..239, 1800ms, adapts to active Chassis Finish!) */
static void hl_paint_boot_splash(int32_t by, uint32_t now)
{
    const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
    int32_t cx = 120, cy = 92;
    int32_t dy, x;
    float t_sec = (float)now * 0.001f;
    float amp = 1.0f;
    if (ui_splash_end && (int32_t)(ui_splash_end - now) > 0) {
        float warmup = br_clampf(1.0f - (float)(ui_splash_end - now) * (1.0f / 1800.0f), 0.0f, 1.0f);
        amp = br_clampf(warmup * 4.5f, 0.25f, 1.0f);
    }

    /* 1. Full-Screen Brushed Anodized Metallic Faceplate in Saved Chassis Finish + 4 Slotted Screws */
    hl_hw_panel(0, 0, 240, 240, by, th->pr0, th->pg0, th->pb0, th->pr1, th->pg1, th->pb1, th->rim, 1);

    /* 2. Machined Circular Tektronix CRT Porthole Bezel (r = 62..55) + Dark-Teal Glass (r = 54) */
    hl_sdisc(cx + 2, cy + 3, 62, by, th->is_light ? RGB(92, 86, 72) : RGB(8, 8, 8));
    hl_sdisc(cx, cy, 62, by, RGB(34, 36, 38));
    hl_sdisc(cx, cy, 60, by, RGB(112, 116, 120));
    hl_sdisc(cx, cy, 57, by, RGB(24, 26, 28));
    /* Recessed dark-teal CRT glass with subtle scanlines */
    {
        int32_t gr = 54, gr2 = gr * gr;
        for (dy = -gr; dy <= gr; dy++) {
            int32_t sy = cy + dy;
            if (sy < by || sy >= by + 48) continue;
            {
                int32_t dx = 0;
                while ((dx + 1) * (dx + 1) + dy * dy <= gr2) dx++;
                uint16_t gc = (dy < -24) ? RGB(14, 30, 34)
                            : ((sy & 1) ? RGB(7, 18, 22) : RGB(10, 23, 27));
                hl_srect(cx - dx, sy, dx * 2 + 1, 1, by, gc);
            }
        }
    }
    /* Subtle Etched CRT Graticule Crosshairs & Ticks */
    hl_srect(cx - 50, cy, 100, 1, by, RGB(18, 42, 46));
    hl_srect(cx, cy - 50, 1, 100, by, RGB(18, 42, 46));
    for (x = -40; x <= 40; x += 10) {
        hl_srect(cx + x, cy - 2, 1, 5, by, RGB(24, 56, 60));
        hl_srect(cx - 2, cy + x, 5, 1, by, RGB(24, 56, 60));
    }

    /* 3. LIVE-ANIMATED P31 PHOSPHOR-GREEN ANALOG SOUND WAVE WITH 3-LAYER CRT BLOOM */
    for (x = -48; x <= 48; x += 2) {
        float u = (float)(x + 48) * (1.0f / 96.0f);
        float win = fm_sin_turns(u * 0.5f); /* smooth circular taper near porthole edges */
        float ph = u * 1.85f - t_sec * 1.15f;
        float wv = 0.60f * fm_sin_turns(ph)
                 + 0.30f * fm_sin_turns(ph * 2.0f + 0.18f)
                 + 0.14f * fm_sin_turns(ph * 3.0f - 0.11f);
        int32_t py = cy - (int32_t)(wv * 30.0f * win * amp);
        int32_t px = cx + x;
        /* Layer 1: Soft Outer P31 Green CRT Bloom Halo */
        hl_srect(px - 2, py - 3, 5, 7, by, RGB(14, 76, 40));
        /* Layer 2: Bright P31 Phosphor Green Trace */
        hl_srect(px - 1, py - 1, 3, 4, by, RGB(52, 224, 112));
        /* Layer 3: Luminous White-Green Electron Beam Core */
        hl_srect(px, py, 2, 2, by, RGB(216, 255, 228));
    }

    /* 4. Clean Wide-Tracked Hardware Typography Below Porthole */
    hl_stext(52, 164, by, &FONT_M, "E N T R O P Y", th->text_pri);
    hl_stext(76, 196, by, &FONT_S, "H E L I O S", th->text_sec);
    hl_stext(102, 218, by, &FONT_T, "V1.0 BETA", th->text_pri);
}

static void hl_paint_screen(uint32_t now)
{
    helios_state_t st;
    int32_t by;
    if (ui_splash_end && (int32_t)(ui_splash_end - now) > 0) {
        for (by = 0; by < 240; by += 48) {
            cv_begin(240u, 48u, RGB(12, 12, 12));
            hl_paint_boot_splash(by, now);
            cv_blit(0u, (uint32_t)by);
        }
        return;
    }
    ui_splash_end = 0u;
    helios_get_state(&st);
    hl_paint_band0(&st);
    if (ui_zoom_t0 && (now - ui_zoom_t0) < 360u) {
        for (by = 48; by < 240; by += 48) {
            cv_begin(240u, 48u, RGB(12, 11, 10));
            hl_paint_zoom_anim(by, now);
            cv_blit(0u, (uint32_t)by);
        }
        return;
    }
    if (ui_page != PG_HOME && ui_page != PG_MATRIX) {
        const hl_theme_t *th = &HL_THEMES[ui_theme % 5u];
        for (by = 48; by < 240; by += 48) {
            cv_begin(240u, 48u, th->outer_bg);
            hl_paint_zoomed_module(&st, by, now);
            cv_blit(0u, (uint32_t)by);
        }
        return;
    }
    hl_paint_band1(&st, now);
    hl_paint_band2(&st);
    hl_paint_band3(&st);
    hl_paint_band4(&st, now);
}

void app_splash(void)
{
    int32_t by;
    for (by = 0; by < 240; by += 48) {
        cv_begin(240u, 48u, RGB(12, 12, 12));
        hl_paint_boot_splash(by, 0u);
        cv_blit(0u, (uint32_t)by);
    }
}

void app_ui_init(void)
{
    uint32_t now = plat_ms();
    ui_splash_end = now + 1800u;
    if (ui_splash_end == 0u) ui_splash_end = 1u;
    hl_paint_screen(now);
}

/* ---- Screen Reader Announcement (fm1-base announce.c) ---- */
static char hl_said_last[256];
static void hl_announce(uint32_t now)
{
    char t[256];
    helios_state_t st;
    unsigned i;
    if (!ann_on()) {
        hl_said_last[0] = 0;
        ann_tick(now);
        return;
    }
    helios_get_state(&st);
    t[0] = 0;
    hl_cat(t, "ENTROPY HELIOS. ");
    hl_cat(t, st.playing ? "PLAYING. " : "STOPPED. ");
    hl_cat(t, "PATCH ");
    hl_cat(t, helios_patch_name(st.patch));
    hl_cat(t, ". ");
    hl_cat(t, PG_TITLE[ui_page % PG_COUNT]);
    hl_cat(t, ". EVOLVE ");
    hl_cat(t, helios_evolve_name(st.evolve));
    for (i = 0; t[i] && t[i] == hl_said_last[i]; i++) {}
    if (ann_where()) {
        ann_say(t, 0, ANN_NOW);
    } else if (t[i] != hl_said_last[i]) {
        ann_later(t, 0, 250u, now);
    }
    for (i = 0; t[i]; i++) hl_said_last[i] = t[i];
    hl_said_last[i] = 0;
    ann_tick(now);
}

/* ---- Physical FM-1 Button & Capacitive Key LEDs ---- */
static void hl_update_lights(void)
{
    static uint32_t last_b = 0xFFFFFFFFu, last_k = 0xFFFFFFFFu, last_dk = 0xFFFFFFFFu;
    helios_state_t st;
    uint32_t b = 0u, keys = 0u, dkeys = 0u, k;

    helios_get_state(&st);
    b |= 1u << PG_BTN_OF[ui_page % PG_COUNT];
    if (st.playing) b |= LED_PLAY_GREEN;
    if (!ui_pure_mode && (st.perf_mode != HL_PERF_GEN || st.evolve > 0u)) b |= 1u << B_REC;
    if (st.arp_mode > 0u) b |= 1u << B_ARP;
    if (ui_sel_alt) b |= 1u << B_SEL;

    if (st.perf_mode != HL_PERF_GEN) {
        /* In HYBRID & PLAY 8V modes: Held/Latched keys light brightly; split/octave roots glow softly */
        keys = st.held_mask | st.latch_mask;
        for (k = 0; k < HL_NKEYS; k++) {
            if (!(keys & (1u << k))) {
                if (st.perf_mode == HL_PERF_HYBRID && k < 12u && (k % 12u == 0u || k % 12u == 7u))
                    dkeys |= 1u << k;
                else if ((k % 12u) == 0u)
                    dkeys |= 1u << k;
            }
        }
    } else {
        /* GEN / PURE Touch Plate mode: Bottom-row white keys ONLY (12 flat plates, upper black keys dark!) */
        static const uint8_t WK[16] = { 0u,2u,4u,6u,7u,9u,11u,12u,14u,16u,18u,19u,21u,23u,24u,26u };
        for (k = 0; k < 12u; k++) {
            uint8_t pk = WK[k];
            uint8_t lit = 0u;
            if ((st.mel[0].hold || st.mel[0].env > 0.04f) && st.mel[0].cur_plate == k) lit = 1u;
            if ((st.mel[1].hold || st.mel[1].env > 0.04f) && st.mel[1].cur_plate == k) lit = 1u;
            if (lit) keys |= 1u << pk;
            else     dkeys |= 1u << pk;
        }
        /* Bottom-row F5 & G5 utility keys glow softly */
        dkeys |= (1u << 24) | (1u << 26);
    }

    if (b == last_b && keys == last_k && dkeys == last_dk) return;
    last_b = b; last_k = keys; last_dk = dkeys;
    plat_lights(b, keys, (1u << NB) - 1u, dkeys, 3);
}

void app_input_only(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port))
        hl_handle_midi(pkt);
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    uint32_t buttons = plat_buttons(), pressed = buttons & ~prev_buttons;
    uint32_t keys, newk, relk, k;
    int32_t d;

    prev_buttons = buttons;
    if (pressed && ui_splash_end) ui_splash_end = 0u;
    app_input_only();

    /* 1. Poll 27 Capacitive Touch Keys with Velocity & Continuous Aftertouch Pressure */
    touch_poll();
    keys = touch_keys() | plat_keys();
    newk = keys & ~prev_keys;
    relk = prev_keys & ~keys;
    prev_keys = keys;

    for (k = 0; k < NKEYS; k++) {
        if (newk & (1u << k)) {
            uint8_t v_raw = touch_sensed(k) ? touch_vel(k) : touch_vel_contact(k);
            float vel = v_raw ? (float)v_raw * (1.0f / 127.0f) : 0.8f;
            float prs = (float)touch_pressure(k) * (1.0f / 127.0f);
            /* Live Keyboard Step-Entry Recording when on PG_SEQ_L or PG_SEQ_R! */
            if ((ui_page == PG_SEQ_L || ui_page == PG_SEQ_R) && k < 24u) {
                helios_state_t st;
                unsigned mi = (ui_page == PG_SEQ_R) ? 1u : 0u;
                unsigned si = (mi == 0u ? ui_sel_step_l : ui_sel_step_r) % HL_NSTEPS;
                char s_lbl[16], s_val[16];
                helios_get_state(&st);
                st.mel[mi].steps[si] = (uint8_t)(k % HL_NPLATES);
                st.mel[mi].step_gate[si] = 1u;
                st.mel[mi].cur_plate = st.mel[mi].steps[si];
                helios_set_state(&st);
                s_lbl[0] = 0;
                hl_cat(s_lbl, mi ? "REC V5 S" : "REC V4 S");
                hl_cat_u(s_lbl, si + 1u);
                hl_fmt_note(s_val, hl_plate_to_note(st.root, st.scale, st.mel[mi].steps[si], st.mel[mi].octave));
                app_say(s_lbl, s_val);
                if (mi == 0u) ui_sel_step_l = (uint8_t)((si + 1u) % HL_NSTEPS);
                else          ui_sel_step_r = (uint8_t)((si + 1u) % HL_NSTEPS);
                ui_sysex_dirty = 1u;
            }
            helios_fm1_key_on(k, vel, prs);
            if (ui_midi_out) {
                uint8_t mn = (uint8_t)clamp(53 + (int)k + helios_get_play_oct() * 12, 0, 127);
                uint8_t mv = (uint8_t)clamp((int)(vel * 127.0f + 0.5f), 1, 127);
                plat_midi_out(0x09u | (0x90u << 8) | ((uint32_t)mn << 16) | ((uint32_t)mv << 24));
            }
        } else if (keys & (1u << k)) {
            if (touch_sensed(k) && touch_down(k)) {
                static uint8_t last_tx_at[NKEYS];
                uint8_t pr_b = touch_pressure(k);
                float prs = (float)pr_b * (1.0f / 127.0f);
                helios_fm1_key_pressure(k, prs);
                if (ui_midi_out && (pr_b > last_tx_at[k] + 3u || pr_b + 3u < last_tx_at[k])) {
                    uint8_t mn = (uint8_t)clamp(53 + (int)k + helios_get_play_oct() * 12, 0, 127);
                    last_tx_at[k] = pr_b;
                    plat_midi_out(0x0Au | (0xA0u << 8) | ((uint32_t)mn << 16) | ((uint32_t)(pr_b & 0x7Fu) << 24));
                }
            }
        } else if (relk & (1u << k)) {
            helios_fm1_key_off(k);
            if (ui_midi_out) {
                uint8_t mn = (uint8_t)clamp(53 + (int)k + helios_get_play_oct() * 12, 0, 127);
                plat_midi_out(0x08u | (0x80u << 8) | ((uint32_t)mn << 16));
            }
        }
    }

    /* 2. Illuminated Section Push-Buttons */
    if (pressed & ((1u << B_PLAY) | (1u << B_REC) | (1u << B_OCTDN) | (1u << B_OCTUP) | (1u << B_ARP) | (1u << B_FX))) {
        ui_sysex_dirty = 1u;
    }
    if (pressed & (1u << B_PLAY)) {
        helios_state_t st;
        if (ui_sel_alt) {
            helios_toggle_sustain_latch();
            helios_get_state(&st);
            app_say("SUSTAIN HOLD", st.sustain_latch ? "LATCH ON" : "MOMENTARY");
        } else {
            helios_toggle_play();
            helios_get_state(&st);
            app_say("ENTROPY HELIOS", st.playing ? "PLAYING" : "STOPPED");
        }
    }
    if (pressed & (1u << B_REC)) {
        helios_state_t st;
        if (ui_page == PG_CART && !ui_sel_alt) {
            /* One-click Solo / Clear Stack on FX CART: keeps only currently selected cartridge ON! */
            helios_get_state(&st);
            st.cart_mask = (uint16_t)(1u << (st.cart_type & 15u));
            helios_set_state(&st);
            app_say("STACK SOLO", helios_cart_name(st.cart_type));
        } else if (ui_page == PG_EQ && !ui_sel_alt) {
            /* One-click Flat Reset on Master Parametric EQ */
            helios_eq_reset();
            app_say("MASTER EQ", "RESET FLAT 0dB");
        } else if ((ui_page == PG_SEQ_L || ui_page == PG_SEQ_R) && !ui_sel_alt) {
            /* One-click Phase/Bar Re-Sync on Dual 5-Step Sequencers */
            helios_seq_sync();
            app_say("DUAL SEQ", "SYNC STEP 1");
        } else if (ui_sel_alt) {
            /* 1-Tap Operating Mode Toggle: ENTROPY (Generative ON) <-> PURE (16 Blank INIT HW Slots) */
            hl_set_mode(!ui_pure_mode);
        } else {
            helios_cycle_perf_mode();
            helios_get_state(&st);
            app_say("PERF MODE", (ui_pure_mode && st.perf_mode == HL_PERF_GEN) ? "HW DRONE" : helios_perfmode_name(st.perf_mode));
        }
    }
    if (pressed & (1u << B_HOME)) {
        hl_set_page(PG_HOME);
    }
    if (pressed & (1u << B_EDIT)) {
        hl_set_page((ui_page == PG_DRN_1_3) ? PG_DRN_6_8 : (ui_page == PG_DRN_6_8) ? PG_DRN_ALL : (ui_page == PG_DRN_ALL) ? PG_HOME : PG_DRN_1_3);
    }
    if (pressed & (1u << B_ARP)) {
        if (ui_sel_alt) {
            helios_state_t st;
            helios_cycle_arp_mode();
            helios_get_state(&st);
            app_say("ARPEGGIATOR", helios_arpmode_name(st.arp_mode));
        } else {
            hl_set_page((ui_page == PG_MEL_V4) ? PG_MEL_V5 : (ui_page == PG_MEL_V5) ? PG_HOME : PG_MEL_V4);
        }
    }
    if (pressed & (1u << B_SEQ)) {
        hl_set_page((ui_page == PG_SEQ_L) ? PG_SEQ_R : (ui_page == PG_SEQ_R) ? PG_HOME : PG_SEQ_L);
    }
    if (pressed & (1u << B_FX)) {
        if (ui_sel_alt) {
            helios_kick_spring(0.95f);
            app_say("SPRING TANK", "PIEZO KICK!");
        } else {
            hl_set_page((ui_page == PG_VCF) ? PG_CART : (ui_page == PG_CART) ? PG_HOME : PG_VCF);
        }
    }
    if (pressed & (1u << B_ENV)) {
        hl_set_page((ui_page == PG_PREAMP) ? PG_EQ : (ui_page == PG_EQ) ? PG_HOME : PG_PREAMP);
    }
    if (pressed & (1u << B_LFO)) {
        hl_set_page((ui_page == PG_MATRIX) ? PG_HOME : PG_MATRIX);
    }
    if (pressed & (1u << B_GLO)) {
        hl_set_page((ui_page == PG_GLOBAL) ? PG_HELP : (ui_page == PG_HELP) ? PG_HOME : PG_GLOBAL);
    }
    if (pressed & (1u << B_SEL)) {
        ui_sel_alt ^= 1u;
        app_say("SELECT ALT", ui_sel_alt ? "SECONDARY ON" : "PRIMARY");
    }
    /* SAVE button:
     *   - Short press (no SEL*): Reset current preset (P01..P24 or U01..U08) to its clean baseline!
     *   - SEL + SAVE: Save current sound permanently to User Flash Slot (U01..U08).
     *   - Hold SAVE for 3 seconds: Full Factory Reset ALL presets & settings back to clean factory defaults! */
    {
        static uint32_t save_hold_t0 = 0u;
        static uint8_t  save_hold_fired = 0u;
        if (pressed & (1u << B_SAVE)) {
            save_hold_t0 = now ? now : 1u;
            save_hold_fired = 0u;
        }
        if ((buttons & (1u << B_SAVE)) && save_hold_t0 && !save_hold_fired) {
            uint32_t held_ms = now - save_hold_t0;
            if (held_ms >= 3000u) {
                uint8_t *z;
                uint32_t zi;
                hl_save_t sv;
                save_hold_fired = 1u;
                z = (uint8_t *)&user_bank;
                for (zi = 0; zi < sizeof user_bank; zi++) z[zi] = 0u;
                user_bank.magic = HL_BANK_MAGIC;
                plat_store_save(OBJ_USER_BANK, &user_bank, sizeof user_bank);
                z = (uint8_t *)&pure_bank_a;
                for (zi = 0; zi < sizeof pure_bank_a; zi++) z[zi] = 0u;
                pure_bank_a.magic = HL_BANK_MAGIC;
                plat_store_save(OBJ_PURE_A, &pure_bank_a, sizeof pure_bank_a);
                z = (uint8_t *)&pure_bank_b;
                for (zi = 0; zi < sizeof pure_bank_b; zi++) z[zi] = 0u;
                pure_bank_b.magic = HL_BANK_MAGIC;
                plat_store_save(OBJ_PURE_B, &pure_bank_b, sizeof pure_bank_b);
                ui_pure_mode = 0u;
                cur_slot_idx = 0u;
                cur_pure_idx = 0u;
                ui_sel_alt = 0u;
                ui_scope_mode = 0u;
                helios_set_spec_en(0);
                helios_init(44100.0f);
                hl_make_save(&sv);
                plat_store_save(OBJ_SETTINGS, &sv, sizeof sv);
                save_sig_last = hl_calc_save_sig(&sv);
                app_say("FACTORY RESET", "ALL BANKS CLEAN");
                ui_sysex_dirty = 1u;
            } else if (held_ms >= 1000u) {
                char cd[16];
                cd[0] = 0;
                hl_cat(cd, "IN ");
                hl_cat_u(cd, (3999u - held_ms) / 1000u);
                hl_cat(cd, " SEC...");
                app_say("RESET ALL", cd);
            }
        }
        if (save_hold_t0 && !(buttons & (1u << B_SAVE))) {
            if (!save_hold_fired) {
                if (ui_sel_alt) {
                    char ub[16];
                    ub[0] = 0;
                    if (ui_pure_mode) {
                        hl_save_pure_slot(cur_pure_idx);
                        hl_cat(ub, cur_pure_idx + 1u < 10u ? "SLOT H0" : "SLOT H");
                        hl_cat_u(ub, cur_pure_idx + 1u);
                        app_say("PURE SAVED", ub);
                    } else {
                        unsigned u = (cur_slot_idx >= HL_NPATCHES) ? (cur_slot_idx - HL_NPATCHES) : 0u;
                        hl_save_user_slot(u);
                        hl_cat(ub, "SLOT E0");
                        hl_cat_u(ub, u + 1u);
                        app_say("ENTROPY SAVED", ub);
                    }
                } else {
                    if (ui_pure_mode) {
                        hl_load_pure_slot(cur_pure_idx);
                        app_say("PURE RESET", hl_pure_slot_used(cur_pure_idx) ? "SAVED PATCH" : "INIT PANEL");
                    } else {
                        helios_state_t st;
                        hl_load_slot(cur_slot_idx);
                        helios_get_state(&st);
                        app_say("PATCH RESET", cur_slot_idx >= HL_NPATCHES ? "ENTROPY SLOT" : helios_patch_name(st.patch));
                    }
                }
                ui_sysex_dirty = 1u;
            }
            save_hold_t0 = 0u;
            save_hold_fired = 0u;
        }
    }

    /* OCT- / OCT+ step voices/steps on EDIT/SEQ pages, shift Keyboard Octave in HYBRID/PLAY mode, or step pages */
    if (pressed & ((1u << B_OCTDN) | (1u << B_OCTUP))) {
        int dir = (pressed & (1u << B_OCTUP)) ? 1 : -1;
        helios_state_t st;
        helios_get_state(&st);
        if (ui_page == PG_DRN_1_3) {
            ui_sel_drone_l = (uint8_t)(((int)ui_sel_drone_l + 3 + dir) % 3);
        } else if (ui_page == PG_DRN_6_8) {
            ui_sel_drone_r = (uint8_t)(((int)ui_sel_drone_r + 3 + dir) % 3);
        } else if (ui_page == PG_SEQ_L) {
            ui_sel_step_l = (uint8_t)(((int)ui_sel_step_l + 5 + dir) % 5);
        } else if (ui_page == PG_SEQ_R) {
            ui_sel_step_r = (uint8_t)(((int)ui_sel_step_r + 5 + dir) % 5);
        } else if (st.perf_mode != HL_PERF_GEN && !ui_sel_alt) {
            char ob[12];
            helios_step_play_octave(dir);
            helios_get_state(&st);
            ob[0] = 0;
            hl_cat_i(ob, (int)st.play_oct);
            app_say("KEY OCTAVE", ob);
        } else {
            ui_page = (uint8_t)(((int)ui_page + (int)PG_COUNT + dir) % (int)PG_COUNT);
            app_say("PAGE", PG_TITLE[ui_page]);
        }
    }

    /* 3. Physical Rotary Encoders: PRESET, SELECT, ALGO, K1..K4 */
    d = plat_enc(EN_PRESET);
    if (d) {
        ui_sysex_dirty = 1u;
        if (ui_sel_alt) {
            helios_state_t st;
            helios_get_state(&st);
            st.cart_type = (uint8_t)(((int)st.cart_type + (int)HL_NCART * 16 + (int)d) % (int)HL_NCART);
            helios_set_state(&st);
            app_say("FX CARTRIDGE", helios_cart_name(st.cart_type));
        } else if (ui_pure_mode) {
            int nxt = ((int)cur_pure_idx + (int)HL_NPURE * 16 + (int)d) % (int)HL_NPURE;
            char hb[16];
            hl_load_pure_slot((unsigned)nxt);
            hb[0] = 0;
            hl_cat(hb, nxt + 1 < 10 ? "PURE H0" : "PURE H");
            hl_cat_u(hb, (unsigned)(nxt + 1));
            app_say(hl_pure_slot_used((unsigned)nxt) ? "PURE PATCH" : "PURE INIT", hb);
        } else {
            int nxt = ((int)cur_slot_idx + (int)HL_NTOTAL_P * 16 + (int)d) % (int)HL_NTOTAL_P;
            hl_load_slot((unsigned)nxt);
            if (cur_slot_idx >= HL_NPATCHES) {
                char ub[16];
                ub[0] = 0;
                hl_cat(ub, "ENTROPY E0");
                hl_cat_u(ub, (unsigned)(cur_slot_idx - HL_NPATCHES + 1u));
                app_say("ENTROPY SLOT", ub);
            } else {
                helios_state_t st;
                helios_get_state(&st);
                app_say("PATCH", helios_patch_name(st.patch));
            }
        }
    }

    d = plat_enc(EN_SELECT);
    if (d) {
        int dir = (int)d;
        if (ui_page == PG_DRN_1_3) {
            ui_sel_drone_l = (uint8_t)(((int)ui_sel_drone_l + 30 + dir) % 3);
        } else if (ui_page == PG_DRN_6_8) {
            ui_sel_drone_r = (uint8_t)(((int)ui_sel_drone_r + 30 + dir) % 3);
        } else if (ui_page == PG_SEQ_L) {
            ui_sel_step_l = (uint8_t)(((int)ui_sel_step_l + 50 + dir) % 5);
        } else if (ui_page == PG_SEQ_R) {
            ui_sel_step_r = (uint8_t)(((int)ui_sel_step_r + 50 + dir) % 5);
        } else if (ui_page == PG_MATRIX) {
            unsigned idx = (unsigned)(((int)ui_pin_r * 8 + (int)ui_pin_c + 640 + dir) & 63);
            ui_pin_r = (uint8_t)(idx >> 3);
            ui_pin_c = (uint8_t)(idx & 7u);
        } else {
            helios_state_t st;
            helios_get_state(&st);
            helios_set_evolve((uint8_t)(((int)st.evolve + 40 + dir) & 3));
            helios_get_state(&st);
            app_say("HELIOS EVOLVE", helios_evolve_name(st.evolve));
            ui_sysex_dirty = 1u;
        }
    }

    d = plat_enc(EN_ALGO);
    if (d) {
        helios_state_t st;
        int dir = (int)d;
        ui_sysex_dirty = 1u;
        helios_get_state(&st);
        if (ui_sel_alt) {
            helios_set_perf_mode((uint8_t)(((int)st.perf_mode + 30 + dir) % 3));
            helios_get_state(&st);
            app_say("PERF MODE", (ui_pure_mode && st.perf_mode == HL_PERF_GEN) ? "HW DRONE" : helios_perfmode_name(st.perf_mode));
        } else if (ui_page == PG_DRN_1_3 || ui_page == PG_DRN_6_8) {
            unsigned di = (ui_page == PG_DRN_1_3) ? (ui_sel_drone_l % 3u) : (3u + (ui_sel_drone_r % 3u));
            char gb[16], vb[16];
            st.drone[di].glide = br_clampf(st.drone[di].glide + (float)dir * 0.05f, 0.0f, 1.0f);
            helios_set_state(&st);
            gb[0] = 0; hl_cat(gb, "V"); hl_cat_u(gb, di < 3u ? di + 1u : di + 3u); hl_cat(gb, " GLIDE");
            vb[0] = 0;
            if (st.drone[di].glide < 0.01f) hl_cat(vb, "INSTANT");
            else hl_fmt_pct(vb, st.drone[di].glide);
            app_say(gb, vb);
        } else if (st.perf_mode != HL_PERF_GEN) {
            helios_set_arp_mode((uint8_t)(((int)st.arp_mode + 60 + dir) % 6));
            helios_get_state(&st);
            app_say("ARPEGGIATOR", helios_arpmode_name(st.arp_mode));
        } else {
            st.key_mode = (uint8_t)(((int)st.key_mode + 30 + dir) % 3);
            helios_set_state(&st);
            app_say("KEYPAD MODE", helios_keymode_name(st.key_mode));
        }
    }

    for (k = 0; k < 4u; k++) {
        d = plat_enc(EN_K1 + (int)k);
        if (d) {
            hl_turn_knob(ui_page, k, d);
            ui_sysex_dirty = 1u;
        }
    }

    /* Live Hardware -> WebMIDI Studio Auto-Sync (~110 ms throttled, zero overhead when Studio not connected) */
#ifdef PLAT_TICKS_PER_US
    if (ui_studio_linked && plat_usb_session() != ui_usb_sess_last) {
        ui_studio_linked = 0u;
        ui_sysex_dirty = 0u;
    }
#endif
    if (ui_studio_linked && ui_sysex_dirty && (uint32_t)(now - ui_sysex_last_ms) >= 110u) {
#ifdef PLAT_TICKS_PER_US
        if (plat_sysex_room() >= 280u)
#endif
        {
            ui_sysex_dirty = 0u;
            ui_sysex_last_ms = now;
            ui_cpu_last_ms = now;
            hl_sysex_send_state();
        }
    } else if (ui_studio_linked && (uint32_t)(now - ui_cpu_last_ms) >= 750u) {
#ifdef PLAT_TICKS_PER_US
        if (plat_sysex_room() >= 20u)
#endif
        {
            ui_cpu_last_ms = now;
            hl_sysex_send_cpu();
        }
    }

    /* 4. Paint 240x240 Screen, Update Screen Reader, Update LEDs & Save Tick */
    hl_paint_screen(now);
    hl_announce(now);
    hl_update_lights();
    hl_save_tick(now);
}

#ifdef PLAT_HOST
int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b; (void)c; (void)d;
    if (cmd && a && cmd[0] == 't' && cmd[1] == 'h' && cmd[2] == 'e') {
        /* "theme N" in host script */
        ui_theme = (uint8_t)((a[0] - '0') % 5);
        return 1;
    }
    if (cmd && a && cmd[0] == 'p' && cmd[1] == 'a' && cmd[2] == 't') {
        /* "patch N" in host script */
        unsigned idx = (unsigned)(a[0] - '0');
        if (a[1] >= '0' && a[1] <= '9') idx = idx * 10u + (unsigned)(a[1] - '0');
        helios_load_patch(idx);
        return 1;
    }
    return 0;
}

int app_expect(const char *what, int *got)
{
    helios_state_t st;
    uint32_t vj, fxj, lt, ls, c1p;
    int md;
    helios_get_state(&st);
    helios_core2_stats(&vj, &fxj, &lt, &ls, &c1p, &md);

    if (!strcmp(what, "playing"))  { *got = (int)st.playing; return 1; }
    if (!strcmp(what, "patch"))    { *got = (int)st.patch; return 1; }
    if (!strcmp(what, "evolve"))   { *got = (int)st.evolve; return 1; }
    if (!strcmp(what, "perfmode")) { *got = (int)st.perf_mode; return 1; }
    if (!strcmp(what, "arpmode"))  { *got = (int)st.arp_mode; return 1; }
    if (!strcmp(what, "page"))     { *got = (int)ui_page; return 1; }
    if (!strcmp(what, "keymode"))  { *got = (int)st.key_mode; return 1; }
    if (!strcmp(what, "c2_mode"))  { *got = md; return 1; }
    if (!strcmp(what, "c2_vjobs"))   { *got = (int)vj; return 1; }
    if (!strcmp(what, "c2_fxjobs"))  { *got = (int)fxj; return 1; }
    if (!strcmp(what, "out"))      { *got = (int)((st.vu_l + st.vu_r) * 5000.0f); return 1; }
    return 0;
}
#endif
