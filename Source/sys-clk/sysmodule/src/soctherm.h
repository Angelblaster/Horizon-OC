/*
 * HOS SOCTHERM Driver
 *
 * Based on NVIDIA L4T kernel driver (soctherm.c, tegra210-soctherm.c,
 * tegra210b01-soctherm.c, tsensor-fuse.c)
 * Adapted for Nintendo Switch homebrew (libnx)
 * Compatible with T210 (Erista) and T210B01 (Mariko)
 *
 * Licensed under LGPLv3
 */
#ifndef _SOCTHERM_H_
#define _SOCTHERM_H_

#include <switch.h>
#include <string.h>
#include <stdlib.h>
#include <registers.h>

// ---------------------------------------------------------------------------
// Hardware constants
// ---------------------------------------------------------------------------

// SOCTHERM is at 0x7001b000 on T210/T210B01.
// This MUST match SOCTHERM_REGION_BASE in registers.h and the perms.json map.
// The old value (0x700E2000) was completely wrong — that region is FUSE_BYPASS.
#define SOCTHERM_PA    SOCTHERM_REGION_BASE   // 0x7001b000
#define SOCTHERM_SIZE  SOCTHERM_REGION_SIZE   // 0x1000

// ---------------------------------------------------------------------------
// SOCTHERM register offsets  (from L4T drivers/thermal/tegra/soctherm.c)
// Each sensor block starts at a base; CONFIG0/1/2 and STATUS0/1 are relative.
// ---------------------------------------------------------------------------
#define SENSOR_CONFIG0            0x00
#define SENSOR_CONFIG1            0x04
#define SENSOR_CONFIG2            0x08
#define SENSOR_STATUS0            0x0c
#define SENSOR_STATUS1            0x10

// Sensor block bases  (from tegra210-soctherm.c tsensor_group_*)
#define SENSOR_CPU_BASE           0xc0
#define SENSOR_GPU_BASE           0x180
#define SENSOR_MEM_BASE           0x140
#define SENSOR_PLLX_BASE          0x1a0

// Global registers
#define SENSOR_PDIV               0x1c0
#define SENSOR_HOTSPOT_OFF        0x1c4
#define SENSOR_TEMP1              0x1c8   // CPU (hi16) | GPU (lo16)
#define SENSOR_TEMP2              0x1cc   // MEM (hi16) | PLLX (lo16)

// ---------------------------------------------------------------------------
// Register bitmasks
// ---------------------------------------------------------------------------
#ifndef BIT
#define BIT(x) (1U << (x))
#endif

#define SENSOR_CONFIG0_STOP                BIT(0)
#define SENSOR_CONFIG0_TALL_SHIFT          8
#define SENSOR_CONFIG0_TALL_MASK           (0xfffffU << 8)

#define SENSOR_CONFIG1_TSAMPLE_SHIFT       0
#define SENSOR_CONFIG1_TSAMPLE_MASK        0x3ff
#define SENSOR_CONFIG1_TIDDQ_EN_SHIFT      15
#define SENSOR_CONFIG1_TIDDQ_EN_MASK       (0x3fU << 15)
#define SENSOR_CONFIG1_TEN_COUNT_SHIFT     24
#define SENSOR_CONFIG1_TEN_COUNT_MASK      (0x3fU << 24)
#define SENSOR_CONFIG1_TEMP_ENABLE         BIT(31)

#define SENSOR_CONFIG2_THERMA_SHIFT        16
#define SENSOR_CONFIG2_THERMA_MASK         (0xffffU << 16)
#define SENSOR_CONFIG2_THERMB_SHIFT        0
#define SENSOR_CONFIG2_THERMB_MASK         0xffffU

#define SENSOR_PDIV_CPU_MASK               (0xfU << 12)
#define SENSOR_PDIV_GPU_MASK               (0xfU << 8)
#define SENSOR_PDIV_MEM_MASK               (0xfU << 4)
#define SENSOR_PDIV_PLLX_MASK              (0xfU << 0)

