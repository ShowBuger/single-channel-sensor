#include <stdio.h>
#include <assert.h>
#include "algo_params.h"

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

/* ---- Test: g_algo starts with compile-time defaults ---- */
static void test_initial_defaults(void)
{
    TEST("g_algo initialised with compile-time defaults");
    assert(g_algo.dead_value          == CONFIG_DEFAULT_DEAD_VALUE);
    assert(g_algo.spike_multiplier    == CONFIG_DEFAULT_SPIKE_MULT);
    assert(g_algo.stability_threshold == CONFIG_DEFAULT_STABILITY_THR);
    PASS();
}

/* ---- Test: AlgoParams_Load copies config fields ---- */
static void test_load_from_config(void)
{
    Config_TypeDef cfg;
    TEST("AlgoParams_Load copies config fields to g_algo");

    Config_SetDefaults(&cfg);
    cfg.dead_value          = 3.14f;
    cfg.spike_multiplier    = 7.77f;
    cfg.stability_threshold = 0.42f;

    AlgoParams_Load(&cfg);

    assert(g_algo.dead_value          == 3.14f);
    assert(g_algo.spike_multiplier    == 7.77f);
    assert(g_algo.stability_threshold == 0.42f);
    PASS();
}

/* ---- Test: AlgoParams_Load does not modify unrelated config fields ---- */
static void test_load_preserves_config(void)
{
    Config_TypeDef cfg;
    TEST("AlgoParams_Load does not modify the Config_TypeDef");

    Config_SetDefaults(&cfg);
    uint8_t saved_flag = cfg.FLAG_RES;
    float   saved_xmap = cfg.X_MAPPING_VALUE;

    AlgoParams_Load(&cfg);

    assert(cfg.FLAG_RES       == saved_flag);
    assert(cfg.X_MAPPING_VALUE == saved_xmap);
    PASS();
}

/* ---- Test: SLIDING_WINDOW_SIZE compile-time constant ---- */
static void test_compile_time_constants(void)
{
    TEST("SLIDING_WINDOW_SIZE and STABILITY_CHECK_SAMPLES are correct");
    assert(SLIDING_WINDOW_SIZE      == 5);
    assert(STABILITY_CHECK_SAMPLES  == 10);
    PASS();
}

int main(void)
{
    printf("=== algo_params tests ===\n");

    test_initial_defaults();
    test_load_from_config();
    test_load_preserves_config();
    test_compile_time_constants();

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
