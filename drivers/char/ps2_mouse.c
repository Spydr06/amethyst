#include <drivers/char/ps2.h>
#include <drivers/char/mouse.h>

#include <x86_64/cpu/idt.h>
#include <drivers/acpi/apic.h>
#include <x86_64/dev/io.h>

#include <assert.h>
#include <kernelio.h>

static uint8_t data_offset;
static uint8_t data[4];

static bool has_wheel;
static bool has_5buttons;

static struct mouse mouse;

static inline bool received_all_data(void) {
    return data_offset == 3 + (uint8_t) has_wheel;
}

static void mouse_isr(struct cpu_context* __unused) {
    assert(data_offset < __len(data));
    data[data_offset++] = inb(PS2_PORT_DATA);

    if((data[0] & 8) == 0) {
        data_offset = 0;
        return;
    }

    if(!received_all_data())
        return;

    data_offset = 0;

    struct mouse_event event = {0};

    event.x = data[1] - (data[0] & 0x10 ? 0x100 : 0);
    event.y = data[2] - (data[2] & 0x20 ? 0x100 : 0);
    event.z = (data[3] & 0x7) * (data[3] & 0x8 ? -1 : 1);

    event.left = !!(data[0] & 1);
    event.right = !!(data[0] & 2);
    event.middle = !!(data[0] & 4);
    event.button4 = !!(data[3] & 0x10);
    event.button5 = !!(data[3] & 0x20);

    mouse_event(&mouse, event);
}

static void ps2_mouse_setrate(uint8_t rate) {
    if(!ps2_device_write_ok(PS2_MOUSE_PORT, PS2_DEVICE_CMD_MOUSE_SAMPLETRATE)
            || !ps2_device_write_ok(PS2_MOUSE_PORT, rate))
        klog(WARN, "failed to set rate %hhu at PS/2 port [%d]", rate, PS2_MOUSE_PORT);
}

static inline bool ps2_mouse_identify(uint8_t identity[2]) {
    if(!ps2_identify(1, identity)) {
        klog(WARN, "failed to identify mouse at PS/2 port [%d]", PS2_MOUSE_PORT);
        return true;
    }

    return false;
}

void ps2_mouse_init(void) {
    uint8_t identity[2];

    if(ps2_mouse_identify(identity))
        return;

    ps2_mouse_setrate(200);
    ps2_mouse_setrate(100);
    ps2_mouse_setrate(80);

    if(ps2_mouse_identify(identity))
        return;

    if(identity[0] == PS2_MOUSE_Z) {
        has_wheel = true;

        ps2_mouse_setrate(200);
        ps2_mouse_setrate(100);
        ps2_mouse_setrate(80);

        if(ps2_mouse_identify(identity))
            return;

        if(identity[0] == PS2_MOUSE_5B)
            has_5buttons = true;
    }

    ps2_mouse_setrate(60);

    struct isr* isr = interrupt_allocate(mouse_isr, apic_send_eoi, IPL_MOUSE);
    assert(isr);
    io_apic_register_interrupt(MOUSE_INTERRUPT, isr->id & 0xff, _cpu()->id, false);
    klog(INFO, "PS/2 mouse interrupt enabled with vector %lu", isr->id & 0xff);
    klog(INFO, "PS/2 mouse: wheel: %hhu, 5 buttons: %hhu", has_wheel, has_5buttons);

    mouse_init(&mouse);
}

