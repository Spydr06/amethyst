#include "sys/timekeeper.h"
#include <drivers/char/ps2.h>

#include <time.h>
#include <x86_64/dev/io.h>
#include <kernelio.h>

static inline bool ps2_outbuffer_empty(void) {
    return !(inb(PS2_PORT_STATUS) & 1);
}

static inline bool ps2_inbuffer_full(void) {
    return !!(inb(PS2_PORT_STATUS) & 2);
}

static inline void ps2_write_command(enum ps2_port_command cmd) {
    while(ps2_inbuffer_full());
    outb(PS2_PORT_COMMAND, cmd);
}

static inline void ps2_write_data(uint8_t data) {
    while(ps2_inbuffer_full());
    outb(PS2_PORT_DATA, data);
}

static inline void ps2_flush(void) {
    while(!ps2_outbuffer_empty())
        inb(PS2_PORT_DATA);
}

static inline uint8_t ps2_read_data(void) {
    while(ps2_outbuffer_empty());
    return inb(PS2_PORT_DATA);
}

static inline void ps2_device_command(uint8_t port, enum ps2_device_command command) {
    if(port == 2)
        ps2_write_command(PS2_CMD_SECOND_PORT_SELECT);
    ps2_write_data(command);
}

static uint8_t ps2_read_data_timeout(int ms, bool* timeout) {
    struct timespec initial = timekeeper_time_from_boot();

    while(ps2_outbuffer_empty()) {
        struct timespec current = timekeeper_time_from_boot();
        if(timespec_diff_ms(initial, current) >= ms) {
            *timeout = true;
            return 0;
        }
    }

    *timeout = false;
    return ps2_read_data();
}

bool ps2_device_write_ok(uint8_t port, uint8_t command) {
    ps2_device_command(port, command);

    for(int trials = 0; ;) {
        bool timeout;
        uint8_t data = ps2_read_data_timeout(100, &timeout);
        if(timeout)
            return false;

        if(data == PS2_RESEND) {
            if(trials++ > 5)
                return false;

            ps2_device_command(port, command);
        }

        if(data == PS2_ACK)
            break;
    }

    return true;
}

void ps2_disable_scanning(uint8_t port) {
    ps2_device_command(port, PS2_DEVICE_CMD_DISABLE_SCANNING);

    for(int trials = 0; ;) {
        bool timeout;
        uint8_t data = ps2_read_data_timeout(100, &timeout);
        if(timeout)
            break;

        if(data == PS2_RESEND) {
            if(trials++ > 5) {
                klog(WARN, "PS/2 port [%d]: too many resends", port);
                return;
            }

            ps2_device_command(port, PS2_DEVICE_CMD_DISABLE_SCANNING);
        }
    }

    ps2_flush();
}

bool ps2_enable_scanning(uint8_t port) {
    ps2_device_command(port, PS2_DEVICE_CMD_ENABLE_SCANNING);

    for(int trials = 0; ;) {
        bool timeout;
        uint8_t data = ps2_read_data_timeout(100, &timeout);
        if(timeout) {
            klog(WARN, "PS/2 port [%d]: timeout", port);
            return false;
        }

        if(data == PS2_ACK)
            break;

        if(data == PS2_RESEND) {
            if(trials++ > 5) {
                klog(WARN, "PS/2 port [%d]: too many resends", port);
                return false;
            }

            ps2_device_command(port, PS2_DEVICE_CMD_ENABLE_SCANNING);
        }
    }

    ps2_flush();
    return true;
}

bool ps2_device_reset_selftest(uint8_t port) {
    ps2_disable_scanning(port);

    ps2_device_command(port, PS2_DEVICE_CMD_RESET_SELFTEST);

    for(int trials = 0; ;) {
        bool timeout;
        uint8_t data = ps2_read_data_timeout(500, &timeout);

        if(timeout) {
            klog(WARN, "PS/2 port [%d]: timed out waiting for ACK", port);
            return false;
        }

        if(data == PS2_RESEND) {
            if(trials++ > 5) {
                klog(WARN, "PS/2 port [%d]: too many resends", port);
                return false;
            }
            
            ps2_device_command(port, PS2_DEVICE_CMD_RESET_SELFTEST);
        }

        if(data == PS2_ACK)
            break;
    }

    for(;;) {
        bool timeout;
        uint8_t data = ps2_read_data_timeout(2000, &timeout);

        if(timeout) {
            klog(WARN, "PS/2 port [%d]: timed out waiting for result", port);
            return false;
        }

        if(data == PS2_DEVICE_SELFTEST_OK)
            break;

        if(data == PS2_DEVICE_SELFTEST_FAIL) {
            klog(ERROR, "PS/2 port [%d]: self test failed", port);
            return false;
        }
    }

    // clear buffer
    for(;;) {
        bool timeout;
        ps2_read_data_timeout(100, &timeout);
        if(timeout)
            break;
    }

    ps2_flush();
    return true;
}

