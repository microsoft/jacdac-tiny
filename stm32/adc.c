#include "jdstm.h"

static uint16_t adc_convert(void) {
    if ((LL_ADC_IsEnabled(ADC1) == 1) && (LL_ADC_IsDisableOngoing(ADC1) == 0) &&
        (LL_ADC_REG_IsConversionOngoing(ADC1) == 0))
        LL_ADC_REG_StartConversion(ADC1);
    else
        jd_panic();

    while (LL_ADC_IsActiveFlag_EOC(ADC1) == 0)
        ;
    LL_ADC_ClearFlag_EOC(ADC1);

    return LL_ADC_REG_ReadConversionData12(ADC1);
}

static void set_channel(uint32_t chan) {
#ifdef STM32G0
    LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, chan);
    LL_ADC_SetChannelSamplingTime(ADC1, chan, LL_ADC_SAMPLINGTIME_COMMON_1);

    LL_ADC_EnableInternalRegulator(ADC1);
    target_wait_us(LL_ADC_DELAY_INTERNAL_REGUL_STAB_US);
#else
    LL_ADC_REG_SetSequencerChannels(ADC1, chan);
#endif

    LL_ADC_StartCalibration(ADC1);

    while (LL_ADC_IsCalibrationOnGoing(ADC1) != 0)
        ;
    target_wait_us(5); // ADC_DELAY_CALIB_ENABLE_CPU_CYCLES

    // LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_AUTOPOWEROFF);

    LL_ADC_Enable(ADC1);

    while (LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0)
        ;
}

// alternative would be build an RNG, eg http://robseward.com/misc/RNG2/

// initializes RNG from TEMP sensor
void adc_init_random(void) {
    __HAL_RCC_ADC_CLK_ENABLE();

    LL_ADC_SetCommonPathInternalCh(
        __LL_ADC_COMMON_INSTANCE(ADC1),
        (LL_ADC_PATH_INTERNAL_VREFINT | LL_ADC_PATH_INTERNAL_TEMPSENSOR));
    target_wait_us(LL_ADC_DELAY_TEMPSENSOR_STAB_US);

    ADC1->CFGR1 = LL_ADC_REG_OVR_DATA_OVERWRITTEN;

#ifdef STM32G0
    LL_ADC_REG_SetSequencerLength(ADC1, LL_ADC_REG_SEQ_SCAN_DISABLE);
    target_wait_us(5);

    LL_ADC_REG_SetSequencerConfigurable(ADC1, LL_ADC_REG_SEQ_CONFIGURABLE);
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
                                         LL_ADC_SAMPLINGTIME_1CYCLE_5);
#else
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_1CYCLE_5);
#endif

    ADC1->CFGR2 = LL_ADC_CLOCK_SYNC_PCLK_DIV2;

    set_channel(LL_ADC_CHANNEL_TEMPSENSOR);

    uint32_t h = 0x811c9dc5;
    for (int i = 0; i < 1000; ++i) {
        int v = adc_convert();
        h = (h * 0x1000193) ^ (v & 0xff);
    }
    jd_seed_random(h);

    // ADC enabled is like 200uA in STOP
    LL_ADC_Disable(ADC1);
    while (LL_ADC_IsDisableOngoing(ADC1))
        ;

    // setup for future pin conversions
#ifdef STM32G0
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_COMMON_1,
                                         LL_ADC_SAMPLINGTIME_7CYCLES_5);
#else
    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_7CYCLES_5);
#endif


    // this brings STOP draw to 4.4uA from 46uA
    LL_ADC_REG_SetSequencerChannels(ADC1, 0);
    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_NONE);
    target_wait_us(LL_ADC_DELAY_TEMPSENSOR_STAB_US);

    __HAL_RCC_ADC_CLK_DISABLE();
}

static const uint32_t channels_PA[] = {
    LL_ADC_CHANNEL_0, LL_ADC_CHANNEL_1, LL_ADC_CHANNEL_2, LL_ADC_CHANNEL_3,
    LL_ADC_CHANNEL_4, LL_ADC_CHANNEL_5, LL_ADC_CHANNEL_6, LL_ADC_CHANNEL_7,
};

static const uint32_t channels_PB[] = {
    LL_ADC_CHANNEL_8,
    LL_ADC_CHANNEL_9,
};

uint16_t adc_read_pin(uint8_t pin) {
    uint32_t chan;

    if (pin >> 4 == 0) {
        if ((pin & 0xf) >= sizeof(channels_PA) / sizeof(channels_PA[0]))
            jd_panic();
        chan = channels_PA[pin & 0xf];
    } else if (pin >> 4 == 1) {
        if ((pin & 0xf) >= sizeof(channels_PB) / sizeof(channels_PB[0]))
            jd_panic();
        chan = channels_PB[pin & 0xf];
    } else {
        chan = 0;
        jd_panic();
    }

    pin_setup_analog_input(pin);

    __HAL_RCC_ADC_CLK_ENABLE();

    set_channel(chan);
    uint16_t r = adc_convert();

    LL_ADC_Disable(ADC1);
    while (LL_ADC_IsDisableOngoing(ADC1))
        ;
    __HAL_RCC_ADC_CLK_DISABLE();

    return r;
}