#define SENSOR_HOTSPOT_CPU_MASK            (0xffU << 16)
#define SENSOR_HOTSPOT_GPU_MASK            (0xffU << 8)
#define SENSOR_HOTSPOT_MEM_MASK            (0xffU << 0)

#define SENSOR_TEMP1_CPU_TEMP_MASK         (0xffffU << 16)
#define SENSOR_TEMP1_GPU_TEMP_MASK         0xffffU
#define SENSOR_TEMP2_MEM_TEMP_MASK         (0xffffU << 16)
#define SENSOR_TEMP2_PLLX_TEMP_MASK        0xffffU

// Temperature readback format  (from soctherm.c translate_temp())
#define READBACK_VALUE_MASK                0xff00
#define READBACK_VALUE_SHIFT               8
#define READBACK_ADD_HALF                  BIT(7)
#define READBACK_NEGATE                    BIT(0)

// ---------------------------------------------------------------------------
// Fuse calibration offsets
//
// IMPORTANT: board.cpp dumps fuse data with:
//   svcReadDebugProcessMemory(dump, debug, mem_info.addr + 0x800, sizeof(dump))
// where dump is char[0x400] and then memcpy'd into u32 fuse[0x400].
// So fuse[0] corresponds to physical fuse byte offset 0x800.
// The register names (R_FUSE_*) in registers.h are absolute byte offsets
// within the fuse hardware page starting at 0x7000F800.
// Therefore, the byte offset within the dump[] / fuse[] array is:
//   R_FUSE_XXX - 0x800  ... but wait — the dump starts at mem_info.addr+0x800
// and mem_info.addr IS the fuse hardware page base (0x7000F800 region), so:
//   byte index into fuse[] = R_FUSE_XXX_CALIB_0 directly
//   (the 0x800 subtraction is handled by the dump offset already being +0x800
//    relative to the *page*, so registers.h offsets map 1:1 into dump/fuse[])
//
// Cross-check: R_FUSE_TSENSOR_COMMON_0 = 0x280.
// 0x280 / 4 = 0xA0 = 160. fuse array has 0x100 = 256 u32s populated.  OK.
// R_FUSE_SPARE_REALIGNMENT_REG_0 = 0x37c. 0x37c/4 = 0xDF = 223. OK.
//
// The fuse CALIB offsets below are byte offsets into fuse[], matching
// exactly the R_FUSE_* defines in registers.h.
// ---------------------------------------------------------------------------

// byte offsets into fuse[] for per-sensor calib words (from registers.h)
#define FUSE_OFF_TSENSOR0   R_FUSE_TSENSOR0_CALIB_0   // 0x198  CPU0
#define FUSE_OFF_TSENSOR1   R_FUSE_TSENSOR1_CALIB_0   // 0x184  CPU1
#define FUSE_OFF_TSENSOR2   R_FUSE_TSENSOR2_CALIB_0   // 0x188  CPU2
#define FUSE_OFF_TSENSOR3   R_FUSE_TSENSOR3_CALIB_0   // 0x22c  CPU3
#define FUSE_OFF_TSENSOR4   R_FUSE_TSENSOR4_CALIB_0   // 0x254  MEM0
#define FUSE_OFF_TSENSOR5   R_FUSE_TSENSOR5_CALIB_0   // 0x258  MEM1
#define FUSE_OFF_TSENSOR6   R_FUSE_TSENSOR6_CALIB_0   // 0x25c  GPU
#define FUSE_OFF_TSENSOR7   R_FUSE_TSENSOR7_CALIB_0   // 0x260  PLLX
#define FUSE_OFF_COMMON     R_FUSE_TSENSOR_COMMON_0   // 0x280
#define FUSE_OFF_SPARE      R_FUSE_SPARE_REALIGNMENT_REG_0  // 0x37c

// Nominal calibration temperatures (from L4T tsensor-fuse.c)
#define NOMINAL_CALIB_FT    105   // °C
#define NOMINAL_CALIB_CP    25    // °C

