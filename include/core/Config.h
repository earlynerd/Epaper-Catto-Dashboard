#ifndef CONFIG_H__
#define CONFIG_H__
#include "WiFiProvisioner.h"
#include <Arduino.h> // Needed for types like uint8_t
//#define ENABLE_GxEPD2_4G_GFX true



#if (EPD_SELECT == 1001)
#include <GxEPD2_4G_4G.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_4G_4G
#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7 // 7.5'' B&W driver
#elif (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01 // 7.3'' Color driver
#endif

// somehow there should be an easier way to do this
#define GxEPD2_4G_IS_GxEPD2_4G_4G true
#define GxEPD2_BW_IS_GxEPD2_BW true
#define GxEPD2_3C_IS_GxEPD2_3C true
#define GxEPD2_4C_IS_GxEPD2_4C true
#define GxEPD2_7C_IS_GxEPD2_7C true
#define GxEPD2_1248_IS_GxEPD2_1248 true
#define GxEPD2_1248c_IS_GxEPD2_1248c true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_4G(x) IS_GxEPD(GxEPD2_4G_IS_, x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)
#define IS_GxEPD2_3C(x) IS_GxEPD(GxEPD2_3C_IS_, x)
#define IS_GxEPD2_4C(x) IS_GxEPD(GxEPD2_4C_IS_, x)
#define IS_GxEPD2_7C(x) IS_GxEPD(GxEPD2_7C_IS_, x)
#define IS_GxEPD2_1248(x) IS_GxEPD(GxEPD2_1248_IS_, x)
#define IS_GxEPD2_1248c(x) IS_GxEPD(GxEPD2_1248c_IS_, x)

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.

#if IS_GxEPD2_BW(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
#elif IS_GxEPD2_3C(GxEPD2_DISPLAY_CLASS) || IS_GxEPD2_4C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
#elif IS_GxEPD2_7C(GxEPD2_DISPLAY_CLASS)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
#include "GxEPD2_GFX.h"
#elif IS_GxEPD2_4G(GxEPD2_DISPLAY_CLASS)

#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 4) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 4))
#endif



//  Namespace for configuration to avoid pollution
namespace Config
{

    constexpr int EPD_WIDTH_PX = 800;
    constexpr int EPD_HEIGHT_PX = 480;

    // Define ePaper SPI pins
    namespace Pins
    {
        constexpr int EPD_SCK = 7;
        constexpr int EPD_MOSI = 9;
        constexpr int EPD_CS = 10;
        constexpr int EPD_DC = 11;
        constexpr int EPD_RES = 12;
        constexpr int EPD_BUSY = 13;

        constexpr int SERIAL_RX = 44;
        constexpr int SERIAL_TX = 43;
        constexpr int LED = 6; // GPIO6 - Onboard LED (inverted logic)

        constexpr int BUZZER = 45; // GPIO45 - Buzzer

        // I2C pins for reTerminal E Series
        constexpr int I2C_SDA_PIN = 19;
        constexpr int I2C_SCL_PIN = 20;

        // Battery monitoring pins
        constexpr int BATTERY_ADC = 1;     // GPIO1 - Battery voltage ADC
        constexpr int BATTERY_ENABLE = 21; // GPIO21 - Battery monitoring enable

        // SD Card Pin Definitions
        constexpr int SD_EN = 16;  // Power enable for the SD card slot
        constexpr int SD_DET = 15; // Card detection pin
        constexpr int SD_CS = 14;  // Chip Select for the SD card
        constexpr int SD_MOSI = 9; // Shared with ePaper Display
        constexpr int SD_MISO = 8;
        constexpr int SD_SCK = 7; // Shared with ePaper Display

        // Define button pins according to schematic
        constexpr int BUTTON_KEY0 = 3; // KEY0 - GPIO3
        constexpr int BUTTON_KEY1 = 4; // KEY1 - GPIO4
        constexpr int BUTTON_KEY2 = 5; // KEY2 - GPIO5
    }

    // Select the ePaper driver to use
    // 1001: reTerminal E1001 (7.5'' B&W)
    // 1002: reTerminal E1002 (7.3'' Color)



// Macro left for type usage in templated classes
// #define MAX_HEIGHT(EPD)                                        \
    //    (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 4) \
    //        ? EPD::HEIGHT                                         \
     //       : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 4))
//#define MAX_HEIGHT(EPD) 480
    constexpr double GRAMS_PER_POUND = 453.592;

    // Bitmasks for ESP32 EXT1 wakeup
    // 1ULL << Pin
    constexpr uint64_t BUTTON_KEY0_MASK = (1ULL << Pins::BUTTON_KEY0);
    constexpr uint64_t BUTTON_KEY1_MASK = (1ULL << Pins::BUTTON_KEY1);
    constexpr uint64_t BUTTON_KEY2_MASK = (1ULL << Pins::BUTTON_KEY2);

    constexpr int WIFI_TIMEOUT_MS = 20000;

    constexpr const char *TIME_API_URL = "https://worldtimeapi.org/api/ip";
    constexpr int MAX_SYNC_RETRIES = 50;

    constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
    constexpr const char *NTP_SERVER_2 = "time.nist.gov";

    // Colors
    constexpr uint16_t EPD_BLACK_COLOR = 0x0000;
    constexpr uint16_t EPD_BLUE_COLOR = 0x001F;
    constexpr uint16_t EPD_GREEN_COLOR = 0x07E0;
    constexpr uint16_t EPD_RED_COLOR = 0xF800;
    constexpr uint16_t EPD_YELLOW_COLOR = 0xFFE0;
    constexpr uint16_t EPD_WHITE_COLOR = 0xFFFF;
    constexpr uint16_t EPD_DARKGREY_COLOR = 0x7BEF;  /* 128, 128, 128 */
    constexpr uint16_t EPD_LIGHTGREY_COLOR = 0xC618; /* 192, 192, 192 */
}

// Macros Removed for Cleanliness (Refactoring Phase 2)
// Users must migrate to Config Namespace
#define EPD_WIDTH Config::EPD_WIDTH_PX
#define EPD_HEIGHT Config::EPD_HEIGHT_PX

// Retaining Colors as macros is common for GFX libraries, but we should switch.
// Mapping them for now to avoid breaking custom graphic code not yet refactored.

#define EPD_BLACK Config::EPD_BLACK_COLOR
#define EPD_BLUE Config::EPD_BLUE_COLOR
#define EPD_GREEN Config::EPD_GREEN_COLOR
#define EPD_RED Config::EPD_RED_COLOR
#define EPD_YELLOW Config::EPD_YELLOW_COLOR
#define EPD_WHITE Config::EPD_WHITE_COLOR
#define EPD_DARKGREY Config::EPD_DARKGREY_COLOR
#define EPD_LIGHTGREY Config::EPD_LIGHTGREY_COLOR

#endif