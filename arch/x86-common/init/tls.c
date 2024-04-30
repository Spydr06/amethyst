#include <x86-common/init/tls.h>
#include <cdefs.h>
#include <stddef.h>

#ifdef __x86_64__
static_assert(offsetof(struct tls_table, guard) == 0x28, "TLS is at 0x28 on x86_64");
#else
static_assert(offsetof(struct tls_table, guard) == 0x14, "TLS is at 0x18 on i386");
#endif

struct tls_table tls __section(".data");
