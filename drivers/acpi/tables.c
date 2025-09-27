#include <drivers/acpi/tables.h>
#include <drivers/acpi/hpet.h>

#include <sys/spinlock.h>

#include <kernelio.h>
#include <stddef.h>
#include <errno.h>

typedef int (*table_behavior_t)(const struct sdt_header*);

static int register_rsdt(const struct sdt_header* header);
static int register_xsdt(const struct sdt_header* header);
static int register_fadt(const struct sdt_header* header);

static spinlock_t tables_lock;
static size_t tables_count;
static struct sdt_header* tables[ACPI_MAX_TABLES];

static const struct {
    sdt_signature_t sig;
    table_behavior_t behavior;
} table_behaviors[] = {
    { .sig = "RSDT", .behavior = register_rsdt },
    { .sig = "XSDT", .behavior = register_xsdt },
    { .sig = "FACP", .behavior = register_fadt },
    { .sig = "HPET", .behavior = hpet_init },
    { "\0\0\0\0", nullptr }
};

_Static_assert(sizeof(sdt_signature_t) == sizeof(uint32_t));

bool acpi_validate_sdt(const struct sdt_header* header) {
    const uint8_t* ptr = (const uint8_t*) header;
    uint8_t sum = 0;
    for(unsigned i = 0; i < header->length; i++)
        sum += *ptr++;

    return sum == 0;
}

static table_behavior_t find_behavior(sdt_signature_t sig) {
    union {
        sdt_signature_t sig;
        uint32_t u;
    } *cur, *key = (void*) sig;

    for(size_t i = 0; table_behaviors[i].behavior; i++) {
        cur = (void*) table_behaviors[i].sig;    

        if(cur->u == key->u)
            return table_behaviors[i].behavior;
    }

    return nullptr;
}

int acpi_register_table(struct sdt_header* header) {
    if(!acpi_validate_sdt(header))
        return EINVAL;

    spinlock_acquire(&tables_lock);

    if(tables_count >= ACPI_MAX_TABLES) {
        spinlock_release(&tables_lock);
        return ENOMEM;
    }

    tables[tables_count++] = header;

    spinlock_release(&tables_lock);

    table_behavior_t onregister = find_behavior(header->sig);
    if(onregister)
        return onregister(header);

    return 0;
}

struct sdt_header* acpi_find_table(sdt_signature_t sig) {
    union {
        sdt_signature_t sig;
        uint32_t u;
    } *cur, *key = (void*) sig;

    spinlock_acquire(&tables_lock);

    struct sdt_header* found = nullptr;

    for(size_t i = 0; i < tables_count; i++) {
        struct sdt_header* header = tables[i];
        cur = (void*) header->sig;

        if(key->u == cur->u) {
            found = header;
            break;
        }
    }

    spinlock_release(&tables_lock);
    return found;
}

void acpi_print_tables(void) {
    spinlock_acquire(&tables_lock);

    for(size_t i = 0; i < tables_count; i++) {
        const struct sdt_header* header = tables[i];

        klog(DEBUG, " %2zu) %.4s : %.6s : %.8s", i, header->sig, header->oem_id, header->oem_table_id);
    }

    spinlock_release(&tables_lock);
}

static int register_rsdt(const struct sdt_header* header) {
    const struct rsdt* rsdt = (const struct rsdt*) header;

    size_t header_count = (rsdt->header.length - sizeof(struct sdt_header)) / sizeof(uint32_t);
    int err;    

    for(size_t i = 0; i < header_count; i++) {
        void* addr = MAKE_HHDM(rsdt->tables[i]);
            
        if((err = acpi_register_table(addr)))
            return err;
    }

    return 0;
}

static int register_xsdt(const struct sdt_header* header) {
    const struct xsdt* xsdt = (const struct xsdt*) header;

    size_t header_count = (xsdt->header.length - sizeof(struct sdt_header)) / sizeof(uint64_t);
    int err;

    for(size_t i = 0; i < header_count; i++) {
        void* addr = MAKE_HHDM(xsdt->tables[i]);

        if((err = acpi_register_table(addr)))
            return err;
    }

    return 0;
}

static int register_fadt(const struct sdt_header* header) {
    const struct fadt* fadt = (const struct fadt*) header;

    struct dsdt* dsdt = MAKE_HHDM(fadt->dsdt);
    if(dsdt != nullptr)
        return acpi_register_table(&dsdt->header);

    return 0;
}

