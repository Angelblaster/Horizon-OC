/*
 * tegra_soctherm.c - Tegra210 / Tegra210b01 SOC_THERM thermal sensor library
 *
 * Derived from NVIDIA's Linux kernel driver (GPL v2).
 * No kernel dependencies.
 */

#include "nxExt/soctherm.h"

/* =========================================================================
 * Internal register definitions
 * ====================================================================== */

#define SENSOR_CONFIG0              0x00
#define SENSOR_CONFIG0_TALL_SHIFT   8

#define SENSOR_CONFIG1                      0x04
#define SENSOR_CONFIG1_TSAMPLE_SHIFT        0
#define SENSOR_CONFIG1_TIDDQ_EN_SHIFT       15
#define SENSOR_CONFIG1_TEN_COUNT_SHIFT      24
#define SENSOR_CONFIG1_TEMP_ENABLE          (1u << 31)

#define SENSOR_CONFIG2              0x08

#define SENSOR_PDIV                 0x1c0
#define SENSOR_PDIV_CPU_MASK        (0xfu << 12)
#define SENSOR_PDIV_GPU_MASK        (0xfu << 8)
#define SENSOR_PDIV_MEM_MASK        (0xfu << 4)
#define SENSOR_PDIV_PLLX_MASK       (0xfu << 0)

#define SENSOR_HOTSPOT_OFF          0x1c4
#define SENSOR_HOTSPOT_CPU_MASK     (0xffu << 16)
#define SENSOR_HOTSPOT_GPU_MASK     (0xffu << 8)
#define SENSOR_HOTSPOT_MEM_MASK     (0xffu << 0)

#define SENSOR_TEMP1                0x1c8
#define SENSOR_TEMP1_CPU_MASK       (0xffffu << 16)
#define SENSOR_TEMP1_GPU_MASK       0xffffu

#define SENSOR_TEMP2                0x1cc
#define SENSOR_TEMP2_MEM_MASK       (0xffffu << 16)
#define SENSOR_TEMP2_PLLX_MASK      0xffffu

/* Readback format */
#define READBACK_VALUE_MASK         0xff00u
#define READBACK_VALUE_SHIFT        8
#define READBACK_ADD_HALF           (1u << 7)
#define READBACK_NEGATE             (1u << 0)

/* Fuse layout — Tegra210 / Tegra210b01 share the same fuse format */
#define FUSE_BASE_CP_MASK           (0x3ffu << 11)
#define FUSE_BASE_CP_SHIFT          11
#define FUSE_BASE_FT_MASK           (0x7ffu << 21)
#define FUSE_BASE_FT_SHIFT          21
#define FUSE_SHIFT_FT_MASK          (0x1fu << 6)
#define FUSE_SHIFT_FT_SHIFT         6

#define FUSE_TSENSOR_CP_BASE_MASK   0x1fffu
#define FUSE_TSENSOR_FT_BASE_MASK   (0x1fffu << 13)
#define FUSE_TSENSOR_FT_BASE_SHIFT  13

#define NOMINAL_CALIB_CP            25
#define NOMINAL_CALIB_FT            105
#define CALIB_COEFFICIENT           1000000LL

/* =========================================================================
 * Per-sensor private data (not exposed in the header)
 * ====================================================================== */

struct tsensor_priv {
    u32  base;       /* register offset within soctherm block */
    int  group_id;   /* TEGRA210_SENSOR_* */
    s32  alpha;      /* fuse correction * 1e6 */
    s32  beta;
};

/* Timing configs */
static const u32 T210_TALL         = 16300;
static const u32 T210_TIDDQ_EN    = 1;
static const u32 T210_TEN_COUNT   = 1;
static const u32 T210_TSAMPLE     = 120;
static const u32 T210_TSAMPLE_ATE = 480;
static const u32 T210_PDIV        = 8;
static const u32 T210_PDIV_ATE    = 8;

static const u32 B01_TALL         = 16300;
static const u32 B01_TIDDQ_EN    = 1;
static const u32 B01_TEN_COUNT   = 1;
static const u32 B01_TSAMPLE     = 240;
static const u32 B01_TSAMPLE_ATE = 480;
static const u32 B01_PDIV        = 12;
static const u32 B01_PDIV_ATE    = 6;

