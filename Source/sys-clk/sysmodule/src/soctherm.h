/*
 * HOS SOCTHERM Driver
 *
 * Based on NVIDIA L4T kernel driver (soctherm.c, tegra210-soctherm.c)
 * Adapted for Nintendo Switch homebrew (libnx 4.9.0)
 * Compatible with T210 (Erista) and T210B01 (Mariko)
 *
 * Licensed under LGPLv3
 */
#ifndef _SOCTHERM_H_
#define _SOCTHERM_H_

#include <switch.h>
#include <string.h>
#include <stdlib.h>

// --- Hardware Constants ---
// These are mapped via kernel capabilities in perms.json
#define SOCTHERM_PA 0x700E2000ULL
#define SOCTHERM_SIZE 0x1000
#define FUSE_PA 0x7000F800ULL
#define FUSE_SIZE 0x400

// --- SOCTHERM Register Offsets (from soctherm.c) ---
#define SENSOR_CONFIG0 0x0
#define SENSOR_CONFIG1 0x4
#define SENSOR_CONFIG2 0x8
#define SENSOR_STATUS0 0xc
#define SENSOR_STATUS1 0x10

// Sensor base addresses
#define SENSOR_CPU_BASE   0xc0
#define SENSOR_GPU_BASE   0x180
#define SENSOR_MEM_BASE   0x140
#define SENSOR_PLLX_BASE  0x1a0

// Global registers
#define SENSOR_PDIV              0x1c0
#define SENSOR_HOTSPOT_OFF       0x1c4
#define SENSOR_TEMP1             0x1c8
#define SENSOR_TEMP2             0x1cc

// --- Register Bitmasks (from soctherm.c) ---
#ifndef BIT
#define BIT(x) (1U << (x))
#endif

#define SENSOR_CONFIG0_STOP                BIT(0)
#define SENSOR_CONFIG0_TALL_SHIFT          8
#define SENSOR_CONFIG0_TALL_MASK           (0xfffff << 8)

#define SENSOR_CONFIG1_TSAMPLE_SHIFT       0
#define SENSOR_CONFIG1_TSAMPLE_MASK        0x3ff
#define SENSOR_CONFIG1_TIDDQ_EN_SHIFT      15
#define SENSOR_CONFIG1_TIDDQ_EN_MASK       (0x3f << 15)
#define SENSOR_CONFIG1_TEN_COUNT_SHIFT     24
#define SENSOR_CONFIG1_TEN_COUNT_MASK      (0x3f << 24)
#define SENSOR_CONFIG1_TEMP_ENABLE         BIT(31)

#define SENSOR_CONFIG2_THERMA_SHIFT        16
#define SENSOR_CONFIG2_THERMA_MASK         (0xffff << 16)
#define SENSOR_CONFIG2_THERMB_SHIFT        0
#define SENSOR_CONFIG2_THERMB_MASK         0xffff

#define SENSOR_STATUS1_TEMP_VALID_MASK     BIT(31)
#define SENSOR_STATUS1_TEMP_MASK           0xffff

#define SENSOR_PDIV_CPU_MASK               (0xf << 12)
#define SENSOR_PDIV_GPU_MASK               (0xf << 8)
#define SENSOR_PDIV_MEM_MASK               (0xf << 4)
#define SENSOR_PDIV_PLLX_MASK              (0xf << 0)

#define SENSOR_HOTSPOT_CPU_MASK            (0xff << 16)
#define SENSOR_HOTSPOT_GPU_MASK            (0xff << 8)
#define SENSOR_HOTSPOT_MEM_MASK            (0xff << 0)

#define SENSOR_TEMP1_CPU_TEMP_MASK         (0xffff << 16)
#define SENSOR_TEMP1_GPU_TEMP_MASK         0xffff
#define SENSOR_TEMP2_MEM_TEMP_MASK         (0xffff << 16)
#define SENSOR_TEMP2_PLLX_TEMP_MASK        0xffff

// Temperature readback format (from soctherm.c)
#define READBACK_VALUE_MASK                0xff00
#define READBACK_VALUE_SHIFT               8
#define READBACK_ADD_HALF                  BIT(7)
#define READBACK_NEGATE                    BIT(0)

// --- Fuse Registers (from tsensor-fuse.c) ---
#define FUSE_TSENSOR_COMMON                0x180
#define FUSE_TSENSOR_CALIB_CP_TS_BASE_MASK 0x1fff
#define FUSE_TSENSOR_CALIB_FT_TS_BASE_MASK (0x1fff << 13)
#define FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT 13

