/**
 * @file test_hal.c
 * @brief Unit tests for SensorHAL vtable structure and MLX90393 HAL adapter.
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "sensor_hal.h"
#include "mlx90393.h"

/* ── Spy for MLX90393_configureAll ──────────────────────────────────────── */
static MLX90393_Config last_cfg;
static uint8_t cfg_call_count = 0;

uint8_t MLX90393_configureAll(MLX90393_Handle *h, uint8_t id, const MLX90393_Config *cfg)
{
    (void)h; (void)id;
    last_cfg = *cfg;
    cfg_call_count++;
    return 0x00;
}

uint8_t MLX90393_begin(MLX90393_Handle *h, uint8_t a1, uint8_t a0, int d, uint8_t id)
{ (void)h;(void)a1;(void)a0;(void)d;(void)id; return 0; }

uint8_t MLX90393_startMeasurement(MLX90393_Handle *h, uint8_t id, uint8_t flags)
{ (void)h;(void)id;(void)flags; return 0; }

uint8_t MLX90393_readMeasurement(MLX90393_Handle *h, uint8_t id, uint8_t flags, MLX90393_RawData *r)
{ (void)h;(void)id;(void)flags; memset(r,0,sizeof(*r)); return 0; }

MLX90393_Data MLX90393_convertRaw(MLX90393_RawData raw)
{ (void)raw; MLX90393_Data d={0,0,0,0}; return d; }

uint8_t error_process(uint8_t s) { return (s & 0x10) ? 1 : 0; }

/* stubs for other mlx functions that mlx90393.c might reference */
uint8_t MLX90393_sendCommand(MLX90393_Handle *h, uint8_t id, uint8_t cmd)
{ (void)h;(void)id;(void)cmd; return 0; }
uint8_t MLX90393_readRegister(MLX90393_Handle *h, uint8_t id, uint8_t reg, uint16_t *d)
{ (void)h;(void)id;(void)reg;(void)d; return 0; }
uint8_t MLX90393_writeRegister(MLX90393_Handle *h, uint8_t id, uint8_t reg, uint16_t d)
{ (void)h;(void)id;(void)reg;(void)d; return 0; }
uint8_t MLX90393_reset(MLX90393_Handle *h, uint8_t id)
{ (void)h;(void)id; return 0; }
int8_t  MLX90393_selfTest(MLX90393_Handle *h, uint8_t id)
{ (void)h;(void)id; return 0; }
uint8_t MLX90393_getDataByteCount(uint8_t s) { (void)s; return 6; }
uint8_t MLX90393_isBurstMode(uint8_t s)  { (void)s; return 0; }
uint8_t MLX90393_isWOCMode(uint8_t s)    { (void)s; return 0; }
uint8_t MLX90393_isSingleMode(uint8_t s) { (void)s; return 0; }
uint8_t MLX90393_setGainSel(MLX90393_Handle *h, uint8_t id, uint8_t g)
{ (void)h;(void)id;(void)g; return 0; }
uint8_t MLX90393_setResolution(MLX90393_Handle *h, uint8_t id, uint8_t rx, uint8_t ry, uint8_t rz)
{ (void)h;(void)id;(void)rx;(void)ry;(void)rz; return 0; }
uint8_t MLX90393_setOverSampling(MLX90393_Handle *h, uint8_t id, uint8_t osr)
{ (void)h;(void)id;(void)osr; return 0; }
uint8_t MLX90393_setDigitalFiltering(MLX90393_Handle *h, uint8_t id, uint8_t flt)
{ (void)h;(void)id;(void)flt; return 0; }
uint8_t MLX90393_setTemperatureCompensation(MLX90393_Handle *h, uint8_t id, uint8_t en)
{ (void)h;(void)id;(void)en; return 0; }
uint8_t MLX90393_setBurstDataRate(MLX90393_Handle *h, uint8_t id, uint8_t dr)
{ (void)h;(void)id;(void)dr; return 0; }
uint8_t MLX90393_startBurst(MLX90393_Handle *h, uint8_t id, uint8_t flags)
{ (void)h;(void)id;(void)flags; return 0; }
uint8_t MLX90393_exit_mode(MLX90393_Handle *h, uint8_t id)
{ (void)h;(void)id; return 0; }

/* ── Helpers ─────────────────────────────────────────────────────────────── */
static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { printf("  %-50s", name); tests_run++; } while(0)
#define PASS()     do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg)  do { printf("FAIL: %s\n", msg); } while(0)

/* ── Tests ──────────────────────────────────────────────────────────────── */
static void test_vtable_not_null(void)
{
    TEST("g_hal_mlx90393 vtable pointers are non-NULL");
    assert(g_hal_mlx90393.init        != NULL);
    assert(g_hal_mlx90393.configure   != NULL);
    assert(g_hal_mlx90393.start_meas  != NULL);
    assert(g_hal_mlx90393.read_meas   != NULL);
    assert(g_hal_mlx90393.convert_raw != NULL);
    PASS();
}

static void test_hal_name(void)
{
    TEST("g_hal_mlx90393.name == \"MLX90393\"");
    assert(strcmp(g_hal_mlx90393.name, "MLX90393") == 0);
    PASS();
}

static void test_meas_interval(void)
{
    TEST("g_hal_mlx90393.meas_interval_ms == 8");
    assert(g_hal_mlx90393.meas_interval_ms == 8);
    PASS();
}

static void test_handle_size(void)
{
    TEST("g_hal_mlx90393.handle_size == sizeof(MLX90393_Handle)");
    assert(g_hal_mlx90393.handle_size == sizeof(MLX90393_Handle));
    PASS();
}

static void test_configure_maps_fields(void)
{
    TEST("configure() maps SensorHwConfig_t to MLX90393_Config correctly");
    cfg_call_count = 0;
    memset(&last_cfg, 0, sizeof(last_cfg));

    MLX90393_Handle h;
    memset(&h, 0, sizeof(h));
    SensorHwConfig_t hw = { 5, 2, 3, 1, 2, 3, 1 };

    g_hal_mlx90393.configure(&h, 0, &hw);

    assert(cfg_call_count    == 1);
    assert(last_cfg.gain_sel == 5);
    assert(last_cfg.osr      == 2);
    assert(last_cfg.dig_filt == 3);
    assert(last_cfg.res_x    == 1);
    assert(last_cfg.res_y    == 2);
    assert(last_cfg.res_z    == 3);
    assert(last_cfg.tcmp_en  == 1);
    PASS();
}

int main(void)
{
    printf("test_hal\n");
    test_vtable_not_null();
    test_hal_name();
    test_meas_interval();
    test_handle_size();
    test_configure_maps_fields();
    printf("\n%d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