/* ---- Tegra210 (8 sensors) ---------------------------------------------- */

static const struct tegra210_tsensor_info t210_info[] = {
    { "cpu0", 0x098 }, { "cpu1", 0x084 }, { "cpu2", 0x088 }, { "cpu3", 0x12c },
    { "mem0", 0x158 }, { "mem1", 0x15c },
    { "gpu",  0x154 }, { "pllx", 0x160 },
};
static const struct tsensor_priv t210_priv[] = {
    { 0xc0,  TEGRA210_SENSOR_CPU,  1085000,  3244200 },
    { 0xe0,  TEGRA210_SENSOR_CPU,  1126200,   -67500 },
    { 0x100, TEGRA210_SENSOR_CPU,  1098400,  2251100 },
    { 0x120, TEGRA210_SENSOR_CPU,  1108000,   602700 },
    { 0x140, TEGRA210_SENSOR_MEM,  1069200,  3549900 },
    { 0x160, TEGRA210_SENSOR_MEM,  1173700, -6263600 },
    { 0x180, TEGRA210_SENSOR_GPU,  1074300,  2734900 },
    { 0x1a0, TEGRA210_SENSOR_PLLX, 1039700,  6829100 },
};
#define T210_NUM_SENSORS  (sizeof(t210_info) / sizeof(t210_info[0]))

/* ---- Tegra210b01 (6 sensors — no mem) ---------------------------------- */

static const struct tegra210_tsensor_info t210b01_info[] = {
    { "cpu0", 0x098 }, { "cpu1", 0x084 }, { "cpu2", 0x088 }, { "cpu3", 0x12c },
    { "gpu",  0x154 }, { "pllx", 0x160 },
};
static const struct tsensor_priv t210b01_priv[] = {
    { 0xc0,  TEGRA210_SENSOR_CPU,  1085000,  3244200 },
    { 0xe0,  TEGRA210_SENSOR_CPU,  1126200,   -67500 },
    { 0x100, TEGRA210_SENSOR_CPU,  1098400,  2251100 },
    { 0x120, TEGRA210_SENSOR_CPU,  1108000,   602700 },
    { 0x180, TEGRA210_SENSOR_GPU,  1074300,  2734900 },
    { 0x1a0, TEGRA210_SENSOR_PLLX, 1039700,  6829100 },
};
#define T210B01_NUM_SENSORS (sizeof(t210b01_info) / sizeof(t210b01_info[0]))

/* =========================================================================
 * Helpers
 * ====================================================================== */

static inline s32 sign_extend32(u32 value, int index)
{
    u8 shift = (u8)(31 - index);
    return (s32)((s32)((u32)(value << shift)) >> shift);
}

/* Extract a field: (reg & mask) >> ffs(mask)-1 */
static inline u32 reg_get(u32 reg, u32 mask)
{
    u32 m = mask, shift = 0;
    if (!m) return 0;
    while (!(m & 1u)) { m >>= 1; shift++; }
    return (reg & mask) >> shift;
}

/* Set a field: (reg & ~mask) | ((val << ffs(mask)-1) & mask) */
static inline u32 reg_set(u32 reg, u32 mask, u32 val)
{
    u32 m = mask, shift = 0;
    if (!m) return reg;
    while (!(m & 1u)) { m >>= 1; shift++; }
    return (reg & ~mask) | ((val << shift) & mask);
}

static inline const struct tegra210_tsensor_info *info_for(int is_b01)
{
    return is_b01 ? t210b01_info : t210_info;
}

static inline const struct tsensor_priv *priv_for(int is_b01)
{
    return is_b01 ? t210b01_priv : t210_priv;
}

static inline unsigned int sensor_count(int is_b01)
{
    return is_b01 ? (unsigned int)T210B01_NUM_SENSORS
                  : (unsigned int)T210_NUM_SENSORS;
}

/* =========================================================================
 * Public API
 * ====================================================================== */

const struct tegra210_tsensor_info *tegra_soctherm_get_sensors(
        int is_b01, unsigned int *out_count)
{
    *out_count = sensor_count(is_b01);
    return info_for(is_b01);
}

void tegra_soctherm_calc_shared_calib(
        u32 fuse_tsensor_common,
        struct tegra210_shared_calib *out)
{
    u32 val = fuse_tsensor_common;
    s32 shifted_cp, shifted_ft;

