#include <drivers/char/mouse.h>

#include <kernelio.h>

void mouse_init(struct mouse* mouse) {
    (void) mouse;
}

void mouse_event(struct mouse* mouse, struct mouse_event event) {
    (void) mouse;
    (void) event;
    here();
}
