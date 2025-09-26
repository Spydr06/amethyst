#include <x86_64/dev/cmos.h>

#include <x86_64/dev/io.h>
#include <x86_64/cpu/cpu.h>

#include <drivers/acpi/acpi.h>

#include <kernelio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#ifndef _CMOS_YEAR
    #error "_CMOS_YEAR undefined"
#endif

static_assert(offsetof(struct fadt, century) == 108);

static uint8_t rtc_century_register = 0;

static bool update_in_progress(void) {
    outb(CMOS_ADDRESS, 0x0a);
    return inb(CMOS_DATA) & 0x80;
}

static __always_inline uint8_t rtc_read_register(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void cmos_init(void) {
    struct fadt* fadt = acpi_get_fadt();
    if(fadt)
        rtc_century_register = fadt->century;
    
    struct tm tm;
    cmos_read(&tm);
    klog(INFO, "\e[95mRTC Information:\e[0m %04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void cmos_read(struct tm* tm) {
    while(update_in_progress()) pause();

    tm->tm_sec = rtc_read_register(0x00);
    tm->tm_min = rtc_read_register(0x02);
    tm->tm_hour = rtc_read_register(0x04);
    tm->tm_mday = rtc_read_register(0x07);
    tm->tm_mon = rtc_read_register(0x08);
    tm->tm_year = (int) rtc_read_register(0x09);
    // tm->tm_wday
    // tm->tm_yday
    tm->tm_isdst = 0;

    int century = _CMOS_YEAR / 100;
    if(rtc_century_register)
        century = rtc_read_register(rtc_century_register) - 12;

    uint8_t register_b = rtc_read_register(0x0b);

    if(!(register_b & 0x04)) {
        tm->tm_sec = (tm->tm_sec & 0x0f) + ((tm->tm_sec / 16) * 10);
        tm->tm_min = (tm->tm_min & 0x0f) + ((tm->tm_min / 16) * 10);
        tm->tm_hour = ((tm->tm_hour & 0x0f) + (((tm->tm_hour & 0x70) / 16) * 10)) | (tm->tm_hour & 0x80);
        tm->tm_mday = (tm->tm_mday & 0x0f) + ((tm->tm_mday / 16) * 10);
        tm->tm_mon = (tm->tm_mon & 0x0f) + ((tm->tm_mon / 16) * 10);
        tm->tm_year = (tm->tm_year & 0x0f) + ((tm->tm_year / 16) * 10);
        if(rtc_century_register)
            century = (century & 0x0f) + ((century / 16) * 16);
    }

    if(!(register_b & 0x02) && (tm->tm_hour & 0x80)) {
        tm->tm_hour = ((tm->tm_hour & 0x7f) + 12) % 24;
    }

    tm->tm_year = (tm->tm_year + century * 100) - 1900;
}