// Sensor stabilisation delay after re-enabling sensors
#define SENSOR_STABILIZATION_DELAY_US  2000

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
#define REG_GET_MASK(r, m)    (((r) & (m)) >> (__builtin_ffs(m) - 1))
#define REG_SET_MASK(r, m, v) (((r) & ~(m)) | (((v) << (__builtin_ffs(m) - 1)) & (m)))

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef enum {
    SOC_REVISION_T210,
    SOC_REVISION_T210B01,
} SocRevision;

typedef enum {
    SENSOR_CPU,
    SENSOR_GPU,
    SENSOR_MEM,
    SENSOR_PLLX,
    SENSOR_COUNT
} SocthermSensor;

// From tegra210-soctherm.c  (tsensor_group configuration)
typedef struct {
    u32 tall;
    u32 tiddq_en;
    u32 ten_count;
    u32 tsample;
    u32 tsample_ate;
    u32 pdiv;
    u32 pdiv_ate;
} TsensorConfig;

// Fuse correction coefficients (from tegra210-soctherm.c)
typedef struct {
    s32 alpha;
    s32 beta;
} FuseCorrCoeff;

// Shared calibration intermediate values
typedef struct {
    s32 base_cp;
    s32 base_ft;
    s32 actual_temp_cp;   // in units of 0.5 °C  (i.e. 2 * °C)
    s32 actual_temp_ft;   // in units of 0.5 °C
} TsensorSharedCalib;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static bool        g_soctherm_initialized = false;
static SocRevision g_soc_revision;
static volatile u8* g_soctherm_vaddr = NULL;   // mapped SOCTHERM registers
static const u32*   g_fuse_data      = NULL;   // pointer into board.cpp fuse[]
static Mutex        g_soctherm_mutex = {0};
static u32          g_calib[SENSOR_COUNT];     // packed therma|thermb per sensor

// ---------------------------------------------------------------------------
// Sensor configurations from tegra210-soctherm.c / tegra210b01-soctherm.c
// ---------------------------------------------------------------------------
static const TsensorConfig g_t210_config = {
    .tall        = 16300,
    .tiddq_en    = 1,
    .ten_count   = 1,
    .tsample     = 120,
    .tsample_ate = 480,
    .pdiv        = 8,
    .pdiv_ate    = 8,
};

static const TsensorConfig g_t210b01_config = {
    .tall        = 16300,
    .tiddq_en    = 1,
    .ten_count   = 1,
    .tsample     = 240,
    .tsample_ate = 480,
    .pdiv        = 12,
    .pdiv_ate    = 6,
};

// Fuse correction coefficients (from tegra210-soctherm.c)
// These scale the raw therma/thermb to account for process variation.
static const FuseCorrCoeff g_fuse_corr[SENSOR_COUNT] = {
    [SENSOR_CPU]  = {.alpha = 1085000, .beta = 3244200},
    [SENSOR_GPU]  = {.alpha = 1074300, .beta = 2734900},
    [SENSOR_MEM]  = {.alpha = 1069200, .beta = 3549900},
    [SENSOR_PLLX] = {.alpha = 1039700, .beta = 6829100},
};