    out->base_cp = reg_get(val, FUSE_BASE_CP_MASK);
    out->base_ft = reg_get(val, FUSE_BASE_FT_MASK);

    shifted_ft = (s32)reg_get(val, FUSE_SHIFT_FT_MASK);
    shifted_ft = sign_extend32((u32)shifted_ft, 4);

    /* On Tegra210 SHIFT_CP lives in the low 6 bits of the same register */
    shifted_cp = sign_extend32(val & 0x3fu, 5);

    out->actual_temp_cp = 2 * NOMINAL_CALIB_CP + shifted_cp;
    out->actual_temp_ft = 2 * NOMINAL_CALIB_FT + shifted_ft;
}

int tegra_soctherm_calc_tsensor_calib(
        const struct tegra210_tsensor_info *sensor,
        int                                 is_b01,
        const struct tegra210_shared_calib *shared,
        u32                                 fuse_val,
        u32                                *out_calib)
{
    /* Find the matching priv entry by matching pointer into the info array */
    const struct tegra210_tsensor_info *base_info = info_for(is_b01);
    const struct tsensor_priv          *base_priv = priv_for(is_b01);
    unsigned int n = sensor_count(is_b01);
    unsigned int idx;

    for (idx = 0; idx < n; idx++) {
        if (&base_info[idx] == sensor)
            break;
    }
    if (idx == n) return -1;  /* sensor not from this variant's table */

    const struct tsensor_priv *priv = &base_priv[idx];

    const u32 tsample     = is_b01 ? B01_TSAMPLE     : T210_TSAMPLE;
    const u32 tsample_ate = is_b01 ? B01_TSAMPLE_ATE : T210_TSAMPLE_ATE;
    const u32 pdiv        = is_b01 ? B01_PDIV        : T210_PDIV;
    const u32 pdiv_ate    = is_b01 ? B01_PDIV_ATE    : T210_PDIV_ATE;

    s32 actual_ft = (s32)(shared->base_ft * 32)
                  + sign_extend32(reg_get(fuse_val, FUSE_TSENSOR_FT_BASE_MASK), 12);
    s32 actual_cp = (s32)(shared->base_cp * 64)
                  + sign_extend32(fuse_val & FUSE_TSENSOR_CP_BASE_MASK, 12);

    s32 delta_sens = actual_ft - actual_cp;
    s32 delta_temp = shared->actual_temp_ft - shared->actual_temp_cp;
    s32 mult       = (s32)(pdiv * tsample_ate);
    s32 div_val    = (s32)(tsample * pdiv_ate);

    if (delta_sens == 0 || div_val == 0)
        return -1;

    s64 temp;
    s16 therma, thermb;

    temp   = (s64)delta_temp * (1LL << 13) * mult;
    therma = (s16)(temp / ((s64)delta_sens * div_val));

    temp   = ((s64)actual_ft * shared->actual_temp_cp)
           - ((s64)actual_cp * shared->actual_temp_ft);
    thermb = (s16)(temp / (s64)delta_sens);

    temp   = (s64)therma * priv->alpha;
    therma = (s16)(temp / CALIB_COEFFICIENT);

    temp   = (s64)thermb * priv->alpha + priv->beta;
    thermb = (s16)(temp / CALIB_COEFFICIENT);

    *out_calib = ((u32)(u16)therma << 16) | (u32)(u16)thermb;
    return 0;
}

/* ---- init ---------------------------------------------------------------- */

static inline u32 soctherm_rd(const struct tegra_soctherm_ctx *ctx, u32 off)
{
    return ctx->read(ctx->base_addr, off);
}
static inline void soctherm_wr(const struct tegra_soctherm_ctx *ctx, u32 off, u32 v)
{
    ctx->write(ctx->base_addr, off, v);
}

