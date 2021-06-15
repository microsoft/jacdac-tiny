#include "jdstm.h"

#ifdef PIN_SDA

#ifndef I2C_IDX
#define I2C_IDX 1
#endif

#if I2C_IDX == 1
#define I2Cx I2C1
#define I2C_CLK LL_APB1_GRP1_PERIPH_I2C1
#define I2C_CLK_SRC LL_RCC_I2C1_CLKSOURCE_HSI
#elif I2C_IDX == 2
#define I2Cx I2C2
#define I2C_CLK LL_APB1_GRP1_PERIPH_I2C2
// only G0Bx and G0Cx support this - we don't really want PCLK source, so in reality only I2C1 is supported
#define I2C_CLK_SRC LL_RCC_I2C2_CLKSOURCE_HSI 
#endif

#ifndef I2C_FAST_MODE
#define I2C_FAST_MODE 1
#endif

#ifdef STM32F0
#if 1
// 22.4.10 I2C_TIMINGR register configuration examples
#if I2C_FAST_MODE
#define I2C_TIMING 0x00310309
#else
#define I2C_TIMING 0x00420F13
#endif
#else
// rise 100ns, fall 10ns
#if I2C_FAST_MODE
#define I2C_TIMING 0x0010020A
#else
#define I2C_TIMING 0x00201D2B
#endif
#endif
#endif

#ifdef STM32G0
// 25.4.11 I2C_TIMINGR register configuration examples
#if I2C_FAST_MODE
#define I2C_TIMING 0x10320309
#else
#define I2C_TIMING 0x30420F13
#endif
#endif

static void setup_pin(uint8_t pin) {
    pin_setup_output_af(pin, I2C_AF);
    LL_GPIO_SetPinOutputType(PIN_PORT(pin), PIN_MASK(pin), LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinPull(PIN_PORT(pin), PIN_MASK(pin), LL_GPIO_PULL_UP);
}

void i2c_init(void) {
    i2c_reset();
    setup_pin(PIN_SDA);
    setup_pin(PIN_SCL);
    LL_RCC_SetI2CClockSource(I2C_CLK_SRC);
    LL_APB1_GRP1_EnableClock(I2C_CLK);
    LL_I2C_Disable(I2Cx);
    LL_I2C_SetTiming(I2Cx, I2C_TIMING);
    LL_I2C_Enable(I2Cx);
}

#ifdef STM32F0
#define CYCLES_PER_MS (77 * cpu_mhz)
#elif defined(STM32G0)
#define CYCLES_PER_MS (100 * cpu_mhz) // TODO measure this!
#else
#error "measure CYCLES_PER_MS"
#endif

static int wait_for_ack(void) {
    unsigned k = 10 * CYCLES_PER_MS;
    while (k--) {
        if (LL_I2C_IsActiveFlag_TXIS(I2Cx))
            return 0;
    }
    return -1;
}

int i2c_setup_write(uint8_t addr, unsigned len, bool repeated) {
    addr <<= 1;
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, len,
                          repeated ? LL_I2C_MODE_SOFTEND : LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    if (wait_for_ack()) {
        DMESG("nack %x", addr);
        return -10;
    }

    return 0;
}

int i2c_write(uint8_t c) {
    LL_I2C_TransmitData8(I2Cx, c);

    unsigned k = 3 * CYCLES_PER_MS;
    while (k--) {
        if (LL_I2C_IsActiveFlag_TXE(I2Cx)) {
            if (LL_I2C_IsActiveFlag_NACK(I2Cx)) {
                DMESG("nack wr:%x", c);
                return -12;
            } else {
                return 0;
            }
        }
    }

    DMESG("t/o wr:%x", c);
    return -13;
}

int i2c_finish_write(bool repeated) {
    if (repeated) {
        while (!LL_I2C_IsActiveFlag_TC(I2Cx))
            ;
    } else {
        while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
            ;
        LL_I2C_ClearFlag_STOP(I2Cx);
    }
    return 0;
}

#define MAXREP 10000

int i2c_read_ex(uint8_t addr, void *dst, unsigned len) {
    addr <<= 1;
    bool fault = false;
i2c_read_reset:
    LL_I2C_HandleTransfer(I2Cx, addr, LL_I2C_ADDRSLAVE_7BIT, len, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_RESTART_7BIT_READ);
    uint8_t *p = dst;
    uint8_t *end = p + len;
    while (p < end) {
        unsigned i;
        for (i = 0; i < MAXREP; ++i)
            if (LL_I2C_IsActiveFlag_RXNE(I2Cx))
                break;

        if (i >= MAXREP) {
            DMESG("T/o rd");
            if (fault)
                hw_panic();
            i2c_reset();
            i2c_init();
            fault = true;
            goto i2c_read_reset;
        }

        *p++ = LL_I2C_ReceiveData8(I2Cx);
    }

    while (!LL_I2C_IsActiveFlag_STOP(I2Cx))
        ;
    LL_I2C_ClearFlag_STOP(I2Cx);

    return 0;
}

//
// platform-independent code starts
//

#define CHECK_RET(call)                                                                            \
    do {                                                                                           \
        int _r = call;                                                                             \
        if (_r < 0)                                                                                \
            return _r;                                                                             \
    } while (0)

int i2c_write_ex(uint8_t addr, const void *src, unsigned len, bool repeated) {
    CHECK_RET(i2c_setup_write(addr, len, repeated));
    const uint8_t *p = src;
    while (len--)
        CHECK_RET(i2c_write(*p++));
    CHECK_RET(i2c_finish_write(repeated));
    return 0;
}

// 8-bit reg addr
int i2c_write_reg_buf(uint8_t addr, uint8_t reg, const void *src, unsigned len) {
    CHECK_RET(i2c_setup_write(addr, len + 1, false));
    CHECK_RET(i2c_write(reg));
    const uint8_t *p = src;
    while (len--)
        CHECK_RET(i2c_write(*p++));
    CHECK_RET(i2c_finish_write(false));
    return 0;
}

int i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    return i2c_write_reg_buf(addr, reg, &val, 1);
}

