#include "blutil.h"

#ifdef PIN_BL_LED

#ifdef STM32F0
#if PIN_BL_LED == PB_1
#define TIMx TIM3
#define TIMx_CLK_ENABLE __HAL_RCC_TIM3_CLK_ENABLE
#define CHANNEL 4
#define AF LL_GPIO_AF_1
#endif
#elif defined(STM32G0)
#if PIN_BL_LED == PA_2
#define TIMx TIM2
#define TIMx_CLK_ENABLE __HAL_RCC_TIM2_CLK_ENABLE
#define CHANNEL 3
#define AF LL_GPIO_AF_3
#elif PIN_BL_LED == PA_6
#define TIMx TIM3
#define TIMx_CLK_ENABLE __HAL_RCC_TIM3_CLK_ENABLE
#define CHANNEL 1
#define AF LL_GPIO_AF_1
#elif PIN_BL_LED == PB_8
#define TIMx TIM16
#define TIMx_CLK_ENABLE __HAL_RCC_TIM16_CLK_ENABLE
#define CHANNEL 1
#define AF LL_GPIO_AF_2
#elif PIN_BL_LED == PB_1
#define TIMx TIM3
#define TIMx_CLK_ENABLE __HAL_RCC_TIM3_CLK_ENABLE
#define CHANNEL 4
#define AF LL_GPIO_AF_1
#endif
#else
#error "unsupported MCU"
#endif

#ifndef TIMx
#error "please add a mapping to this file for PIN_BL_LED"
#endif

void blled_init(uint32_t period) {
    TIMx_CLK_ENABLE();

    pin_setup_output_af(PIN_BL_LED, AF);

    // LL_TIM_SetPrescaler(TIMx, 2);
    LL_TIM_SetAutoReload(TIMx, period - 1);
    LL_TIM_GenerateEvent_UPDATE(TIMx);
    LL_TIM_EnableARRPreload(TIMx);

    if (TIMx == TIM1 || TIMx == TIM16)
        TIMx->BDTR |= TIM_BDTR_MOE;

#define MODE (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE)
#if CHANNEL == 1
    TIMx->CCMR1 = MODE;
#elif CHANNEL == 2
    TIMx->CCMR1 = MODE << 8;
#elif CHANNEL == 3
    TIMx->CCMR2 = MODE;
#elif CHANNEL == 4
    TIMx->CCMR2 = MODE << 8;
#else
#error "wrong channel"
#endif

    uint32_t chmask = 1 << (4 * (CHANNEL - 1));
    LL_TIM_CC_EnableChannel(TIMx, chmask);

    LL_TIM_EnableCounter(TIMx);
    LL_TIM_GenerateEvent_UPDATE(TIMx);
}

void blled_set_duty(uint32_t duty) {
    WRITE_REG(*(&TIMx->CCR1 + CHANNEL - 1), duty);
}

#endif