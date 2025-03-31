#pragma once

#include <lib/result.h>
#include <lib/types.h>

/**
 * PLIC device structure.
 * This structure holds the base address of the PLIC device and a flag
 * indicating whether the device has been initialized.
 */
typedef struct {
  uint64_t base;
  g_bool is_initialized;
  uint32_t hart_count;    // Number of harts (processor cores)
  uint32_t context_count; // Number of contexts per hart (usually 2: M-mode and
                          // S-mode)
  uint32_t max_priority;  // Maximum priority level supported
  uint32_t irq_count;     // Number of interrupt sources supported
} plic_t;

// Context IDs for different privilege modes
#define PLIC_CONTEXT_MACHINE 0
#define PLIC_CONTEXT_SUPERVISOR 1

/**
 * Creates a new PLIC device.
 * @param base The base address of the PLIC device.
 * @return A result_t that can safely be cast to a plic_t pointer if successful.
 */
RESULT_TYPE(plic_t *) make_plic(uint64_t base);

/**
 * Initializes the PLIC device.
 * @param plic The PLIC device to initialize.
 * @return A g_bool indicating success or failure.
 */
g_bool plic_init(plic_t *plic);

/**
 * Sets the priority of an interrupt source.
 * @param plic The PLIC device.
 * @param irq The interrupt source ID.
 * @param priority The priority level (0 = disabled, 1 = lowest, 7 = highest).
 * @return A g_bool indicating success or failure.
 */
g_bool plic_set_priority(plic_t *plic, uint32_t irq, uint32_t priority);

/**
 * Sets the threshold priority for a hart and context.
 * Only interrupts with priority > threshold will be delivered.
 * @param plic The PLIC device.
 * @param hart The hart ID.
 * @param context The context ID (PLIC_CONTEXT_MACHINE or
 * PLIC_CONTEXT_SUPERVISOR).
 * @param threshold The threshold level (0-7).
 * @return A g_bool indicating success or failure.
 */
g_bool plic_set_threshold(plic_t *plic, uint32_t hart, uint32_t context,
                          uint32_t threshold);

/**
 * Enables an interrupt source for a specific hart and context.
 * @param plic The PLIC device.
 * @param hart The hart ID.
 * @param context The context ID (PLIC_CONTEXT_MACHINE or
 * PLIC_CONTEXT_SUPERVISOR).
 * @param irq The interrupt source ID to enable.
 * @return A g_bool indicating success or failure.
 */
g_bool plic_enable_interrupt(plic_t *plic, uint32_t hart, uint32_t context,
                             uint32_t irq);

/**
 * Disables an interrupt source for a specific hart and context.
 * @param plic The PLIC device.
 * @param hart The hart ID.
 * @param context The context ID (PLIC_CONTEXT_MACHINE or
 * PLIC_CONTEXT_SUPERVISOR).
 * @param irq The interrupt source ID to disable.
 * @return A g_bool indicating success or failure.
 */
g_bool plic_disable_interrupt(plic_t *plic, uint32_t hart, uint32_t context,
                              uint32_t irq);

/**
 * Claims the highest priority pending interrupt.
 * @param plic The PLIC device.
 * @param hart The hart ID.
 * @param context The context ID (PLIC_CONTEXT_MACHINE or
 * PLIC_CONTEXT_SUPERVISOR).
 * @return The claimed interrupt ID, or 0 if no interrupt is pending.
 */
uint32_t plic_claim(plic_t *plic, uint32_t hart, uint32_t context);

/**
 * Completes an interrupt, indicating it has been handled.
 * @param plic The PLIC device.
 * @param hart The hart ID.
 * @param context The context ID (PLIC_CONTEXT_MACHINE or
 * PLIC_CONTEXT_SUPERVISOR).
 * @param irq The interrupt ID to complete.
 * @return A g_bool indicating success or failure.
 */
g_bool plic_complete(plic_t *plic, uint32_t hart, uint32_t context,
                     uint32_t irq);

/**
 * Checks if an interrupt is pending.
 * @param plic The PLIC device.
 * @param irq The interrupt source ID.
 * @return g_bool True if the interrupt is pending, false otherwise.
 */
g_bool plic_is_pending(plic_t *plic, uint32_t irq);
