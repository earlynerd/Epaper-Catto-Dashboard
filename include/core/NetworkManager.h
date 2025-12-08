#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TzDbLookup.h>
#include "WiFiProvisioner.h"
#include "SmartLitterbox.h"
#include "PetKitApi.h"
#include "WhiskerApi.h"
#include "core/Config.h"
#include "RTClib.h"
#include "core/DataManager.h"

#if (EPD_SELECT == 1002)
#include <GxEPD2_7C.h>
#elif (EPD_SELECT == 1001)
#include <GxEPD2_BW.h>
#endif
class NetworkManager {
public:
    NetworkManager(DataManager* dataManager);
    
    // Connect to WiFi, falling back to provisioning if it fails
    void connectOrProvision(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display);

    //load time from rtc, and set timezone from NVS
    bool initializeFromRtc(RTC_PCF8563& rtc);

    // Sync time via API or NTP
    bool syncTime(RTC_PCF8563& rtc);
    
    // Initialize API client
    bool initApi();
    
    // Get the API client instance
    SmartLitterbox* getApi() { return _litterbox; }
    
    // Clear credentials
    void factoryReset();

private:
    WhiskerApi* _whisker = nullptr;
    PetKitApi* _petkit = nullptr;
    SmartLitterbox* _litterbox = nullptr;
    //PetKitApi* _petkit = nullptr;
    char _time_zone[64];
    bool _ispetkit = false;
    bool getTimezoneAndSync(RTC_PCF8563& rtc);
    DataManager* _dataManager;
    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *_display;
    void printProvMessage();
};

#endif