bool ps2_identify(uint8_t port, uint8_t identity[2]) {
    ps2_device_command(port, PS2_DEVICE_CMD_IDENTIFY);

    bool timeout;

    for(int trials = 0; ;) {
        uint8_t data = ps2_read_data_timeout(100, &timeout);
        if(timeout)
            return false;

        if(data == PS2_RESEND) {
            if(trials++ > 5)
                return false;

            ps2_device_command(port, PS2_DEVICE_CMD_IDENTIFY);
        }

        if(data == PS2_ACK)
            break;
    }

    identity[0] = ps2_read_data_timeout(100, &timeout);
    if(timeout)
        return false;

    identity[1] = ps2_read_data_timeout(100, &timeout);
    return !timeout;
}

void ps2_init(void) {
    ps2_write_command(PS2_CMD_DISABLEP1);
    ps2_write_command(PS2_CMD_DISABLEP2);

    ps2_flush();

    ps2_write_command(PS2_CMD_READCFG);
    enum ps2_config cfg = ps2_read_data();

    bool dual_port = cfg & PS2_CFG_CLOCKP2;
    cfg &= ~(PS2_CFG_IRQP1 | PS2_CFG_IRQP2 | PS2_CFG_TRANSLATION);

    ps2_write_command(PS2_CMD_WRITECFG);
    ps2_write_data(cfg);

    ps2_write_command(PS2_CMD_SELFTEST);
    uint8_t result = ps2_read_data();

    if(result != PS2_SELFTEST_OK) {
        klog(ERROR, "controller self test failed (expected %x, got %x)", PS2_SELFTEST_OK, result);
        return;
    }

    ps2_write_command(PS2_CMD_WRITECFG);
    ps2_write_data(cfg);

    if(dual_port) {
        ps2_write_command(PS2_CMD_ENABLEP2);
        ps2_write_command(PS2_CMD_READCFG);
        cfg = ps2_read_data();

        if(cfg & PS2_CFG_CLOCKP2)
            dual_port = false;
    }

    int working_flag = 0;

    ps2_write_command(PS2_CMD_SELFTESTP1);
    if((result = ps2_read_data()))
        klog(ERROR, "PS/2 port [1] self test failed (expected 0, got %x)", result);
    else
        working_flag |= PS2_KEYBOARD_PORT;
    
    if(dual_port) {
        ps2_write_command(PS2_CMD_SELFTESTP2);
        if((result = ps2_read_data()))
            klog(ERROR, "PS/2 port [2] self test failed (expected 0, got %x)", result);
        else
            working_flag |= PS2_MOUSE_PORT;
    }

    if(!working_flag) {
        klog(ERROR, "no working PS/2 ports");
        return;
    }

    int connected_flag = 0;

    if(working_flag & PS2_KEYBOARD_PORT) {
        if(ps2_device_reset_selftest(PS2_KEYBOARD_PORT))
            connected_flag |= PS2_KEYBOARD_PORT;
        else
            klog(ERROR, "PS/2 port [1] self test failed");
    }

    if(working_flag & PS2_MOUSE_PORT) {
        if(ps2_device_reset_selftest(PS2_MOUSE_PORT))
            connected_flag |= PS2_MOUSE_PORT;
        else
            klog(ERROR, "PS/2 port [2] self test failed");
    }

    int ok_flag = 0;

    klog(INFO, "\033[95mPS/2:\033[0m controller with %d ports", dual_port + 1);

    if(connected_flag & PS2_KEYBOARD_PORT) {
        ps2_write_command(PS2_CMD_ENABLEP1);
        uint8_t identity[2];
        if(ps2_identify(PS2_KEYBOARD_PORT, identity)) {
            ps2_keyboard_init();
            ps2_write_command(PS2_CMD_DISABLEP1);
            ok_flag |= PS2_KEYBOARD_PORT;
        }
        else
            klog(WARN, "failed to identify PS/2 port [1]");
    }

    if(connected_flag & PS2_MOUSE_PORT) {
        ps2_write_command(PS2_CMD_ENABLEP2);
        uint8_t identity[2];
        if(ps2_identify(1, identity)) {
            ps2_mouse_init();
            ps2_write_command(PS2_CMD_DISABLEP2);
            ok_flag |= PS2_MOUSE_PORT;
        }
        else
            klog(WARN, "failed to identify PS/2 port [2]");
    }

    if(ok_flag & PS2_KEYBOARD_PORT) {
        ps2_write_command(PS2_CMD_ENABLEP1);
        if(!ps2_enable_scanning(PS2_KEYBOARD_PORT))
            klog(WARN, "failed to enable scanning for PS/2 port [1]");
    }

    if(ok_flag & PS2_MOUSE_PORT) {
        ps2_write_command(PS2_CMD_ENABLEP2);
        if(!ps2_enable_scanning(PS2_MOUSE_PORT))
            klog(WARN, "falied to enable scanning for PS/2 port [2]");
    }

    ps2_write_command(PS2_CMD_READCFG);
    cfg = ps2_read_data();

    cfg |= (dual_port ? PS2_CFG_IRQP1 | PS2_CFG_IRQP2 : PS2_CFG_IRQP1) | PS2_CFG_TRANSLATION;

    ps2_write_command(PS2_CMD_WRITECFG);
    ps2_write_data(cfg);
}