// Fuse byte offsets into fuse[] array for each sensor
static const u32 g_fuse_sensor_offsets[SENSOR_COUNT] = {
    [SENSOR_CPU]  = FUSE_OFF_TSENSOR0,   // CPU0 is the representative CPU sensor
    [SENSOR_GPU]  = FUSE_OFF_TSENSOR6,
    [SENSOR_MEM]  = FUSE_OFF_TSENSOR4,   // MEM0
    [SENSOR_PLLX] = FUSE_OFF_TSENSOR7,
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Read a u32 from the fuse snapshot at a byte offset
static inline u32 fuse_read(u32 byte_offset) {
    // fuse[] is u32[], so byte_offset must be 4-aligned.
    // All R_FUSE_* defines are 4-byte aligned, so this is safe.
    return g_fuse_data[byte_offset / 4];
}

static inline s32 sign_extend32(u32 value, int last_bit) {
    u8 shift = 31 - last_bit;
    return (s32)(value << shift) >> shift;
}

static inline s64 div64_s64_precise(s64 a, s32 b) {
    // Round-to-nearest with extra precision bit
    s64 al = a << 16;
    s64 r  = (al * 2 + 1) / (2 * b);
    return r >> 16;
}

// From soctherm.c - translate_temp()
// Converts the packed 16-bit hardware temperature word to millicelsius.
static inline int translate_temp(u16 val) {
    int t = ((val & READBACK_VALUE_MASK) >> READBACK_VALUE_SHIFT) * 1000;
    if (val & READBACK_ADD_HALF)
        t += 500;
    if (val & READBACK_NEGATE)
        t *= -1;
    return t;
}

// ---------------------------------------------------------------------------
// Calibration  (ported from L4T tsensor-fuse.c)
// ---------------------------------------------------------------------------

// tegra_calc_shared_calib()
// Extracts the shared CP/FT calibration temperatures from the COMMON fuse.
// Returns false if fuse data not available.
static inline bool calc_shared_calib(TsensorSharedCalib* shared) {
    if (!g_fuse_data) return false;

    u32 val = fuse_read(FUSE_OFF_COMMON);

    // -------------------------------------------------------------------
    // T210 (Erista) fuse layout for FUSE_TSENSOR_COMMON (0x280):
    //   [9:0]   base_cp  (unsigned 10-bit)
    //   [20:10] base_ft  (unsigned 11-bit)
    //   [25:21] shift_ft (signed 5-bit, sign-extended)
    //
    // T210B01 (Mariko) fuse layout for FUSE_TSENSOR_COMMON (0x280):
    //   [20:11] base_cp  (unsigned 10-bit)
    //   [31:21] base_ft  (unsigned 11-bit)
    //   [11:6]  shift_ft (signed 6-bit, sign-extended) -- from SPARE_REALIGNMENT
    //
    // Both T210 and T210B01 store shifted_cp in FUSE_SPARE_REALIGNMENT_REG (0x37c).
    // -------------------------------------------------------------------

    s32 shifted_ft, shifted_cp;

    if (g_soc_revision == SOC_REVISION_T210B01) {
        // T210B01 bit layout
        shared->base_cp = (val >> 11) & 0x3ff;
        shared->base_ft = (val >> 21) & 0x7ff;
        shifted_ft = sign_extend32((val >> 6) & 0x1f, 4);
    } else {
        // T210 bit layout
        shared->base_cp = (val >>  0) & 0x3ff;
        shared->base_ft = (val >> 10) & 0x7ff;
        shifted_ft = sign_extend32((val >> 21) & 0x1f, 4);
    }

    // shifted_cp lives in FUSE_SPARE_REALIGNMENT_REG on both SoC variants
    u32 spare = fuse_read(FUSE_OFF_SPARE);
    shifted_cp = sign_extend32(spare & 0x3f, 5);

    // actual_temp is in units of 0.5 °C  (nominal * 2 + shift)
    shared->actual_temp_cp = 2 * NOMINAL_CALIB_CP + shifted_cp;
    shared->actual_temp_ft = 2 * NOMINAL_CALIB_FT + shifted_ft;

    return true;
}

// tegra_calc_tsensor_calib()
// Computes the packed therma/thermb calibration word for one sensor.
// Returns false if fuse data not available.
static inline bool calc_tsensor_calib(const TsensorConfig*     cfg,
                                      const TsensorSharedCalib* shared,
                                      const FuseCorrCoeff*      corr,
                                      u32                       fuse_byte_offset,
                                      u32*                      calib_out) {
    if (!g_fuse_data) return false;

    u32 val = fuse_read(fuse_byte_offset);

    // Lower 13 bits: CP delta (signed); upper 13 bits: FT delta (signed)
    // From L4T tsensor-fuse.c:
    //   actual_tsensor_cp = base_cp * 64 + sign_extend(val[12:0], 12)
    //   actual_tsensor_ft = base_ft * 32 + sign_extend(val[25:13], 12)
    s32 cp_raw = sign_extend32(val & 0x1fff, 12);
    s32 ft_raw = sign_extend32((val >> 13) & 0x1fff, 12);

    s32 actual_tsensor_cp = (shared->base_cp * 64) + cp_raw;
    s32 actual_tsensor_ft = (shared->base_ft * 32) + ft_raw;

    s32 delta_sens = actual_tsensor_ft - actual_tsensor_cp;
    s32 delta_temp = shared->actual_temp_ft - shared->actual_temp_cp;

    if (delta_sens == 0) return false;

    s32 mult = cfg->pdiv * cfg->tsample_ate;
    s32 div  = cfg->tsample * cfg->pdiv_ate;

    // therma  = (delta_temp << 13) * mult / (delta_sens * div)
    s64 temp  = (s64)delta_temp * (1LL << 13) * mult;
    s16 therma = (s16)div64_s64_precise(temp, (s64)delta_sens * div);

    // thermb  = (tsensor_ft * actual_temp_cp - tsensor_cp * actual_temp_ft) / delta_sens
    temp = ((s64)actual_tsensor_ft * shared->actual_temp_cp) -
           ((s64)actual_tsensor_cp * shared->actual_temp_ft);
    s16 thermb = (s16)div64_s64_precise(temp, delta_sens);

    // Apply correction coefficients
    temp   = (s64)therma * corr->alpha;
    therma = (s16)div64_s64_precise(temp, 1000000LL);

    temp   = (s64)thermb * corr->alpha + corr->beta;
    thermb = (s16)div64_s64_precise(temp, 1000000LL);

    *calib_out = ((u32)(u16)therma << SENSOR_CONFIG2_THERMA_SHIFT) |
                 ((u32)(u16)thermb << SENSOR_CONFIG2_THERMB_SHIFT);
    return true;
}

// ---------------------------------------------------------------------------
// Sensor enable/disable
// ---------------------------------------------------------------------------

static inline void enable_tsensor(SocthermSensor sensor, const TsensorConfig* cfg) {
    if (!g_soctherm_vaddr) return;

    u32 base;
    switch (sensor) {
        case SENSOR_CPU:  base = SENSOR_CPU_BASE;  break;
        case SENSOR_GPU:  base = SENSOR_GPU_BASE;  break;
        case SENSOR_MEM:  base = SENSOR_MEM_BASE;  break;
        case SENSOR_PLLX: base = SENSOR_PLLX_BASE; break;
        default: return;
    }

    volatile u32* config0 = (volatile u32*)(g_soctherm_vaddr + base + SENSOR_CONFIG0);
    volatile u32* config1 = (volatile u32*)(g_soctherm_vaddr + base + SENSOR_CONFIG1);
    volatile u32* config2 = (volatile u32*)(g_soctherm_vaddr + base + SENSOR_CONFIG2);

    // CONFIG0: set TALL, clear STOP
    u32 v0 = cfg->tall << SENSOR_CONFIG0_TALL_SHIFT;
    v0 &= ~SENSOR_CONFIG0_STOP;
    *config0 = v0;

    // CONFIG1: tsample, tiddq_en, ten_count, enable
    u32 v1 = (cfg->tsample - 1) << SENSOR_CONFIG1_TSAMPLE_SHIFT;
    v1 |= cfg->tiddq_en  << SENSOR_CONFIG1_TIDDQ_EN_SHIFT;
    v1 |= cfg->ten_count << SENSOR_CONFIG1_TEN_COUNT_SHIFT;
    v1 |= SENSOR_CONFIG1_TEMP_ENABLE;
    *config1 = v1;

    // CONFIG2: write packed therma/thermb calibration
    *config2 = g_calib[sensor];
}

static inline void disable_tsensor(SocthermSensor sensor) {
    if (!g_soctherm_vaddr) return;

    u32 base;
    switch (sensor) {
        case SENSOR_CPU:  base = SENSOR_CPU_BASE;  break;
        case SENSOR_GPU:  base = SENSOR_GPU_BASE;  break;
        case SENSOR_MEM:  base = SENSOR_MEM_BASE;  break;
        case SENSOR_PLLX: base = SENSOR_PLLX_BASE; break;
        default: return;
    }

    volatile u32* config1 = (volatile u32*)(g_soctherm_vaddr + base + SENSOR_CONFIG1);
    *config1 &= ~SENSOR_CONFIG1_TEMP_ENABLE;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/*
 * socthermInit()
 *
 * @param fuse_data  Pointer to the u32 fuse[] array filled by fuseReadSpeedos()
 *                   in board.cpp.  The array stores fuse words with byte offset
 *                   index (i.e. fuse[0x280/4] == FUSE_TSENSOR_COMMON word).
 * @param isMariko   true for T210B01 (Mariko / OLED), false for T210 (Erista).
 *
 * Must be called AFTER fuseReadSpeedos() has populated fuse[].
 * Must be called AFTER svcQueryMemoryMapping has mapped SOCTHERM_REGION_BASE.
 */
static inline bool socthermInit(const u32* fuse_data, bool isMariko) {
    mutexLock(&g_soctherm_mutex);

    if (g_soctherm_initialized) {
        mutexUnlock(&g_soctherm_mutex);
        return true;
    }

    g_soc_revision = isMariko ? SOC_REVISION_T210B01 : SOC_REVISION_T210;
    g_fuse_data    = fuse_data;

    const TsensorConfig* cfg = isMariko ? &g_t210b01_config : &g_t210_config;

    // Map SOCTHERM registers using the kernel capability granted in perms.json.
    // perms.json must have: {"address": "0x7001b000", "size": "0x1000", ...}
    u64 mapped_va = 0, mapped_size = 0;
    Result rc = svcQueryMemoryMapping(&mapped_va, &mapped_size, SOCTHERM_PA, SOCTHERM_SIZE);
    if (R_FAILED(rc)) {
        g_fuse_data = NULL;
        mutexUnlock(&g_soctherm_mutex);
        return false;
    }
    g_soctherm_vaddr = (volatile u8*)mapped_va;

    // Calculate shared calibration data from fuse snapshot
    TsensorSharedCalib shared = {0};
    if (!calc_shared_calib(&shared)) {
        g_fuse_data      = NULL;
        g_soctherm_vaddr = NULL;
        mutexUnlock(&g_soctherm_mutex);
        return false;
    }

    // Calculate per-sensor calibration words
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i == SENSOR_MEM && isMariko) {
            // MEM sensor not present on T210B01
            g_calib[i] = 0;
            continue;
        }
        if (!calc_tsensor_calib(cfg, &shared, &g_fuse_corr[i],
                                g_fuse_sensor_offsets[i], &g_calib[i])) {
            g_fuse_data      = NULL;
            g_soctherm_vaddr = NULL;
            mutexUnlock(&g_soctherm_mutex);
            return false;
        }
    }

    // Set PDIV register
    volatile u32* pdiv_reg = (volatile u32*)(g_soctherm_vaddr + SENSOR_PDIV);
    u32 pdiv_val = *pdiv_reg;
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_CPU_MASK,  cfg->pdiv);
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_GPU_MASK,  cfg->pdiv);
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_PLLX_MASK, cfg->pdiv);
    if (!isMariko)
        pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_MEM_MASK, cfg->pdiv);
    *pdiv_reg = pdiv_val;

    // Set HOTSPOT offsets (from tegra210-soctherm.c)
    volatile u32* hotspot_reg = (volatile u32*)(g_soctherm_vaddr + SENSOR_HOTSPOT_OFF);
    u32 hotspot_val = *hotspot_reg;
    hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_CPU_MASK, 10);
    hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_GPU_MASK, 5);
    if (!isMariko)
        hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_MEM_MASK, 0);
    *hotspot_reg = hotspot_val;

    // Enable all sensors
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i == SENSOR_MEM && isMariko) continue;
        enable_tsensor((SocthermSensor)i, cfg);
    }

    // Wait for sensors to stabilise
    svcSleepThread((u64)SENSOR_STABILIZATION_DELAY_US * 1000ULL);

    g_soctherm_initialized = true;
    mutexUnlock(&g_soctherm_mutex);
    return true;
}

