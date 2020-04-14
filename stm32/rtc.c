#include "jdstm.h"

#ifndef BL

#define US_TICK_SCALE 20
#define TICK_US_SCALE 10
#define US_TO_TICKS(us) (((us)*ticksPerUs) >> US_TICK_SCALE)
#define TICKS_TO_US(t) (((t)*usPerTick) >> TICK_US_SCALE)

static uint32_t ticksPerUs, usPerTick;
static uint16_t presc;
static uint16_t lastSetSleep;
static cb_t cb;

#define PIN_PWR_STATE -1
#define PIN_PWR_LOG -1

static void rtc_set(uint32_t delta_us, cb_t f) {
    if (delta_us > 10000)
        jd_panic();
    uint32_t delta_tick = US_TO_TICKS(delta_us);
    if (delta_tick < 20)
        delta_tick = 20;

    cb = f;

    LL_RTC_ALMA_Disable(RTC);
    while (!LL_RTC_IsActiveFlag_ALRAW(RTC))
        ;

    uint32_t v = LL_RTC_TIME_GetSubSecond(RTC);
    lastSetSleep = v;
    uint32_t nv = v + presc - delta_tick; // the timer counts back; also don't underflow
    if (nv >= presc)
        nv -= presc; // make sure it's in range

    LL_RTC_ALMA_SetSubSecond(RTC, nv);
    LL_RTC_ClearFlag_ALRA(RTC);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_17);
    NVIC_ClearPendingIRQ(RTC_IRQn);
    LL_RTC_ALMA_Enable(RTC);
    // pin_pulse(PIN_PWR_LOG, 1);
}

void rtc_cancel_cb() {
    cb = NULL;
}

void rtc_sync_time() {
    pin_set(PIN_PWR_STATE, 1);
    target_disable_irq();
    if ((lastSetSleep >> 15) == 0) {
        int d = lastSetSleep + presc - LL_RTC_TIME_GetSubSecond(RTC);
        if (d >= presc)
            d -= presc;
        tim_forward(TICKS_TO_US(d));
        lastSetSleep = 0xffff;
    }
    target_enable_irq();
}

void RTC_IRQHandler(void) {
    rtc_sync_time();

    if (LL_RTC_IsActiveFlag_ALRA(RTC) != 0) {
        LL_RTC_ClearFlag_ALRA(RTC);

        target_disable_irq();
        cb_t f = cb;
        cb = NULL;
        target_enable_irq();

        if (f)
            f();
    }

    // Clear the EXTI's Flag for RTC Alarm
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_17);
}

static void rtc_config(uint8_t p0, uint16_t p1) {
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_EnableBkUpAccess();

    LL_RCC_LSI_Enable();

    while (LL_RCC_LSI_IsReady() != 1)
        ;

    LL_RCC_ForceBackupDomainReset();
    LL_RCC_ReleaseBackupDomainReset();
    LL_RCC_SetRTCClockSource(LL_RCC_RTC_CLKSOURCE_LSI);

    LL_RCC_EnableRTC();

    LL_RTC_DisableWriteProtection(RTC);

    LL_RTC_EnableInitMode(RTC);
    while (LL_RTC_IsActiveFlag_INIT(RTC) != 1)
        ;

    LL_RTC_SetAsynchPrescaler(RTC, p0 - 1);
    LL_RTC_SetSynchPrescaler(RTC, p1 - 1);

    // set alarm
    LL_RTC_ALMA_SetMask(RTC, LL_RTC_ALMA_MASK_ALL); // ignore all

    LL_RTC_ALMA_SetSubSecond(RTC, 0);
    LL_RTC_ALMA_SetSubSecondMask(RTC, 15); // compare entire sub-second reg

    LL_RTC_ALMA_Enable(RTC);
    LL_RTC_ClearFlag_ALRA(RTC);
    LL_RTC_EnableIT_ALRA(RTC);

    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_17);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_17);

    NVIC_SetPriority(RTC_IRQn, 2); // match tim.c
    NVIC_EnableIRQ(RTC_IRQn);

    LL_RTC_DisableInitMode(RTC);

    LL_RTC_ClearFlag_RS(RTC);
    while (LL_RTC_IsActiveFlag_RS(RTC) != 1)
        ;

    // LL_RTC_EnableWriteProtection(RTC);
}

// with more than 4000 it overflows!
#define CALIB_CYCLES 1024

void rtc_init() {
    target_disable_irq();
    rtc_config(1, CALIB_CYCLES);
    uint64_t t0 = tim_get_micros();
    while (LL_RTC_IsActiveFlag_ALRA(RTC) == 0)
        ;
    uint32_t d = (tim_get_micros() - t0) + 20;
    target_enable_irq();

    ticksPerUs = (1 << US_TICK_SCALE) * CALIB_CYCLES / d;
    usPerTick = d * CALIB_CYCLES / (1 << TICK_US_SCALE);

    uint32_t ten_ms = US_TO_TICKS(10000);
    DMESG("rtc: 10ms = %d ticks", ten_ms);
    // we're expecting around 400, but there's large drift possible
    if (!(300 <= ten_ms && ten_ms <= 500))
        jd_panic();

    // use a little more than 10ms, so we don't have issues with wrap around
    presc = US_TO_TICKS(12000);
    rtc_config(1, presc);
}

void rtc_sleep(bool forceShallow) {
    if (forceShallow) {
        pin_pulse(PIN_PWR_LOG, 1);
        __WFI();
        return;
    }

    target_disable_irq();
    uint32_t usec;
    cb_t f = tim_steal_callback(&usec);
    if (!f) {
        target_enable_irq();
        pin_pulse(PIN_PWR_LOG, 2);
        __WFI();
        return;
    }

    rtc_set(usec, f);
    LL_PWR_SetPowerMode(LL_PWR_MODE_STOP_LPREGU);

// enable to test power in stand by; should be around 3.2uA
#if 0
    LL_RTC_ALMA_Disable(RTC);
    pin_pulse(PIN_P1, 5);
    LL_PWR_ClearFlag_SB();
    LL_PWR_ClearFlag_WU();
    LL_PWR_SetPowerMode(LL_PWR_MODE_STANDBY);
#endif

    LL_LPM_EnableDeepSleep();
    pin_set(PIN_PWR_STATE, 0);
    __WFI();
    target_enable_irq();
    rtc_sync_time();      // likely already happened in ISR, but doesn't hurt to check again
    LL_LPM_EnableSleep(); // i.e., no deep
}
#else
void rtc_sleep(bool forceShallow) {
    __WFI();
}
void rtc_sync_time() {}
void rtc_cancel_cb() {}
#endif