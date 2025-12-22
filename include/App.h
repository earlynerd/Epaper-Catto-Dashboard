#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include "core/Config.h"
#include "core/SharedTypes.h"
#include "core/DataManager.h"
#include "core/NetworkManager.h"
#include "ui/PlotManager.h"
#include "RTClib.h"
#include "Adafruit_SHT4x.h"

class App {
public:
    App();
    void setup();
    void loop();

private:
    // Lifecycle methods
    void initHardware();
    void checkFactoryReset();
    void initStorage();
    void updateData(bool isViewUpdate);
    void renderView(int rangeIndex, const SL_Status& status, float vbat);
    void enterSleep();

    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display;
    RTC_PCF8563 rtc;
    Adafruit_SHT4x sht4;
    SPIClass hspi;

    DataManager dataManager;
    NetworkManager *networkManager;
    PlotManager *plotManager;

    PetDataMap allPetData;
    std::vector<SL_Pet> allPets;
};

#endif
