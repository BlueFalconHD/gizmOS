#ifndef GEAR_H
#define GEAR_H

#include <stdint.h>

#define NULL ((void *)0)

typedef unsigned long uintptr_t;

enum GEAR_TYPE {
    GEAR_TYPE_GEAR = (uint8_t)1,
    GEAR_TYPE_AXEL = (uint8_t)2,
    GEAR_TYPE_UNKNOWN = (uint8_t)0xFF,
};

/* Gear can contain data or other gears */
typedef struct gear {
    uint8_t type;          /* Type of data */
    unsigned int id;       /* ID of gear */
    unsigned int permissions; /* Permissions */
    unsigned int size;     /* Size of data */
    unsigned char data[];  /* Data */
} gear_t;

/* Axel is a type that contains multiple gears */
typedef struct axel {
    unsigned int size;     /* Capacity of the gears array */
    unsigned int count;    /* Current number of gears */
    gear_t *gears[];       /* Array of pointers to gears */
} axel_t;

gear_t *galloc(unsigned int size);

#endif /* GEAR_H */