// Fuse calibration offsets (from tegra210-soctherm.c)
#define FUSE_TSENSOR0_CALIB 0x098  // CPU0
#define FUSE_TSENSOR1_CALIB 0x084  // CPU1
#define FUSE_TSENSOR2_CALIB 0x088  // CPU2
#define FUSE_TSENSOR3_CALIB 0x12c  // CPU3
#define FUSE_TSENSOR4_CALIB 0x158  // MEM0
#define FUSE_TSENSOR5_CALIB 0x15c  // MEM1
#define FUSE_TSENSOR6_CALIB 0x154  // GPU
#define FUSE_TSENSOR7_CALIB 0x160  // PLLX

// Nominal calibration points (from tsensor-fuse.c)
#define NOMINAL_CALIB_FT 105
#define NOMINAL_CALIB_CP 25

// Timing
#define SENSOR_STABILIZATION_DELAY_US 2000
#define SENSOR_READ_DELAY_US 100

#define REG_GET_MASK(r, m) (((r) & (m)) >> (__builtin_ffs(m) - 1))
#define REG_SET_MASK(r, m, v) (((r) & ~(m)) | (((v) << (__builtin_ffs(m) - 1)) & (m)))

// --- Types ---
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

// From tegra210-soctherm.c
typedef struct {
    u32 tall;
    u32 tiddq_en;
    u32 ten_count;
    u32 tsample;
    u32 tsample_ate;
    u32 pdiv;
    u32 pdiv_ate;
} TsensorConfig;

// From tsensor-fuse.c
typedef struct {
    s32 alpha;
    s32 beta;
} FuseCorrCoeff;

typedef struct {
    s32 base_cp;
    s32 base_ft;
    s32 actual_temp_cp;
    s32 actual_temp_ft;
} TsensorSharedCalib;

// --- Global State ---
static bool g_soctherm_initialized = false;
static SocRevision g_soc_revision;
static volatile void* g_soctherm_vaddr = NULL;
static volatile void* g_fuse_vaddr = NULL;
static Mutex g_soctherm_mutex = {0};
static u32 g_calib[SENSOR_COUNT];

// Sensor configurations from tegra210-soctherm.c
static const TsensorConfig g_t210_config = {
    .tall = 16300,
    .tiddq_en = 1,
    .ten_count = 1,
    .tsample = 120,
    .tsample_ate = 480,
    .pdiv = 8,
    .pdiv_ate = 8,
};

static const TsensorConfig g_t210b01_config = {
    .tall = 16300,
    .tiddq_en = 1,
    .ten_count = 1,
    .tsample = 240,
    .tsample_ate = 480,
    .pdiv = 12,
    .pdiv_ate = 6,
};

// Fuse correction coefficients from tegra210-soctherm.c
static const FuseCorrCoeff g_fuse_corr[SENSOR_COUNT] = {
    [SENSOR_CPU]  = {.alpha = 1085000, .beta = 3244200},   // Average of CPU0-3
    [SENSOR_GPU]  = {.alpha = 1074300, .beta = 2734900},
    [SENSOR_MEM]  = {.alpha = 1069200, .beta = 3549900},   // Average of MEM0-1
    [SENSOR_PLLX] = {.alpha = 1039700, .beta = 6829100},
};

// Fuse offsets for each sensor type
static const u32 g_fuse_offsets[SENSOR_COUNT] = {
    [SENSOR_CPU]  = FUSE_TSENSOR0_CALIB,
    [SENSOR_GPU]  = FUSE_TSENSOR6_CALIB,
    [SENSOR_MEM]  = FUSE_TSENSOR4_CALIB,
    [SENSOR_PLLX] = FUSE_TSENSOR7_CALIB,
};

// --- Helper Functions ---
static inline s32 sign_extend32(u32 value, int index) {
    u8 shift = 31 - index;
    return (s32)(value << shift) >> shift;
}

static inline s64 div64_s64_precise(s64 a, s32 b) {
    s64 r, al;
    al = a << 16;
    r = (al * 2 + 1) / (2 * b);
    return r >> 16;
}

// From soctherm.c - translate_temp()
static inline int translate_temp(u16 val) {
    int t = ((val & READBACK_VALUE_MASK) >> READBACK_VALUE_SHIFT) * 1000;
    if (val & READBACK_ADD_HALF)
        t += 500;
    if (val & READBACK_NEGATE)
        t *= -1;
    return t;
}

