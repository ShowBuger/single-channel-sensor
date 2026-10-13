#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "config_manager.h"

/* Declared in hal_stub.c */
extern void test_flash_reset(void);

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  %-50s", name); \
    tests_run++; \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

/* ---- Test: Config_SetDefaults fills expected values ---- */
static void test_set_defaults(void)
{
    Config_TypeDef cfg;
    TEST("Config_SetDefaults fills expected values");
    Config_SetDefaults(&cfg);
    assert(cfg.schema_ver          == CONFIG_SCHEMA_VERSION);
    assert(cfg.FLAG_RES            == CONFIG_DEFAULT_FLAG_RES);
    assert(cfg.gain_sel            == CONFIG_DEFAULT_GAIN_SEL);
    assert(cfg.osr                 == CONFIG_DEFAULT_OSR);
    assert(cfg.dead_value          == CONFIG_DEFAULT_DEAD_VALUE);
    assert(cfg.spike_multiplier    == CONFIG_DEFAULT_SPIKE_MULT);
    assert(cfg.stability_threshold == CONFIG_DEFAULT_STABILITY_THR);
    PASS();
}

/* ---- Test: Config_SetDefaults checksum is consistent ---- */
static void test_defaults_checksum(void)
{
    Config_TypeDef a, b;
    TEST("Config_SetDefaults checksum is reproducible");
    Config_SetDefaults(&a);
    Config_SetDefaults(&b);
    assert(a.checksum == b.checksum);
    assert(a.checksum != 0);
    PASS();
}

/* ---- Test: Save + Load round-trip ---- */
static void test_save_load_roundtrip(void)
{
    Config_TypeDef saved, loaded;
    uint8_t ret;

    TEST("Config_Save / Config_Load round-trip");
    test_flash_reset();

    Config_SetDefaults(&saved);
    saved.FLAG_RES            = 0x1F;
    saved.X_MAPPING_VALUE     = 0.25f;
    saved.Y_MAPPING_VALUE     = -0.5f;
    saved.Z_MAPPING_VALUE     = 1.0f;
    saved.dead_value          = 5.0f;
    saved.spike_multiplier    = 15.0f;
    saved.stability_threshold = 0.05f;

    ret = Config_Save(&saved);
    assert(ret == 0);

    ret = Config_Load(&loaded);
    assert(ret == 0); /* 0 = valid data found */

    assert(loaded.FLAG_RES            == 0x1F);
    assert(loaded.X_MAPPING_VALUE     == 0.25f);
    assert(loaded.Y_MAPPING_VALUE     == -0.5f);
    assert(loaded.Z_MAPPING_VALUE     == 1.0f);
    assert(loaded.dead_value          == 5.0f);
    assert(loaded.spike_multiplier    == 15.0f);
    assert(loaded.stability_threshold == 0.05f);
    PASS();
}

/* ---- Test: Load on blank flash returns defaults ---- */
static void test_load_blank_flash(void)
{
    Config_TypeDef cfg;
    uint8_t ret;

    TEST("Config_Load on blank flash applies defaults");
    test_flash_reset(); /* all 0xFF — no magic number */

    ret = Config_Load(&cfg);
    assert(ret == 1); /* 1 = defaults applied */
    assert(cfg.FLAG_RES   == CONFIG_DEFAULT_FLAG_RES);
    assert(cfg.dead_value == CONFIG_DEFAULT_DEAD_VALUE);
    PASS();
}

/* ---- Test: Load with corrupt checksum returns defaults ---- */
static void test_load_corrupt_checksum(void)
{
    Config_TypeDef cfg;
    extern uint8_t _test_flash_page[];
    uint8_t ret;

    TEST("Config_Load detects corrupt checksum");
    test_flash_reset();

    Config_SetDefaults(&cfg);
    Config_Save(&cfg);

    /* Flip a byte in the middle of the stored struct */
    _test_flash_page[16] ^= 0xAA;

    ret = Config_Load(&cfg);
    assert(ret == 1); /* defaults applied */
    PASS();
}

/* ---- Test: Config_RestoreDefault writes valid defaults ---- */
static void test_restore_default(void)
{
    Config_TypeDef loaded;
    uint8_t ret;

    TEST("Config_RestoreDefault writes valid defaults to flash");
    test_flash_reset();
    Config_RestoreDefault();

    ret = Config_Load(&loaded);
    assert(ret == 0);
    assert(loaded.FLAG_RES == CONFIG_DEFAULT_FLAG_RES);
    assert(loaded.osr      == CONFIG_DEFAULT_OSR);
    PASS();
}

/* ---- Test: schema_ver mismatch returns defaults ---- */
static void test_schema_mismatch(void)
{
    Config_TypeDef cfg;
    extern uint8_t _test_flash_page[];
    uint8_t ret;

    TEST("Config_Load detects schema version mismatch");
    test_flash_reset();

    Config_SetDefaults(&cfg);
    Config_Save(&cfg);

    /* Corrupt the schema_ver byte (byte 4 in StoredConfig = magic(4) + schema_ver) */
    _test_flash_page[4] = CONFIG_SCHEMA_VERSION + 1;

    ret = Config_Load(&cfg);
    assert(ret == 1);
    PASS();
}

int main(void)
{
    printf("=== config_manager tests ===\n");

    test_set_defaults();
    test_defaults_checksum();
    test_save_load_roundtrip();
    test_load_blank_flash();
    test_load_corrupt_checksum();
    test_restore_default();
    test_schema_mismatch();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
