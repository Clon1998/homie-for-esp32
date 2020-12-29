#pragma once

typedef enum {
    DSTATE_READY = 0,
    DSTATE_INIT = 1,
    DSTATE_DISCONNECTED = 2,
    DSTATE_SLEEPING = 3,
    DSTATE_LOST = 4,
    DSTATE_ALERT = 5
} HomieDeviceState;