// From tsensor-fuse.c - tegra_calc_shared_calib()
static inline bool calc_shared_calib(TsensorSharedCalib* shared) {
    if (!g_fuse_vaddr) return false;
    
    volatile u32* fuse_common = (volatile u32*)((u8*)g_fuse_vaddr + FUSE_TSENSOR_COMMON);
    u32 val = *fuse_common;
    
    // T210 fuse layout
    u32 fuse_base_cp_mask = 0x3ff;
    u32 fuse_base_cp_shift = 0;
    u32 fuse_base_ft_mask = 0x7ff << 10;
    u32 fuse_base_ft_shift = 10;
    u32 fuse_shift_ft_mask = 0x1f << 21;
    u32 fuse_shift_ft_shift = 21;
    
    // For T210B01, layout is different (from tegra210-soctherm.c)
    if (g_soc_revision == SOC_REVISION_T210B01) {
        fuse_base_cp_mask = 0x3ff << 11;
        fuse_base_cp_shift = 11;
        fuse_base_ft_mask = 0x7ff << 21;
        fuse_base_ft_shift = 21;
        fuse_shift_ft_mask = 0x1f << 6;
        fuse_shift_ft_shift = 6;
    }
    
    shared->base_cp = (val & fuse_base_cp_mask) >> fuse_base_cp_shift;
    shared->base_ft = (val & fuse_base_ft_mask) >> fuse_base_ft_shift;
    
    s32 shifted_ft = (val & fuse_shift_ft_mask) >> fuse_shift_ft_shift;
    shifted_ft = sign_extend32(shifted_ft, 4);
    
    s32 shifted_cp = 0;
    if (g_soc_revision == SOC_REVISION_T210) {
        // T210 needs FUSE_SPARE_REALIGNMENT (0x1fc)
        volatile u32* spare = (volatile u32*)((u8*)g_fuse_vaddr + 0x1fc);
        shifted_cp = sign_extend32(*spare, 5);
    } else {
        shifted_cp = sign_extend32(val, 5);
    }
    
    shared->actual_temp_cp = 2 * NOMINAL_CALIB_CP + shifted_cp;
    shared->actual_temp_ft = 2 * NOMINAL_CALIB_FT + shifted_ft;
    
    return true;
}

// From tsensor-fuse.c - tegra_calc_tsensor_calib()
static inline bool calc_tsensor_calib(const TsensorConfig* cfg,
                                      const TsensorSharedCalib* shared,
                                      const FuseCorrCoeff* corr,
                                      u32 offset,
                                      u32* calib_out) {
    if (!g_fuse_vaddr) return false;
    
    volatile u32* fuse_reg = (volatile u32*)((u8*)g_fuse_vaddr + offset);
    u32 val = *fuse_reg;
    
    s32 actual_tsensor_cp = (shared->base_cp * 64) + sign_extend32(val, 12);
    val = (val & FUSE_TSENSOR_CALIB_FT_TS_BASE_MASK) >> FUSE_TSENSOR_CALIB_FT_TS_BASE_SHIFT;
    s32 actual_tsensor_ft = (shared->base_ft * 32) + sign_extend32(val, 12);
    
    s32 delta_sens = actual_tsensor_ft - actual_tsensor_cp;
    s32 delta_temp = shared->actual_temp_ft - shared->actual_temp_cp;
    
    s32 mult = cfg->pdiv * cfg->tsample_ate;
    s32 div = cfg->tsample * cfg->pdiv_ate;
    
    s64 temp = (s64)delta_temp * (1LL << 13) * mult;
    s16 therma = div64_s64_precise(temp, (s64)delta_sens * div);
    
    temp = ((s64)actual_tsensor_ft * shared->actual_temp_cp) -
           ((s64)actual_tsensor_cp * shared->actual_temp_ft);
    s16 thermb = div64_s64_precise(temp, delta_sens);
    
    temp = (s64)therma * corr->alpha;
    therma = div64_s64_precise(temp, 1000000LL);
    
    temp = (s64)thermb * corr->alpha + corr->beta;
    thermb = div64_s64_precise(temp, 1000000LL);
    
    *calib_out = ((u16)therma << SENSOR_CONFIG2_THERMA_SHIFT) |
                 ((u16)thermb << SENSOR_CONFIG2_THERMB_SHIFT);
    
    return true;
}