static inline void socthermExit(void) {
    mutexLock(&g_soctherm_mutex);

    if (!g_soctherm_initialized) {
        mutexUnlock(&g_soctherm_mutex);
        return;
    }

    if (g_soctherm_vaddr) {
        for (int i = 0; i < SENSOR_COUNT; i++)
            disable_tsensor((SocthermSensor)i);
    }

    g_soctherm_vaddr = NULL;
    g_fuse_data      = NULL;
    g_soctherm_initialized = false;
    mutexUnlock(&g_soctherm_mutex);
}

// Read a sensor temperature in millicelsius. Returns -273000 on error.
static inline int socthermReadTemp(SocthermSensor sensor) {
    if (!g_soctherm_initialized || !g_soctherm_vaddr) return -273000;

    if (sensor == SENSOR_MEM && g_soc_revision == SOC_REVISION_T210B01)
        return -273000;

    mutexLock(&g_soctherm_mutex);

    u32 temp_reg_offset, temp_mask;

    switch (sensor) {
        case SENSOR_CPU:
            temp_reg_offset = SENSOR_TEMP1;
            temp_mask       = SENSOR_TEMP1_CPU_TEMP_MASK;
            break;
        case SENSOR_GPU:
            temp_reg_offset = SENSOR_TEMP1;
            temp_mask       = SENSOR_TEMP1_GPU_TEMP_MASK;
            break;
        case SENSOR_MEM:
            temp_reg_offset = SENSOR_TEMP2;
            temp_mask       = SENSOR_TEMP2_MEM_TEMP_MASK;
            break;
        case SENSOR_PLLX:
            temp_reg_offset = SENSOR_TEMP2;
            temp_mask       = SENSOR_TEMP2_PLLX_TEMP_MASK;
            break;
        default:
            mutexUnlock(&g_soctherm_mutex);
            return -273000;
    }

    volatile u32* reg = (volatile u32*)(g_soctherm_vaddr + temp_reg_offset);
    u32 raw = REG_GET_MASK(*reg, temp_mask);

    mutexUnlock(&g_soctherm_mutex);

    return translate_temp((u16)raw);
}

