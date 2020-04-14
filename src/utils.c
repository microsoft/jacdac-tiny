#include "jdsimple.h"

/**
 * Performs an in buffer reverse of a given char array.
 *
 * @param s the string to reverse.
 *
 * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER.
 */
int string_reverse(char *s) {
    // sanity check...
    if (s == NULL)
        return -1;

    char *j;
    int c;

    j = s + strlen(s) - 1;

    while (s < j) {
        c = *s;
        *s++ = *j;
        *j-- = c;
    }

    return 0;
}

/**
 * Converts a given integer into a string representation.
 *
 * @param n The number to convert.
 *
 * @param s A pointer to the buffer where the resulting string will be stored.
 *
 * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER.
 */
int itoa(int n, char *s) {
    int i = 0;
    int positive = (n >= 0);

    if (s == NULL)
        return -1;

    // Record the sign of the number,
    // Ensure our working value is positive.
    unsigned k = positive ? n : -n;

    // Calculate each character, starting with the LSB.
    do {
        s[i++] = (k % 10) + '0';
    } while ((k /= 10) > 0);

    // Add a negative sign as needed
    if (!positive)
        s[i++] = '-';

    // Terminate the string.
    s[i] = '\0';

    // Flip the order.
    string_reverse(s);

    return 0;
}

#ifndef BL
// faster versions of memcpy() and memset()
void *memcpy(void *dst, const void *src, size_t sz) {
    if (sz >= 4 && !((uintptr_t)dst & 3) && !((uintptr_t)src & 3)) {
        size_t cnt = sz >> 2;
        uint32_t *d = (uint32_t *)dst;
        const uint32_t *s = (const uint32_t *)src;
        while (cnt--) {
            *d++ = *s++;
        }
        sz &= 3;
        dst = d;
        src = s;
    }

    uint8_t *dd = (uint8_t *)dst;
    uint8_t *ss = (uint8_t *)src;

    while (sz--) {
        *dd++ = *ss++;
    }

    return dst;
}

void *memset(void *dst, int v, size_t sz) {
    if (sz >= 4 && !((uintptr_t)dst & 3)) {
        size_t cnt = sz >> 2;
        uint32_t vv = 0x01010101 * v;
        uint32_t *d = (uint32_t *)dst;
        while (cnt--) {
            *d++ = vv;
        }
        sz &= 3;
        dst = d;
    }

    uint8_t *dd = (uint8_t *)dst;

    while (sz--) {
        *dd++ = v;
    }

    return dst;
}
#endif

uint32_t random_int(int max) {
    if (max == 0)
        return 0;
    uint32_t mask = 0x1;
    while (mask < max)
        mask = (mask << 1) | 1;
    for (;;) {
        uint32_t v = jd_random() & mask;
        if (v <= max)
            return v;
    }
}

void dump_pkt(jd_packet_t *pkt, const char *msg) {
    DMESG("pkt[%s]; s#=%d sz=%d %x", msg, pkt->service_number, pkt->service_size,
          pkt->service_command);
}
