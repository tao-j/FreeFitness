// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2023-2026 Tao Jin
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define DEVICE_NAME_PREFIX "FreeFitness"
#define VERSION_SW_MAJOR   0
#define VERSION_SW_MINOR   1
#define VERSION_SW_PATCH   3
#define VERSION_SW_STR     "0.1.3"
#define VERSION_FW_STR     "0.1.3"

#define VERSION_HW_REV     1
#define VERSION_HW_STR     "1.0.0"

#define MANUFACTURER_NAME  "Yzz"
#define MANUFACTURER_ID    0x3862

struct ProfileConfig {
    bool ant_pwr = true;
    bool ant_csc = false;
    bool ant_fec = false;
    bool ble_cp = true;
    bool ble_csc = false;
    bool ble_ftms = false;
    uint8_t brightness = 5;  // 1..5, mapped by ui_manager; applied live
};

// brightness level 1..5 → M5.Display.setBrightness() value.
// Level 1 is dim-but-visible (lower bound capped above 0).
inline uint8_t brightness_to_level(uint8_t level) {
    static const uint8_t kLevels[] = {32, 80, 128, 192, 255};
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    return kLevels[level - 1];
}

#endif
