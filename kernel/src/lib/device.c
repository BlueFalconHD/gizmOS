#include <lib/device.h>
#include <lib/panic.h>
#include <lib/print.h>
#include <lib/result.h>
#include <lib/str.h>

#define MAX_DEVICES 32

static device_t *devices[MAX_DEVICES];
static int device_count = 0;

result_t device_register(device_t *dev) {
  if (!dev || !dev->name) {
    return RESULT_FAILURE(RESULT_INVALID);
  }

  // Check if we've reached the maximum number of devices
  if (device_count >= MAX_DEVICES) {
    return RESULT_FAILURE(RESULT_NOMEM);
  }

  // Check for duplicate device
  for (int i = 0; i < device_count; i++) {
    if (devices[i] == dev || strcmp(devices[i]->name, dev->name) == 0) {
      return RESULT_FAILURE(RESULT_BUSY);
    }
  }

  // Add device to registry
  devices[device_count++] = dev;

  return RESULT_SUCCESS(0);
}

device_t *device_find_by_name(const char *name) {
  if (!name) {
    return NULL;
  }

  for (int i = 0; i < device_count; i++) {
    if (strcmp(devices[i]->name, name) == 0) {
      return devices[i];
    }
  }

  return NULL;
}

device_t *device_find_by_type(device_type_t type, int index) {
  int found_count = 0;

  for (int i = 0; i < device_count; i++) {
    if (devices[i]->type == type) {
      if (found_count == index) {
        return devices[i];
      }
      found_count++;
    }
  }

  return NULL;
}

result_t device_init_all(void) {
  result_t result;

  for (int i = 0; i < device_count; i++) {
    if (!devices[i]->initialized && devices[i]->init) {
      result = devices[i]->init(devices[i]);
      if (!result_is_ok(result)) {
        // Log the failure but continue with other devices
        printf("Failed to initialize device '%{type: str}': %{type: int}\n",
               PRINT_FLAG_BOTH, devices[i]->name, result.code);
      }
    }
  }

  return RESULT_SUCCESS(0);
}