// Convenience wrappers
static inline int socthermReadCpuTemp(void)  { return socthermReadTemp(SENSOR_CPU); }
static inline int socthermReadGpuTemp(void)  { return socthermReadTemp(SENSOR_GPU); }
static inline int socthermReadMemTemp(void)  { return socthermReadTemp(SENSOR_MEM); }
static inline int socthermReadPllTemp(void)  { return socthermReadTemp(SENSOR_PLLX); }

static inline int socthermMCToC(int millicelsius) { return millicelsius / 1000; }

static inline void socthermMCToCWithDecimal(int millicelsius, int* celsius, int* decimal) {
    if (celsius) *celsius  = millicelsius / 1000;
    if (decimal) *decimal  = (millicelsius % 1000 + 1000) % 1000;
}

static inline SocRevision socthermGetRevision(void) { return g_soc_revision; }

static inline const char* socthermGetRevisionString(void) {
    return (g_soc_revision == SOC_REVISION_T210B01) ? "T210B01 (Mariko)" : "T210 (Erista)";
}

// ---------------------------------------------------------------------------
// Power management callbacks
// ---------------------------------------------------------------------------

static inline void socthermOnSleep(void) {
    if (!g_soctherm_initialized) return;
    mutexLock(&g_soctherm_mutex);
    for (int i = 0; i < SENSOR_COUNT; i++)
        disable_tsensor((SocthermSensor)i);
    mutexUnlock(&g_soctherm_mutex);
}

static inline void socthermOnWake(void) {
    if (!g_soctherm_initialized) return;
    mutexLock(&g_soctherm_mutex);
    bool isMariko = (g_soc_revision == SOC_REVISION_T210B01);
    const TsensorConfig* cfg = isMariko ? &g_t210b01_config : &g_t210_config;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i == SENSOR_MEM && isMariko) continue;
        enable_tsensor((SocthermSensor)i, cfg);
    }
    svcSleepThread((u64)SENSOR_STABILIZATION_DELAY_US * 1000ULL);
    mutexUnlock(&g_soctherm_mutex);
}

#endif // _SOCTHERM_H_