int i2c_read_reg_buf(uint8_t addr, uint8_t reg, void *dst, unsigned len) {
    CHECK_RET(i2c_write_ex(addr, &reg, 1, true) < 0);
    return i2c_read_ex(addr, dst, len);
}

int i2c_read_reg(uint8_t addr, uint8_t reg) {
    uint8_t r = 0;
    CHECK_RET(i2c_read_reg_buf(addr, reg, &r, 1));
    return r;
}

// 16-bit reg addr
int i2c_write_reg16_buf(uint8_t addr, uint16_t reg, const void *src, unsigned len) {
    CHECK_RET(i2c_setup_write(addr, len + 2, false));
    CHECK_RET(i2c_write(reg >> 8));
    CHECK_RET(i2c_write(reg & 0xff));
    const uint8_t *p = src;
    while (len--)
        CHECK_RET(i2c_write(*p++));
    CHECK_RET(i2c_finish_write(false));
    return 0;
}

int i2c_write_reg16(uint8_t addr, uint16_t reg, uint8_t val) {
    return i2c_write_reg16_buf(addr, reg, &val, 1);
}

int i2c_read_reg16_buf(uint8_t addr, uint16_t reg, void *dst, unsigned len) {
    uint8_t a[] = {reg >> 8, reg & 0xff};
    CHECK_RET(i2c_write_ex(addr, a, 2, true) < 0);
    return i2c_read_ex(addr, dst, len);
}

int i2c_read_reg16(uint8_t addr, uint16_t reg) {
    uint8_t r = 0;
    CHECK_RET(i2c_read_reg16_buf(addr, reg, &r, 1));
    return r;
}

void i2c_reset(void) {
    pin_setup_output(PIN_SDA);
    pin_setup_output(PIN_SCL);

    pin_set(PIN_SDA, 0);
    pin_set(PIN_SCL, 0);

    target_wait_us(10);

    pin_set(PIN_SDA, 1);
    pin_set(PIN_SCL, 1);

    for (int i = 0; i < 8; i++) {
        pin_set(PIN_SCL, 1);
        target_wait_us(10);
        pin_set(PIN_SCL, 0);
        target_wait_us(10);
    }
}

#endif