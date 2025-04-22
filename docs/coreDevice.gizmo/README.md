# coreDevice.gizmo

The coreDevice protocol aims to implement simple and easy, developer friendly, and extensible device communication protocols which bypass that of USB and PS/2.

coreDevice serves both as a robust and capable protocol for devices, and as an intermediary step between getting a low-level operating system or project running and implementing a full USB stack. It is not intended to be a replacement for USB, but rather a way to get started with HID devices without the complexity of USB.

## Devices

coreDevice supports keyboards and mice only. This choice has been made to ensure that the protocol is easy to implement.

## Protocol

### Devices

In coreDevice, each device is modeled as an identifiable entity who 'owns' a small portion of memory on the host, both for describing the device's type and state.

```c
enum coreDeviceType {
    KEYBOARD,
    MOUSE,
    CONTROLLER,
};

typedef struct coreDevice {
    enum coreDeviceType type;
    uint8_t id;
    void *state;
} coreDevice;
```

#### Device State

A device's state is composed of a list of events sent by the device to the host. Each event is a simple structure that contains a type and a payload.

```c
typedef struct coreDeviceEvent {
    enum coreDeviceEventType type;
    uint32_t length;
    void *payload;
} coreDeviceEvent;

typedef struct coreDeviceState {
    uint8_t length;
    coreDeviceEvent *events;
} coreDeviceState;
```
