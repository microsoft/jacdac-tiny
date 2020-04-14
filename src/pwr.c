#include "jdsimple.h"

static uint8_t pll_cnt, tim_cnt;

bool pwr_in_pll() {
    return pll_cnt > 0;
}

void pwr_enter_pll() {
    if (target_in_irq() || pll_cnt == 0xff)
        jd_panic(); // should not be called in ISR handler
    if (pll_cnt == 0)
        clk_set_pll(1);
    pll_cnt++;
}

void pwr_leave_pll() {
    if (!pll_cnt)
        jd_panic();
    pll_cnt--;
    if (pll_cnt == 0)
        clk_set_pll(0);
}

void pwr_enter_tim() {
    if (target_in_irq() || tim_cnt == 0xff)
        jd_panic(); // should not be called in ISR handler
    tim_cnt++;
}

void pwr_leave_tim() {
    if (!tim_cnt)
        jd_panic();
    tim_cnt--;
}

void pwr_sleep() {
#ifdef BL
    rtc_sleep(true);
#else
    rtc_sleep(pll_cnt || tim_cnt || jd_is_busy());
#endif
}
