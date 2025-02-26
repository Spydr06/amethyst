#ifndef _AMETHYST_DRIVERS_CHAR_PS2_H
#define _AMETHYST_DRIVERS_CHAR_PS2_H

#include <stdint.h>

enum ps2_port : uint16_t {
    PS2_PORT_DATA = 0x60,
    PS2_PORT_COMMAND = 0x64,
    PS2_PORT_STATUS = 0x64
};

enum ps2_response : uint8_t {
    PS2_ECHO   = 0xee,
    PS2_ACK    = 0xfa,
    PS2_RESEND = 0xfe,
};

enum ps2_port_command : uint8_t {
    PS2_CMD_READCFG     = 0x20,
    PS2_CMD_WRITECFG    = 0x60,
    PS2_CMD_SELFTEST    = 0xaa,
    PS2_CMD_SELFTESTP1  = 0xab,
    PS2_CMD_DISABLEP1   = 0xad,
    PS2_CMD_ENABLEP1    = 0xae,
    PS2_CMD_DISABLEP2   = 0xa7,
    PS2_CMD_ENABLEP2    = 0xa8,
    PS2_CMD_SELFTESTP2  = 0xa9,
    PS2_CMD_SECOND_PORT_SELECT = 0xd4,
};

enum ps2_device_command : uint8_t {
    PS2_DEVICE_CMD_IDENTIFY         = 0xf2,
    PS2_DEVICE_CMD_ENABLE_SCANNING  = 0xf4,
    PS2_DEVICE_CMD_DISABLE_SCANNING = 0xf5,
    PS2_DEVICE_CMD_RESET_SELFTEST   = 0xff
};

static const uint8_t PS2_SELFTEST_OK = 0x55;

static const uint8_t PS2_DEVICE_SELFTEST_OK   = 0xaa;
static const uint8_t PS2_DEVICE_SELFTEST_FAIL = 0xfc;

enum ps2_config : uint8_t {
    PS2_CFG_IRQP1       = 0x01,
    PS2_CFG_IRQP2       = 0x02,
    PS2_CFG_CLOCKP2     = 0x20,
    PS2_CFG_TRANSLATION = 0x40
};

void ps2_init(void);
void ps2_keyboard_init(void);
void ps2_mouse_init(void);

#endif /* _AMETHYST_DRIVERS_CHAR_PS2_H */

