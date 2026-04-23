#ifndef CONFIG_H
#define CONFIG_H

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
};

#endif