// From soctherm.c - enable_tsensor()
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
    
    volatile u32* config0 = (volatile u32*)((u8*)g_soctherm_vaddr + base + SENSOR_CONFIG0);
    volatile u32* config1 = (volatile u32*)((u8*)g_soctherm_vaddr + base + SENSOR_CONFIG1);
    volatile u32* config2 = (volatile u32*)((u8*)g_soctherm_vaddr + base + SENSOR_CONFIG2);
    
    u32 val = cfg->tall << SENSOR_CONFIG0_TALL_SHIFT;
    *config0 = val;
    
    val = (cfg->tsample - 1) << SENSOR_CONFIG1_TSAMPLE_SHIFT;
    val |= cfg->tiddq_en << SENSOR_CONFIG1_TIDDQ_EN_SHIFT;
    val |= cfg->ten_count << SENSOR_CONFIG1_TEN_COUNT_SHIFT;
    val |= SENSOR_CONFIG1_TEMP_ENABLE;
    *config1 = val;
    
    *config2 = g_calib[sensor];
}

// --- Public API ---
static inline bool socthermInit(void* fuse_vaddr, bool isMariko) {
    mutexLock(&g_soctherm_mutex);
    
    if (g_soctherm_initialized) {
        mutexUnlock(&g_soctherm_mutex);
        return true;
    }
    
    // Detect SoC revision
    bool is_mariko = (hosversionAtLeast(8, 0, 0));
    g_soc_revision = (SocRevision)isMariko;
    
    const TsensorConfig* cfg = is_mariko ? &g_t210b01_config : &g_t210_config;
    
    // Use kernel-mapped memory regions (mapped via perms.json)
    // These addresses are directly accessible because of the kernel capabilities
    u64 temp;
    u64 soctherm_map;
    svcQueryMemoryMapping(&soctherm_map, &temp, SOCTHERM_PA, SOCTHERM_SIZE);
    g_soctherm_vaddr = (volatile void*)soctherm_map;
    g_fuse_vaddr = (volatile void*)fuse_vaddr;
    
    // Calculate shared calibration
    TsensorSharedCalib shared;
    if (!calc_shared_calib(&shared)) {
        g_fuse_vaddr = NULL;
        g_soctherm_vaddr = NULL;
        mutexUnlock(&g_soctherm_mutex);
        return false;
    }
    
    // Calculate per-sensor calibration
    for (int i = 0; i < SENSOR_COUNT; i++) {
        // Skip MEM sensor on T210B01
        if (i == SENSOR_MEM && is_mariko) {
            g_calib[i] = 0;
            continue;
        }
        
        if (!calc_tsensor_calib(cfg, &shared, &g_fuse_corr[i], 
                                g_fuse_offsets[i], &g_calib[i])) {
            g_fuse_vaddr = NULL;
            g_soctherm_vaddr = NULL;
            mutexUnlock(&g_soctherm_mutex);
            return false;
        }
    }
    
    // Initialize PDIV register
    volatile u32* pdiv_reg = (volatile u32*)((u8*)g_soctherm_vaddr + SENSOR_PDIV);
    u32 pdiv_val = *pdiv_reg;
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_CPU_MASK, cfg->pdiv);
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_GPU_MASK, cfg->pdiv);
    pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_PLLX_MASK, cfg->pdiv);
    if (!is_mariko) {
        pdiv_val = REG_SET_MASK(pdiv_val, SENSOR_PDIV_MEM_MASK, cfg->pdiv);
    }
    *pdiv_reg = pdiv_val;
    
    // Initialize HOTSPOT register (from tegra210-soctherm.c)
    volatile u32* hotspot_reg = (volatile u32*)((u8*)g_soctherm_vaddr + SENSOR_HOTSPOT_OFF);
    u32 hotspot_val = *hotspot_reg;
    hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_CPU_MASK, 10);  // pllx_hotspot_diff
    hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_GPU_MASK, 5);
    if (!is_mariko) {
        hotspot_val = REG_SET_MASK(hotspot_val, SENSOR_HOTSPOT_MEM_MASK, 0);
    }
    *hotspot_reg = hotspot_val;
    
    // Enable all sensors
    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (i == SENSOR_MEM && is_mariko) continue;
        enable_tsensor((SocthermSensor)i, cfg);
    }
    
    // Wait for stabilization
    svcSleepThread(SENSOR_STABILIZATION_DELAY_US * 1000ULL);
    
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
    
    // Disable all sensors
    if (g_soctherm_vaddr) {
        const u32 bases[] = {SENSOR_CPU_BASE, SENSOR_GPU_BASE, SENSOR_MEM_BASE, SENSOR_PLLX_BASE};
        for (int i = 0; i < 4; i++) {
            volatile u32* config1 = (volatile u32*)((u8*)g_soctherm_vaddr + bases[i] + SENSOR_CONFIG1);
            *config1 &= ~SENSOR_CONFIG1_TEMP_ENABLE;
        }
    }
    
    // No need to unmap - kernel mappings persist
    g_fuse_vaddr = NULL;
    g_soctherm_vaddr = NULL;
    
    g_soctherm_initialized = false;
    mutexUnlock(&g_soctherm_mutex);
}

