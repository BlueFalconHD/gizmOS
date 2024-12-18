#include "gear.h"
#include "memory.h"

gear_t *galloc(unsigned int size)
{
    /* Get root axel */
    axel_t *root = (axel_t *)MEM_START;

    /* Check if root axel has enough space for a new gear pointer */
    if (root->count >= root->size) {
        return NULL; /* No space in the gears array */
    }

    /* Ensure mem_free is aligned to 8-byte boundary */
    mem_free = (mem_free + 7) & ~((uintptr_t)7);

    /* Calculate the total size of the new gear including its data */
    unsigned int gear_size = sizeof(gear_t) + size;

    /* Check if there's enough free memory */
    if (mem_free + gear_size > MEM_START + MEM_SIZE) {
        return NULL; /* Not enough memory */
    }

    /* Allocate new gear at mem_free */
    gear_t *new_gear = (gear_t *)mem_free;
    mem_free += gear_size; /* Update mem_free */

    /* Initialize the new gear */
    new_gear->type = GEAR_TYPE_UNKNOWN;
    new_gear->id = root->count; /* Assign an ID */
    new_gear->permissions = 0;  /* Set default permissions */
    new_gear->size = size;

    /* Store the new gear's pointer in the gears array */
    root->gears[root->count] = new_gear;

    /* Update root axel's count */
    root->count += 1;

    return new_gear;
}