static void enable_sensor(const struct tegra_soctherm_ctx *ctx,
                          const struct tsensor_priv *p, u32 calib)
{
    int is_b01 = ctx->is_b01;
    u32 base   = p->base;
    u32 val;

    val = (is_b01 ? B01_TALL : T210_TALL) << SENSOR_CONFIG0_TALL_SHIFT;
    soctherm_wr(ctx, base + SENSOR_CONFIG0, val);

    val  = ((is_b01 ? B01_TSAMPLE : T210_TSAMPLE) - 1) << SENSOR_CONFIG1_TSAMPLE_SHIFT;
    val |= (is_b01 ? B01_TIDDQ_EN  : T210_TIDDQ_EN)    << SENSOR_CONFIG1_TIDDQ_EN_SHIFT;
    val |= (is_b01 ? B01_TEN_COUNT : T210_TEN_COUNT)    << SENSOR_CONFIG1_TEN_COUNT_SHIFT;
    val |= SENSOR_CONFIG1_TEMP_ENABLE;
    soctherm_wr(ctx, base + SENSOR_CONFIG1, val);

    soctherm_wr(ctx, base + SENSOR_CONFIG2, calib);
}

void tegra_soctherm_init(
        struct tegra_soctherm_ctx *ctx,
        int                        is_b01,
        u64                        base_addr,
        const u32                 *calib,
        soctherm_read_fn           read_fn,
        soctherm_write_fn          write_fn)
{
    const struct tsensor_priv *privs = priv_for(is_b01);
    unsigned int n = sensor_count(is_b01);
    unsigned int i;
    u32 pdiv_val = 0;
    u32 hspot    = 0;
    u32 pdiv     = is_b01 ? B01_PDIV : T210_PDIV;

    ctx->base_addr   = base_addr;
    ctx->is_b01      = is_b01;
    ctx->read        = read_fn;
    ctx->write       = write_fn;
    ctx->calib       = calib;
    ctx->num_sensors = n;

    /* Build PDIV register: same pdiv value in every group's field */
    pdiv_val = reg_set(pdiv_val, SENSOR_PDIV_CPU_MASK,  pdiv);
    pdiv_val = reg_set(pdiv_val, SENSOR_PDIV_GPU_MASK,  pdiv);
    pdiv_val = reg_set(pdiv_val, SENSOR_PDIV_PLLX_MASK, pdiv);
    if (!is_b01)
        pdiv_val = reg_set(pdiv_val, SENSOR_PDIV_MEM_MASK, pdiv);
    soctherm_wr(ctx, SENSOR_PDIV, pdiv_val);

    /*
     * HOTSPOT_OFF: units are 0.5 °C per LSB.
     * CPU diff = 10 °C → 20, GPU diff = 5 °C → 10, MEM diff = 0.
     */
    hspot = reg_set(hspot, SENSOR_HOTSPOT_CPU_MASK, 20u);
    hspot = reg_set(hspot, SENSOR_HOTSPOT_GPU_MASK, 10u);
    if (!is_b01)
        hspot = reg_set(hspot, SENSOR_HOTSPOT_MEM_MASK, 0u);
    soctherm_wr(ctx, SENSOR_HOTSPOT_OFF, hspot);

    /* Enable all sensors */
    for (i = 0; i < n; i++)
        enable_sensor(ctx, &privs[i], calib[i]);
}

/* ---- temperature readback ------------------------------------------------ */

static int translate_temp(u16 val)
{
    int t = (int)((val & READBACK_VALUE_MASK) >> READBACK_VALUE_SHIFT) * 1000;
    if (val & READBACK_ADD_HALF) t += 500;
    if (val & READBACK_NEGATE)   t  = -t;
    return t;
}

int tegra_soctherm_read_temp(
        const struct tegra_soctherm_ctx *ctx,
        int                              group_id,
        int                             *out_milli)
{
    u32 reg, mask;

    switch (group_id) {
    case TEGRA210_SENSOR_CPU:
        reg = SENSOR_TEMP1; mask = SENSOR_TEMP1_CPU_MASK; break;
    case TEGRA210_SENSOR_GPU:
        reg = SENSOR_TEMP1; mask = SENSOR_TEMP1_GPU_MASK; break;
    case TEGRA210_SENSOR_MEM:
        if (ctx->is_b01) return -1;  /* not present on b01 */
        reg = SENSOR_TEMP2; mask = SENSOR_TEMP2_MEM_MASK; break;
    case TEGRA210_SENSOR_PLLX:
        reg = SENSOR_TEMP2; mask = SENSOR_TEMP2_PLLX_MASK; break;
    default:
        return -1;
    }

    *out_milli = translate_temp((u16)reg_get(soctherm_rd(ctx, reg), mask));
    return 0;
}