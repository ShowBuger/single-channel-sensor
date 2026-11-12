/* STM32F1 port: system init, clock tree, reset cause, watchdog. */
#include "plat.h"
#include "board_pins.h"

static uint32_t           s_reset_flags;
static IWDG_HandleTypeDef s_iwdg;

static void clock_config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* HSE 8 MHz crystal -> PLL x9 = 72 MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.LSIState       = RCC_LSI_ON;                  /* IWDG */
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        /* No crystal: HSI/2 = 4 MHz x16 = 64 MHz */
        osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
        osc.HSEState            = RCC_HSE_OFF;
        osc.HSIState            = RCC_HSI_ON;
        osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
        osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
        osc.PLL.PLLMUL          = RCC_PLL_MUL16;
        HAL_RCC_OscConfig(&osc);
    }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;     /* APB1 max 36 MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
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
const char *plat_mcu_name(void)           { return "STM32F103"; }
void        plat_sys_reset(void)          { NVIC_SystemReset(); }

void plat_wdt_start(void)
{
    s_iwdg.Instance       = IWDG;
    s_iwdg.Init.Prescaler = BOARD_IWDG_PRESCALER;
    s_iwdg.Init.Reload    = BOARD_IWDG_RELOAD;          /* F1 IWDG has no window */
    HAL_IWDG_Init(&s_iwdg);
}

void plat_wdt_feed(void)
{
    if (s_iwdg.Instance != NULL) HAL_IWDG_Refresh(&s_iwdg);
}
