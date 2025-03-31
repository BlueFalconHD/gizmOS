#include "plic.h"
#include <lib/result.h>
#include <physical_alloc.h>

// PLIC register offsets (based on RISC-V PLIC specification)
#define PLIC_PRIORITY_BASE 0x000000 // Priority registers (4 bytes per source)
#define PLIC_PENDING_BASE 0x001000  // Pending bits (1 bit per source)
#define PLIC_ENABLE_BASE 0x002000 // Enable bits (1 bit per source per context)
#define PLIC_THRESHOLD_BASE 0x200000 // Priority threshold (4 bytes per context)
#define PLIC_CLAIM_COMPLETE_BASE                                               \
  0x200004 // Claim/complete (4 bytes per context)

// Default configuration
#define PLIC_DEFAULT_MAX_PRIORITY 7 // Max priority (1-7, 0 = disabled)
#define PLIC_DEFAULT_IRQ_COUNT                                                 \
  1024                            // Default max interrupts (platform dependent)
#define PLIC_DEFAULT_HART_COUNT 1 // Default single hart
#define PLIC_DEFAULT_CONTEXTS 2   // M-mode and S-mode

// Internal helper functions
static uint32_t *plic_enable_addr(plic_t *plic, uint32_t hart, uint32_t context,
                                  uint32_t irq) {
  uint32_t enable_index = irq / 32;
  uint32_t context_offset =
      (hart * plic->context_count + context) * (PLIC_DEFAULT_IRQ_COUNT / 32);
  return (uint32_t *)((uint64_t)plic->base + PLIC_ENABLE_BASE +
                      (context_offset + enable_index) * 4);
}

RESULT_TYPE(plic_t *) make_plic(uint64_t base) {
  plic_t *plic = (plic_t *)alloc_page();
  if (!plic) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  // Initialize basic fields
  plic->base = base;
  plic->is_initialized = false;
  plic->hart_count = PLIC_DEFAULT_HART_COUNT;
  plic->context_count = PLIC_DEFAULT_CONTEXTS;
  plic->max_priority = PLIC_DEFAULT_MAX_PRIORITY;
  plic->irq_count = PLIC_DEFAULT_IRQ_COUNT;

  return RESULT_SUCCESS(plic);
}

g_bool plic_init(plic_t *plic) {
  if (!plic) {
    return false;
  }

  if (plic->is_initialized) {
    return true;
  }

  // Disable all interrupts for all contexts
  for (uint32_t hart = 0; hart < plic->hart_count; hart++) {
    for (uint32_t context = 0; context < plic->context_count; context++) {
      // Calculate base address for this hart/context enable array
      uint32_t enable_words = plic->irq_count / 32;
      for (uint32_t word = 0; word < enable_words; word++) {
        uint32_t *enable_addr =
            plic_enable_addr(plic, hart, context, word * 32);
        *enable_addr = 0;
      }

      // Set threshold to maximum (masks all interrupts)
      uint32_t *threshold_addr =
          (uint32_t *)((uint64_t)plic->base + PLIC_THRESHOLD_BASE +
                       (hart * plic->context_count + context) * 0x1000);
      *threshold_addr = plic->max_priority;
    }
  }

  // Set all interrupt priorities to 0 (disabled)
  for (uint32_t irq = 1; irq < plic->irq_count; irq++) {
    uint32_t *priority_addr =
        (uint32_t *)((uint64_t)plic->base + PLIC_PRIORITY_BASE + irq * 4);
    *priority_addr = 0;
  }

  plic->is_initialized = true;
  return true;
}

g_bool plic_set_priority(plic_t *plic, uint32_t irq, uint32_t priority) {
  if (!plic || !plic->is_initialized || irq >= plic->irq_count) {
    return false;
  }

  // Ensure priority is within allowed range
  if (priority > plic->max_priority) {
    priority = plic->max_priority;
  }

  // Set priority for this interrupt
  uint32_t *priority_addr =
      (uint32_t *)((uint64_t)plic->base + PLIC_PRIORITY_BASE + irq * 4);
  *priority_addr = priority;

  return true;
}

g_bool plic_set_threshold(plic_t *plic, uint32_t hart, uint32_t context,
                          uint32_t threshold) {
  if (!plic || !plic->is_initialized || hart >= plic->hart_count ||
      context >= plic->context_count) {
    return false;
  }

  // Ensure threshold is within allowed range
  if (threshold > plic->max_priority) {
    threshold = plic->max_priority;
  }

  // Set threshold for this hart/context
  uint32_t *threshold_addr =
      (uint32_t *)((uint64_t)plic->base + PLIC_THRESHOLD_BASE +
                   (hart * plic->context_count + context) * 0x1000);
  *threshold_addr = threshold;

  return true;
}

g_bool plic_enable_interrupt(plic_t *plic, uint32_t hart, uint32_t context,
                             uint32_t irq) {
  if (!plic || !plic->is_initialized || hart >= plic->hart_count ||
      context >= plic->context_count || irq >= plic->irq_count ||
      irq == 0) { // IRQ 0 is reserved
    return false;
  }

  uint32_t enable_bit = 1 << (irq % 32);
  uint32_t *enable_addr = plic_enable_addr(plic, hart, context, irq);

  // Set the enable bit for this interrupt
  *enable_addr |= enable_bit;

  return true;
}

g_bool plic_disable_interrupt(plic_t *plic, uint32_t hart, uint32_t context,
                              uint32_t irq) {
  if (!plic || !plic->is_initialized || hart >= plic->hart_count ||
      context >= plic->context_count || irq >= plic->irq_count) {
    return false;
  }

  uint32_t enable_bit = 1 << (irq % 32);
  uint32_t *enable_addr = plic_enable_addr(plic, hart, context, irq);

  // Clear the enable bit for this interrupt
  *enable_addr &= ~enable_bit;

  return true;
}

uint32_t plic_claim(plic_t *plic, uint32_t hart, uint32_t context) {
  if (!plic || !plic->is_initialized || hart >= plic->hart_count ||
      context >= plic->context_count) {
    return 0;
  }

  // Read the claim register for this hart/context
  uint32_t *claim_addr =
      (uint32_t *)((uint64_t)plic->base + PLIC_CLAIM_COMPLETE_BASE +
                   (hart * plic->context_count + context) * 0x1000);

  return *claim_addr;
}

g_bool plic_complete(plic_t *plic, uint32_t hart, uint32_t context,
                     uint32_t irq) {
  if (!plic || !plic->is_initialized || hart >= plic->hart_count ||
      context >= plic->context_count || irq >= plic->irq_count ||
      irq == 0) { // IRQ 0 is reserved
    return false;
  }

  // Write to the claim/complete register to signal completion
  uint32_t *complete_addr =
      (uint32_t *)((uint64_t)plic->base + PLIC_CLAIM_COMPLETE_BASE +
                   (hart * plic->context_count + context) * 0x1000);

  *complete_addr = irq;

  return true;
}

g_bool plic_is_pending(plic_t *plic, uint32_t irq) {
  if (!plic || !plic->is_initialized || irq >= plic->irq_count || irq == 0) {
    return false;
  }

  uint32_t pending_bit = 1 << (irq % 32);
  uint32_t *pending_addr =
      (uint32_t *)((uint64_t)plic->base + PLIC_PENDING_BASE + (irq / 32) * 4);

  return (*pending_addr & pending_bit) != 0;
}
