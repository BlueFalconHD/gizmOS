#include "virtio_kbd.h"
#include "virtio.h"
#include <lib/macros.h>
#include <lib/memory.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <page_table.h>
#include <physical_alloc.h>

#include <lib/memory.h>
#include <lib/print.h>
#include <lib/str.h>

void hex_str_padded_uint64(uint64_t value, char *buffer, int width) {
  char temp[20];
  int i = 0;

  if (value == 0) {
    temp[i++] = '0';
  } else {
    while (value > 0 && i < 16) { // Ensure we don't exceed buffer size
      uint8_t digit = value & 0xF;
      temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
      value >>= 4;
    }
  }

  // Calculate total digits needed and padding
  int total_digits = (i > width) ? i : width;
  int padding = total_digits - i;
  int j = 0;

  // Add leading zeros
  for (; j < padding && j < width; j++) {
    buffer[j] = '0';
  }

  // Reverse the digits into buffer
  for (int k = 0; k < i && (j + k) < 20; k++) {
    buffer[j + k] = temp[i - k - 1];
  }

  buffer[j + i] = '\0';
}

void hex_str_padded_uint8(uint8_t value, char *buffer) {
  const char hex_digits[] = "0123456789abcdef";
  buffer[0] = hex_digits[(value >> 4) & 0xF];
  buffer[1] = hex_digits[value & 0xF];
  buffer[2] = '\0';
}

void hexdump(uint64_t start, uint64_t size) {
  char line_buffer[128];

  for (uint64_t i = 0; i < size; i += 16) {
    // Clear line buffer
    memset(line_buffer, 0, sizeof(line_buffer));

    // Write the address
    char addr_buffer[20]; // Enough for address
    hex_str_padded_uint64(start + i, addr_buffer, 8);

    // Build the line_buffer
    // Start with the address
    strcat(line_buffer, addr_buffer);
    strcat(line_buffer, ": ");

    // Now add the hex bytes
    for (uint64_t j = 0; j < 16; j++) {
      if (i + j >= size) {
        // Reached the end of data
        break;
      }
      uint8_t byte = *(uint8_t *)(start + i + j);

      // Convert byte to hex string
      char byte_buffer[4]; // 2 hex digits + space + null terminator

      hex_str_padded_uint8(byte, byte_buffer);

      // Append to line_buffer
      strcat(line_buffer, byte_buffer);
      strcat(line_buffer, " ");
    }

    // Optional: Add ASCII representation at the end
    // Uncomment the following code block if you wish to include ASCII
    /*
    strcat(line_buffer, " ");
    for (uint64_t j = 0; j < 16; j++) {
      if (i + j >= size) {
        break;
      }
      uint8_t byte = *(uint8_t *)(start + i + j);
      char ascii_char = (byte >= 32 && byte <= 126) ? byte : '.';
      char ascii_buffer[2] = {ascii_char, '\0'};
      strcat(line_buffer, ascii_buffer);
    }
    */

    // Output the line
    strcat(line_buffer, "\n");
    print(line_buffer, PRINT_FLAG_BOTH);
  }
}

#define NUM_EVENTS 16

static struct virtq_desc *kdesc;
static struct virtq_avail *kavail;
static struct virtq_used *kused;
static struct virtio_input_event kevents[NUM_EVENTS];
static uint16_t kfree[NUM_EVENTS];
static uint16_t kused_idx;

static struct virtq_desc *cdesc;
static struct virtq_avail *cavail;
static struct virtq_used *cused;
static struct virtio_input_event cevents[NUM_EVENTS];
static uint16_t cfree[NUM_EVENTS];
static uint16_t cused_idx;

#define R(r) ((volatile uint32_t *)(VIRTIO0 + (r)))

struct {
  uint16_t select;   // VIRTIO_INPUT_CFG_ID_STATUS (==1)
  uint16_t reserved; // zero
  uint32_t size;     // length of payload (1)
  uint8_t data;      // 1 == DRIVER_OK
} __attribute__((packed)) status_pkt = {
    .select = 1, .reserved = 0, .size = 1, .data = 1};

