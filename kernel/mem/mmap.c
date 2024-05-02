#include "mem/pmm.h"
#include <mem/mmap.h>
#include <kernelio.h>

const char* mmap_types[] = {
    "Invalid",
    "Available",
    "Reserved",
    "Reclaimable",
    "NVS",
    "Defective"
};

