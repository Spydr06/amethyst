#include <assert.h>

void __stack_chk_fail(void) {
    assert(!"__stack_chk_fail(void) called");
}

