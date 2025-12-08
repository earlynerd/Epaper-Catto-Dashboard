#ifndef PLOT_MANAGER_H
#define PLOT_MANAGER_H

#include "core/SharedTypes.h"
#include "core/Config.h"
#include "ui/ScatterPlot.h"
#include "ui/Histogram.h"
#include "ui/Widget.h"
#include "ui/StatusBox.h"
#include "ui/TextLabel.h"
#include "core/DataManager.h"
#include "ui/PlotDataTypes.h"

class PlotManager {
public:
    PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *display, DataManager* datamanager);
    
    void renderDashboard(const std::vector<SL_Pet> &pets, 
                         PetDataMap &allPetData, 
                         const DateRangeInfo &range,
                         const SL_Status &status,
                         bool wifiSuccess);
private:
    GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *_display;
    DataManager* _dataManager;
    // Constants for colors, layout, etc.
    const std::vector<ColorPair> _petColors = {
        {EPD_RED, EPD_YELLOW}, {EPD_BLUE, EPD_BLACK}, 
        {EPD_GREEN, EPD_YELLOW}, {EPD_BLACK, EPD_WHITE}
    };
};

#endif