G_INLINE void setup_virtio_queue(uint32_t qsel, struct virtq_desc *desc,
                                 struct virtq_avail *avail,
                                 struct virtq_used *used, void *buffers,
                                 uint16_t *free_map) {
  // select the queue
  *R(VIRTIO_MMIO_QUEUE_SEL) = qsel;

  // tell the device how big we want it
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM_EVENTS;

  // allocate pages (assume caller zeroes them/checked errors)
  uint64_t p_desc = V2P((uint64_t)desc);
  uint64_t p_avail = V2P((uint64_t)avail);
  uint64_t p_used = V2P((uint64_t)used);

  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = p_desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = p_desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = p_avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = p_avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = p_used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = p_used >> 32;

  // mark the queue ready
  *R(VIRTIO_MMIO_QUEUE_READY) = 1;

  // prime all descriptors into the avail ring
  for (int i = 0; i < NUM_EVENTS; i++) {
    free_map[i] = 1;
    desc[i].addr = V2P((uint64_t)&((struct virtio_input_event *)buffers)[i]);
    desc[i].len = sizeof(struct virtio_input_event);
    desc[i].flags = VRING_DESC_F_WRITE; // device writes into these
    avail->ring[i] = i;
  }
  avail->idx = NUM_EVENTS;
  __sync_synchronize();

  // kick it
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = qsel;
}

void virtio_keyboard_init(void) {
  uint32_t status = 0;

  // 1) verify device
  if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
      *R(VIRTIO_MMIO_VERSION) != 2 || *R(VIRTIO_MMIO_DEVICE_ID) != 18 ||
      *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
    panic("virtio keyboard not found");

  // 2) reset & ACK/DRIVER
  *R(VIRTIO_MMIO_STATUS) = status;
  status |= VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER;
  *R(VIRTIO_MMIO_STATUS) = status;

  // 3) negotiate (none for keyboard)
  *R(VIRTIO_MMIO_DRIVER_FEATURES) = 0;
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
  if (!(*R(VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio FEATURES_OK unset");

  // 4) set up the input queue (queue 0)
  kdesc = alloc_page();
  memset(kdesc, 0, PAGE_SIZE);
  kavail = alloc_page();
  memset(kavail, 0, PAGE_SIZE);
  kused = alloc_page();
  memset(kused, 0, PAGE_SIZE);
  if (!kdesc || !kavail || !kused)
    panic("kbd page alloc");

  setup_virtio_queue(0, kdesc, kavail, kused, kevents, kfree);

  // (optional) drop the control queue entirely until you need config cmds
  // if you do need it, call setup_virtio_queue(1, cdesc, cavail, cused,
  // cevents, cfree) and then push your status_pkt into it before DRIVER_OK.
  cdesc = alloc_page();
  memset(cdesc, 0, PAGE_SIZE);
  cavail = alloc_page();
  memset(cavail, 0, PAGE_SIZE);
  cused = alloc_page();
  memset(cused, 0, PAGE_SIZE);
  if (!cdesc || !cavail || !cused)
    panic("ctrl page alloc");

  setup_virtio_queue(1, cdesc, cavail, cused, cevents, cfree);

  cdesc[0].addr = V2P((uint64_t)&status_pkt);
  cdesc[0].len = sizeof(status_pkt);
  cdesc[0].flags = 0; // device reads this
  cavail->ring[0] = 0;
  cavail->idx = 1;
  __sync_synchronize();
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 1; // kick queue1

  // 5) DRIVER_OK — now the device will start sending events on queue0
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;
}

static inline uint32_t virtio_read(uint32_t reg) { return *R(reg); }
static inline void virtio_write(uint32_t reg, uint32_t v) { *R(reg) = v; }

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03

#define KEY_RELEASE 0
#define KEY_PRESS 1
#define KEY_REPEAT 2

const char *ev_type_str(uint16_t type) {
  switch (type) {
  case EV_SYN:
    return "EV_SYN";
  case EV_KEY:
    return "EV_KEY";
  case EV_REL:
    return "EV_REL";
  case EV_ABS:
    return "EV_ABS";
  default:
    return "UNKNOWN";
  }
}

const char *ev_value_str(uint32_t value) {
  switch (value) {
  case KEY_RELEASE:
    return "RELEASE";
  case KEY_PRESS:
    return "PRESS";
  case KEY_REPEAT:
    return "REPEAT";
  default:
    return "UNKNOWN";
  }
}

void handle_virtio_keyboard_irq(void) {
  uint32_t ist = virtio_read(VIRTIO_MMIO_INTERRUPT_STATUS);
  virtio_write(VIRTIO_MMIO_INTERRUPT_ACK, ist);

  while (kused_idx != kused->idx) {
    uint16_t pos = kused_idx % NUM_EVENTS;
    uint16_t desc_id = kused->ring[pos].id;
    struct virtio_input_event *event = &kevents[desc_id];

    printf("Event received: type=%{type: str} (%{type: uint}), code=%{type: "
           "uint}, value=%{type: str} (%{type: uint})\n",
           PRINT_FLAG_BOTH, ev_type_str(event->type), event->type, event->code,
           ev_value_str(event->value), event->value);

    // recycle the descriptor
    kfree[desc_id] = 1;
    kavail->ring[kavail->idx % NUM_EVENTS] = desc_id;
    kavail->idx++;
    kused_idx++;
  }

  __sync_synchronize();
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
}
