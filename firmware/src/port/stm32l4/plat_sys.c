/* STM32L4 port: system init, clock tree, reset cause, watchdog. */
#include "plat.h"
#include "board_pins.h"

static uint32_t           s_reset_flags;
static IWDG_HandleTypeDef s_iwdg;

static void clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) { while (1) {} }

    /* HSI16 -> PLL: 16 MHz / M1 * N10 / R2 = 80 MHz */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.LSIState            = RCC_LSI_ON;              /* IWDG */
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 1;
    osc.PLL.PLLN            = 10;                      /* 160 MHz VCO */
    osc.PLL.PLLP            = RCC_PLLP_DIV7;
    osc.PLL.PLLQ            = RCC_PLLQ_DIV2;
    osc.PLL.PLLR            = RCC_PLLR_DIV2;           /* 80 MHz SYSCLK */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { while (1) {} }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* 80 MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;     /* 80 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;     /* 80 MHz */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) { while (1) {} }
}

void plat_sys_init(void)
{
    s_reset_flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    HAL_Init();
    clock_config();
}

uint32_t    plat_reset_flags(void)        { return s_reset_flags; }
uint8_t     plat_reset_was_watchdog(void) { return (s_reset_flags & RCC_CSR_IWDGRSTF) ? 1U : 0U; }
const char *plat_mcu_name(void)           { return "STM32L432"; }
void        plat_sys_reset(void)          { NVIC_SystemReset(); }

void plat_wdt_start(void)
{
    s_iwdg.Instance       = IWDG;
    s_iwdg.Init.Prescaler = BOARD_IWDG_PRESCALER;
    s_iwdg.Init.Reload    = BOARD_IWDG_RELOAD;
    s_iwdg.Init.Window    = IWDG_WINDOW_DISABLE;   /* Window=0 would reset on every refresh */
    HAL_IWDG_Init(&s_iwdg);
}

void plat_wdt_feed(void)
{
    if (s_iwdg.Instance != NULL) HAL_IWDG_Refresh(&s_iwdg);
}
