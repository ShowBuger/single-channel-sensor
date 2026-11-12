/* STM32H7 port: system init, clock tree, reset cause, watchdog. */
#include "plat.h"
#include "board_pins.h"

static uint32_t          s_reset_flags;
static IWDG_HandleTypeDef s_iwdg;

static void clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* LDO supply, VOS1: 400 MHz is allowed on both rev Y and rev V silicon. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* HSI64 -> PLL1: 64 MHz / M4 * N50 / P2 = 400 MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState            = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState            = RCC_LSI_ON;              /* IWDG */
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 4;                       /* 16 MHz */
    osc.PLL.PLLN            = 50;                      /* 800 MHz VCO */
    osc.PLL.PLLP            = 2;                       /* 400 MHz SYSCLK */
    osc.PLL.PLLQ            = 4;
    osc.PLL.PLLR            = 2;
    osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_3;      /* 8-16 MHz input */
    osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN        = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { while (1) {} }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2   | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;   /* CPU   400 MHz */
    clk.AHBCLKDivider  = RCC_HCLK_DIV2;     /* HCLK  200 MHz */
    clk.APB3CLKDivider = RCC_APB3_DIV2;     /* 100 MHz */
    clk.APB1CLKDivider = RCC_APB1_DIV2;     /* 100 MHz */
    clk.APB2CLKDivider = RCC_APB2_DIV2;     /* 100 MHz (USART1) */
    clk.APB4CLKDivider = RCC_APB4_DIV2;     /* 100 MHz */
    /* VOS1 + HCLK 200 MHz -> 2 wait states */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) { while (1) {} }
}

void plat_sys_init(void)
{
    s_reset_flags = RCC->RSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    SCB_EnableICache();
    SCB_EnableDCache();
    HAL_Init();
    clock_config();
}

uint32_t plat_reset_flags(void)       { return s_reset_flags; }
uint8_t  plat_reset_was_watchdog(void) { return (s_reset_flags & RCC_RSR_IWDG1RSTF) ? 1U : 0U; }
const char *plat_mcu_name(void)       { return "STM32H743"; }

void plat_sys_reset(void)
{
    NVIC_SystemReset();
}

void plat_wdt_start(void)
{
    s_iwdg.Instance       = BOARD_IWDG_INSTANCE;
    s_iwdg.Init.Prescaler = BOARD_IWDG_PRESCALER;
    s_iwdg.Init.Reload    = BOARD_IWDG_RELOAD;
    s_iwdg.Init.Window    = IWDG_WINDOW_DISABLE;   /* Window=0 would reset on every refresh */
    HAL_IWDG_Init(&s_iwdg);
}

void plat_wdt_feed(void)
{
    if (s_iwdg.Instance != NULL) HAL_IWDG_Refresh(&s_iwdg);
}