static inline int socthermReadTemp(SocthermSensor sensor) {
    if (!g_soctherm_initialized || !g_soctherm_vaddr) return -273000;
    
    // MEM sensor not available on T210B01
    if (sensor == SENSOR_MEM && g_soc_revision == SOC_REVISION_T210B01) {
        return -273000;
    }
    
    mutexLock(&g_soctherm_mutex);
    
    u32 temp_offset;
    u32 temp_mask;
    
    switch (sensor) {
        case SENSOR_CPU:
            temp_offset = SENSOR_TEMP1;
            temp_mask = SENSOR_TEMP1_CPU_TEMP_MASK;
            break;
        case SENSOR_GPU:
            temp_offset = SENSOR_TEMP1;
            temp_mask = SENSOR_TEMP1_GPU_TEMP_MASK;
            break;
        case SENSOR_MEM:
            temp_offset = SENSOR_TEMP2;
            temp_mask = SENSOR_TEMP2_MEM_TEMP_MASK;
            break;
        case SENSOR_PLLX:
            temp_offset = SENSOR_TEMP2;
            temp_mask = SENSOR_TEMP2_PLLX_TEMP_MASK;
            break;
        default:
            mutexUnlock(&g_soctherm_mutex);
            return -273000;
    }
    
    volatile u32* temp_reg = (volatile u32*)((u8*)g_soctherm_vaddr + temp_offset);
    u32 raw = REG_GET_MASK(*temp_reg, temp_mask);
    
    mutexUnlock(&g_soctherm_mutex);
    
    return translate_temp((u16)raw);
}

// Convenience functions
static inline int socthermReadCpuTemp(void)  { return socthermReadTemp(SENSOR_CPU); }
static inline int socthermReadGpuTemp(void)  { return socthermReadTemp(SENSOR_GPU); }
static inline int socthermReadMemTemp(void)  { return socthermReadTemp(SENSOR_MEM); }
static inline int socthermReadPllTemp(void)  { return socthermReadTemp(SENSOR_PLLX); }

static inline int socthermMCToC(int millicelsius) {
    return millicelsius / 1000;
}

static inline void socthermMCToCWithDecimal(int millicelsius, int* celsius, int* decimal) {
    if (celsius) *celsius = millicelsius / 1000;
    if (decimal) *decimal = (millicelsius % 1000 + 1000) % 1000;
}

static inline SocRevision socthermGetRevision(void) {
    return g_soc_revision;
}

static inline const char* socthermGetRevisionString(void) {
    switch (g_soc_revision) {
        case SOC_REVISION_T210: return "T210 (Erista)";
        case SOC_REVISION_T210B01: return "T210B01 (Mariko)";
        default: return "Unknown";
    }
}

// Power management callbacks
static inline void socthermOnSleep(void) {
    if (g_soctherm_initialized) {
        mutexLock(&g_soctherm_mutex);
        if (g_soctherm_vaddr) {
            const u32 bases[] = {SENSOR_CPU_BASE, SENSOR_GPU_BASE, SENSOR_MEM_BASE, SENSOR_PLLX_BASE};
            for (int i = 0; i < 4; i++) {
                volatile u32* config1 = (volatile u32*)((u8*)g_soctherm_vaddr + bases[i] + SENSOR_CONFIG1);
                *config1 &= ~SENSOR_CONFIG1_TEMP_ENABLE;
            }
        }
        mutexUnlock(&g_soctherm_mutex);
    }
}

static inline void socthermOnWake(void) {
    if (g_soctherm_initialized) {
        mutexLock(&g_soctherm_mutex);
        if (g_soctherm_vaddr) {
            bool is_mariko = (g_soc_revision == SOC_REVISION_T210B01);
            const TsensorConfig* cfg = is_mariko ? &g_t210b01_config : &g_t210_config;
            
            for (int i = 0; i < SENSOR_COUNT; i++) {
                if (i == SENSOR_MEM && is_mariko) continue;
                enable_tsensor((SocthermSensor)i, cfg);
            }
            svcSleepThread(SENSOR_STABILIZATION_DELAY_US * 1000ULL);
        }
        mutexUnlock(&g_soctherm_mutex);
    }
}

#endif // _SOCTHERM_H_