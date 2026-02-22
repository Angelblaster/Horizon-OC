/*
 * tegra_soctherm.h - Self-contained Tegra210 SOC_THERM thermal sensor library
 *
 * Supports Tegra210 and Tegra210b01.
 * Derived from NVIDIA's Linux kernel driver (GPL v2).
 * No kernel dependencies.
 *
 * Usage:
 *   1. Call tegra_soctherm_calc_shared_calib() with the raw FUSE_TSENSOR_COMMON
 *      register value.
 *   2. Call tegra_soctherm_get_sensors() to get the sensor table, then call
 *      tegra_soctherm_calc_tsensor_calib() once per sensor, reading each
 *      sensor's fuse value from sensor->calib_fuse_offset.
 *   3. Call tegra_soctherm_init() with your MMIO base, is_b01 flag, calib[],
 *      and read/write callbacks.
 *   4. Call tegra_soctherm_read_temp() to get millicelsius readings.
 */

#ifndef TEGRA_SOCTHERM_H
#define TEGRA_SOCTHERM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Primitive types
 * ---------------------------------------------------------------------- */

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* -------------------------------------------------------------------------
 * Sensor group IDs
 * ---------------------------------------------------------------------- */

#define TEGRA210_SENSOR_CPU   0
#define TEGRA210_SENSOR_MEM   1  /**< Tegra210 only — not present on b01 */
#define TEGRA210_SENSOR_GPU   2
#define TEGRA210_SENSOR_PLLX  3

/* -------------------------------------------------------------------------
 * Public data structures
 * ---------------------------------------------------------------------- */

/**
 * struct tegra210_tsensor_info - Static description of one physical tsensor.
 *
 * Returned by tegra_soctherm_get_sensors(). Use calib_fuse_offset to know
 * which fuse register to read when computing calibration.
 */
struct tegra210_tsensor_info {
    const char *name;              /**< e.g. "cpu0", "gpu", "pllx" */
    u32         calib_fuse_offset; /**< byte offset within the fuse register block */
};

/**
 * struct tegra210_shared_calib - Intermediate shared calibration state.
 *
 * Produced by tegra_soctherm_calc_shared_calib(); passed into every call
 * to tegra_soctherm_calc_tsensor_calib().
 */
struct tegra210_shared_calib {
    u32 base_cp;
    u32 base_ft;
    s32 actual_temp_cp; /**< millicelsius */
    s32 actual_temp_ft; /**< millicelsius */
};

/* -------------------------------------------------------------------------
 * Hardware I/O callbacks
 * ---------------------------------------------------------------------- */

/** Read a 32-bit register at (base_addr + offset). */
typedef u32  (*soctherm_read_fn) (u64 base_addr, u32 offset);

/** Write a 32-bit register at (base_addr + offset). */
typedef void (*soctherm_write_fn)(u64 base_addr, u32 offset, u32 value);

/* -------------------------------------------------------------------------
 * Runtime context — treat as opaque
 * ---------------------------------------------------------------------- */

struct tegra_soctherm_ctx {
    u64               base_addr;
    int               is_b01;
    soctherm_read_fn  read;
    soctherm_write_fn write;
    const u32        *calib;
    unsigned int      num_sensors;
};

/* -------------------------------------------------------------------------
 * API
 * ---------------------------------------------------------------------- */

/**
 * tegra_soctherm_get_sensors - Return the sensor table for the chosen variant.
 *
 * @is_b01:    Non-zero for Tegra210b01, zero for Tegra210.
 * @out_count: Set to the number of entries in the returned array.
 *
 * Returns a pointer to a static array; valid forever, do not free.
 * Use this to determine calib[] array size and which fuse offsets to read.
 *
 * Tegra210   : 8 sensors — cpu0-3, mem0-1, gpu, pllx
 * Tegra210b01: 6 sensors — cpu0-3, gpu, pllx  (no mem)
 */
const struct tegra210_tsensor_info *tegra_soctherm_get_sensors(
        int is_b01, unsigned int *out_count);

/**
 * tegra_soctherm_calc_shared_calib - Derive shared calibration from fuse.
 *
 * @fuse_tsensor_common: Raw 32-bit value of FUSE_TSENSOR_COMMON (fuse block
 *                       offset 0x180).
 * @out: Filled with the shared calibration values needed by
 *       tegra_soctherm_calc_tsensor_calib().
 */
void tegra_soctherm_calc_shared_calib(
        u32 fuse_tsensor_common,
        struct tegra210_shared_calib *out);

/**
 * tegra_soctherm_calc_tsensor_calib - Compute SENSOR_CONFIG2 for one sensor.
 *
 * Call once per sensor in the same order as tegra_soctherm_get_sensors().
 *
 * @sensor:    Entry from tegra_soctherm_get_sensors().
 * @is_b01:    Non-zero for Tegra210b01 (selects different timing parameters).
 * @shared:    Output of tegra_soctherm_calc_shared_calib().
 * @fuse_val:  Raw 32-bit value from sensor->calib_fuse_offset in the fuse block.
 * @out_calib: The resulting word to store in your calib[] array.
 *
 * Return: 0 on success, -1 if calibration is degenerate (divide-by-zero).
 */
int tegra_soctherm_calc_tsensor_calib(
        const struct tegra210_tsensor_info *sensor,
        int                                 is_b01,
        const struct tegra210_shared_calib *shared,
        u32                                 fuse_val,
        u32                                *out_calib);

/**
 * tegra_soctherm_init - Initialise the SOC_THERM block and enable all sensors.
 *
 * @ctx:       Caller-allocated context to fill in.
 * @is_b01:    Non-zero for Tegra210b01, zero for Tegra210.
 * @base_addr: Base address of the SOC_THERM register block (passed verbatim
 *             to read_fn/write_fn as the first argument).
 * @calib:     Array of calibration words, one per sensor in the order given
 *             by tegra_soctherm_get_sensors(). Must remain valid for the
 *             lifetime of @ctx.
 * @read_fn:   32-bit register read callback.
 * @write_fn:  32-bit register write callback.
 */
void tegra_soctherm_init(
        struct tegra_soctherm_ctx *ctx,
        int                        is_b01,
        u64                        base_addr,
        const u32                 *calib,
        soctherm_read_fn           read_fn,
        soctherm_write_fn          write_fn);

/**
 * tegra_soctherm_read_temp - Read the current temperature of a sensor group.
 *
 * @ctx:       Initialised context from tegra_soctherm_init().
 * @group_id:  One of TEGRA210_SENSOR_{CPU, MEM, GPU, PLLX}.
 *             TEGRA210_SENSOR_MEM is not available on b01 and will return -1.
 * @out_milli: Temperature in millicelsius.
 *
 * Return: 0 on success, -1 if the group is not present on this variant.
 */
int tegra_soctherm_read_temp(
        const struct tegra_soctherm_ctx *ctx,
        int                              group_id,
        int                             *out_milli);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TEGRA_SOCTHERM_H */