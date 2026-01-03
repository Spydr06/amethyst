#ifndef _AMETHYST_DRIVERS_PCI_H
#define _AMETHYST_DRIVERS_PCI_H

#include <kernelio.h>
#include "dynarray.h"

#define PCI_DATA_PORT    0x0cfc
#define PCI_COMMAND_PORT 0x0cf8

enum pci_class : uint8_t {
    PCI_CLASS_UNCLASSIFIED = 0x00,

    PCI_CLASS_MASS_STORAGE = 0x01,
    PCI_CLASS_NETWORK_CONTROLLER = 0x02,
    PCI_CLASS_DISPLAY_CONTROLLER = 0x03,
    PCI_CLASS_MULITMEDIA_CONTROLLER = 0x04,
    PCI_CLASS_MEMORY_CONTROLLER = 0x05,
    PCI_CLASS_BRIDGE = 0x06,
    PCI_CLASS_SIMPLE_COMMUNICATION_CONTROLLER = 0x07,
    PCI_CLASS_BASE_SYSTEM_PERIPHERAL = 0x08,
    PCI_CLASS_INPUT_DEVICE_CONTROLLER = 0x09,
    PCI_CLASS_PROCESSOR = 0x0b,
    PCI_CLASS_SERIAL_BUS_CONTROLLER = 0x0c,

    PCI_CLASS_UNASSIGNED = 0xff,
};

enum pci_subclass : uint8_t {
    // Mass storage controller (class 0x01)
    PCI_SUB_NONVOLATILE_MEMORY_CONTROLLER = 0x08,

    // Bridge (class 0x06)
    PCI_SUB_PCI_TO_PCI = 0x04
};

enum pci_header_type : uint8_t {
    PCI_HEADER_GENERAL = 0x00,
    PCI_HEADER_PCI_TO_PCI_BRIDGE = 0x01,
    PCI_HEADER_PCI_TO_CARDBUS_BRIDGE = 0x02
};

enum pci_capability_id : uint8_t {
    PCI_CAP_MSI = 0x05,
    PCI_CAP_MSI_X = 0x11,
};

#define PCI_EXTRA_HEADER_OFFSET 0x10

struct pci_default_header {
    uint32_t bar[6];
    uint32_t cardbus_cis_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_bar;
    uint8_t capabilities_ptr;
    uint8_t reserved[7];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} __attribute__((packed));

static_assert(sizeof(struct pci_default_header) == 0x30);

struct pci_bridge_header {
    uint32_t bar[2];
    uint8_t prim_bus_number;
    uint8_t sec_bus_number;
    uint8_t sub_bus_number;
    uint8_t sec_latency_timer;
    uint8_t io_base_low;
    uint8_t io_limit_low;
    uint16_t sec_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetch_memory_base;
    uint16_t prefetch_memory_limit;
    uint32_t prefetch_base;
    uint32_t prefetch_limit;
    uint16_t io_limit_high;
    uint16_t io_base_high;
    uint8_t reserved[3];
    uint8_t capabilities_ptr;
    uint32_t expansion_rom_bar;
    uint8_t interrupt_pin;
    uint8_t interrupt_line;
    uint16_t bridge_control;
} __attribute__((packed));

static_assert(sizeof(struct pci_bridge_header) == 0x30);

struct pci_cardbus_header {
    uint32_t cardbus_socket;
    uint8_t capabilities_offset;
    uint8_t reserved1;
    uint16_t sec_status;
    uint8_t pci_bus_number;
    uint8_t cardbus_bus_number;
    uint8_t sub_bus_number;
    uint8_t latency_timer;
    struct {
        uint32_t bar;
        uint32_t limit;
    } memory[2], io[2];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t bridge_control;
    uint16_t subsystem_device_id;
    uint16_t subsystem_vendor_id;
    uint32_t legacy_bar;
} __attribute__((packed));

static_assert(sizeof(struct pci_cardbus_header) == 0x38);

struct pci_header {
    enum pci_header_type type;

    uint16_t device_id;
    uint16_t vendor_id;

    enum pci_class class;
    enum pci_subclass subclass;
    uint8_t prog_if;
    uint8_t rev_id;

    union {
        alignas(uint32_t) struct pci_default_header ext_default;
        alignas(uint32_t) struct pci_bridge_header ext_bridge;
        alignas(uint32_t) struct pci_cardbus_header ext_cardbus;
    };
};

struct pci_capability {
    struct {
        enum pci_capability_id cap_id;
        uint8_t next_ptr;
        uint16_t message_control;
    } __attribute__((packed));

    struct pci_capability* next;
    uint8_t pci_offset;
};

struct pci_device {
    spinlock_t lock;

    int64_t parent;
    uint8_t bus;
    uint8_t func;
    uint8_t device;

    struct pci_header header;
    struct pci_capability* capabilities;
};

struct pic_bar {
    uintptr_t base;
    size_t size;

    bool is_mmio;
    bool is_prefetchable;
};

struct pci_device_id {
    uint16_t device_id;
    const char* device;
} __attribute__((packed));

struct pci_vendor_id {
    uint16_t vendor_id;
    const char* vendor;

    size_t num_device_ids;
    struct pci_device_id* device_ids;
} __attribute__((packed));

extern const size_t pci_id_lookup_table_size;
extern const struct pci_vendor_id pci_id_lookup_table[];

extern struct dynarray devices;

void pci_init(void);

uint16_t pci_device_read_word(const struct pci_device* device, uint32_t offset);
void pci_device_write_word(const struct pci_device* device, uint32_t offset, uint16_t value);

uint32_t pci_device_read_dword(const struct pci_device* device, uint32_t offset);
void pci_device_write_dword(const struct pci_device* device, uint32_t offset, uint32_t value);

uint16_t pci_device_get_status(const struct pci_device* device);

static inline bool pci_device_has_capabilities(const struct pci_device* device) {
    uint16_t status = pci_device_get_status(device);
    return !!(status & 0x10);
}

//int pci_device_get_capabilities(const struct pci_device* device, uint8_t* capabilities);

int pci_device_load_capabilities(struct pci_device* device);
struct pci_capability* pci_device_get_capability(struct pci_device* device, enum pci_capability_id cap);

const struct pci_vendor_id* pci_lookup_vendor_id(uint16_t vendor_id);
const struct pci_device_id* pci_lookup_device_id(const struct pci_vendor_id*, uint16_t device_id);


#endif /* _AMETHYST_DRIVERS_PCI_H */

