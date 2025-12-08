#include "ui/PlotManager.h"
#include "Fonts/FreeMono9pt7b.h"
#include "Fonts/FreeMonoBold9pt7b.h"

// Layout Constants
namespace Layout {
    constexpr int HEADER_HEIGHT = 20; // Estimated from context
    
    // Histogram Layout
    constexpr int HIST_Y_OFFSET_DENOM = 4; // 1/4 of height from bottom
    constexpr int HIST_HEIGHT_DENOM = 4; // 1/4 of height
    constexpr int INTERVAL_HIST_WIDTH_DENOM_TWO = 8; // 3/8 for two hists
    constexpr int INTERVAL_HIST_WIDTH_DENOM_ONE = 4; // 3/4 for single hist
    
    // Battery Gauge
    constexpr int BATTERY_W = 59;
    constexpr int BATTERY_H = 22;
    constexpr int BATTERY_X_OFFSET = 15 + 60;
    
    // Litter Gauge
    constexpr int LITTER_GAUGE_WIDTH_DIVISOR = 4;
    constexpr int LITTER_GAUGE_HEIGHT_DIVISOR = 4;
    
    constexpr int PADDING_SMALL = 5;
    constexpr int PADDING_MEDIUM = 15;
    constexpr int PADDING_LARGE = 20;
}

PlotManager::PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp, DataManager* datamanager)
    : _display(disp), _dataManager(datamanager) {}

/**
 * @brief Renders the entire dashboard to the E-Paper display.
 * 
 * This is the master rendering function. It performs the following steps:
 * 1. Prepares data histograms and scatter plots from the raw PetDataMap.
 * 2. Draws Interval and Duration histograms (bottom left).
 * 3. Draws the Weight ScatterPlot (main area).
 * 4. Draws the Status Bar (Battery, Update Time, Litterbox Status).
 * 
 * @param pets Vector of Pet profiles.
 * @param allPetData Map of all historical pet data.
 * @param range The selected date range for the dashboard (7d, 30d, etc).
 * @param status The current status of the litterbox hardware.
 * @param wifiSuccess Status of the last update attempt (not currently visualized specifically, but passed).
 */
void PlotManager::renderDashboard(const std::vector<SL_Pet> &pets, PetDataMap &allPetData, const DateRangeInfo &range, const SL_Status &status, bool wifiSuccess)
{
    _display->fillScreen(GxEPD_WHITE);

    // Prepare vectors
    size_t numPets = pets.size();
    std::vector<DataPoint> pet_scatterplot[numPets];
    std::vector<float> interval_hist[numPets];
    std::vector<float> duration_hist[numPets];

    time_t now = time(NULL);
    time_t timeStart = now - range.seconds;

    int idx = 0;
    for (const auto &pet : pets)
    {
        if (allPetData.find(pet.id.toInt()) == allPetData.end())
        {
            idx++;
            continue;
        }

        time_t lastTimestamp = -1;
        for (auto const &recordPair : allPetData[pet.id.toInt()])
        {
            const SL_Record &record = recordPair.second;
            if (record.timestamp < timeStart)
                continue;

            float weight_lbs = (float)record.weight_lbs;
            struct tm *thistimestamp = localtime(&record.timestamp);
            time_t ts = mktime(thistimestamp);
            pet_scatterplot[idx].push_back({(float)ts, weight_lbs});
            if (record.duration_seconds > 0.0)
                duration_hist[idx].push_back((float)record.duration_seconds / 60.0);

            if (lastTimestamp > 0)
                interval_hist[idx].push_back(((float)(record.timestamp - lastTimestamp)) / 3600.0);

            lastTimestamp = record.timestamp;
        }
        idx++;
    }
    
    int displayW = _display->width();
    int displayH = _display->height();

    // --- Draw Histograms ---
    if (status.api_type == PETKIT)
    {
        Histogram histInterval(_display, 0, displayH * 3 / 4, displayW * 3 / 8, displayH / 4);
        histInterval.setTitle("Interval (Hours)");
        histInterval.setBinCount(12);
        histInterval.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histInterval.addSeries(pets[i].name.c_str(), interval_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histInterval.plot();

        Histogram histDuration(_display, displayW * 3 / 8, displayH * 3 / 4, displayW * 3 / 8, displayH / 4);
        histDuration.setTitle("Duration (Minutes)");
        histDuration.setBinCount(12);
        histDuration.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histDuration.addSeries(pets[i].name.c_str(), duration_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histDuration.plot();
    }
    else       //whisker
    {
        Histogram histInterval(_display, 0, displayH * 3 / 4, displayW * 3 / 4, displayH / 4);
        histInterval.setTitle("Interval (Hours)");
        if (pets.size() == 1)
            histInterval.setBinCount(32);
        else
            histInterval.setBinCount(24);
        histInterval.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histInterval.addSeries(pets[i].name.c_str(), interval_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histInterval.plot();
    }

    // --- Draw ScatterPlot ---
    ScatterPlot plot(_display, 0, 0, Config::EPD_WIDTH_PX, Config::EPD_HEIGHT_PX * 3 / 4);
    char title[64];
    sprintf(title, "Weight (lb) - %s", range.name);
    plot.setLabels(title, "Date", "Weight(lb)");

    int xticks = (range.type == LAST_7_DAYS) ? 10 : 18; 

    for (int i = 0; i < numPets; ++i)
    {
        plot.addSeries(pets[i].name.c_str(), pet_scatterplot[i], _petColors[i % 4].color, _petColors[i % 4].background, xticks, 10);
    }
    plot.draw();

    // --- Status Bar ---
    // Draw Battery
    int mv = analogReadMilliVolts(Config::Pins::BATTERY_ADC);
    float battery_voltage = (mv / 1000.0) * 2;
    int16_t x = 0, y = 0, x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;

    const float full_voltage = 4.20, empty_voltage = 3.20;
    if (battery_voltage >= full_voltage)
    {
        battery_voltage = full_voltage;
    }
    if (battery_voltage < empty_voltage)
        battery_voltage = empty_voltage;
    
    int battery_percent = (int)(100.0 * (battery_voltage - empty_voltage) / (full_voltage - empty_voltage));

    LinearGauge *batteryGauge;
    
    uint16_t batteryColor = EPD_BLACK;
#if (EPD_SELECT == 1002)
    if (battery_percent > 80) batteryColor = EPD_GREEN;
    else if (battery_percent > 20) batteryColor = EPD_YELLOW;
    else batteryColor = EPD_RED;
#endif

    batteryGauge = new LinearGauge(_display, displayW - Layout::BATTERY_X_OFFSET, 2, Layout::BATTERY_W, Layout::BATTERY_H, batteryColor, EPD_WHITE);

    batteryGauge->setRange(0, 100, "% ");
    batteryGauge->showLabel(true, " ");
    batteryGauge->draw(battery_percent);
    
    // Battery Icon
    int battRight = Config::EPD_WIDTH_PX - 15;
    _display->drawLine(battRight - 1, 7, battRight - 1, 18, EPD_BLACK); 
    _display->drawLine(battRight, 7, battRight, 18, EPD_BLACK);
    _display->drawLine(battRight + 1, 7, battRight + 1, 18, EPD_BLACK);
    // Tip
    _display->drawLine(battRight, 9, battRight, 16, EPD_WHITE);
    _display->drawLine(battRight - 1, 9, battRight - 1, 16, EPD_WHITE);
    _display->drawLine(battRight - 2, 9, battRight - 2, 16, EPD_WHITE);

    // Draw Update Time
    struct tm timeinfo;
    char strftime_buf[64]; 
    time(&now);                   
    localtime_r(&now, &timeinfo); 
    strftime(strftime_buf, sizeof(strftime_buf), "%m/%d/%y %H:%M", &timeinfo);
    
    _display->setFont(&FreeMono9pt7b);
    _display->setTextSize(0);
    _display->getTextBounds(strftime_buf, 0, 0, &x1, &y1, &w, &h);
    
    int timeX = Config::EPD_WIDTH_PX - 15 - w;
    _display->setTextColor(EPD_BLACK); 
    _display->setCursor(29, h / 2 + 12); // Re-using old magic number "29" for now (left padding?)
    _display->print(strftime_buf);

    if (status.litter_level_percent > 0)
    {
        if (status.api_type == PETKIT)
        {
            LinearGauge *litterGauge;
            // Calculations derived from original code
            int litterGauge_x = (displayW * 3/4) + 10;
            int litterGauge_y = (displayH * 3/4) + Layout::PADDING_LARGE; 
            int litterGauge_w = displayW - litterGauge_x - Layout::PADDING_MEDIUM;
            // Height calculation: (1/4 vertical - 20 - 15 - 15) / 2
            int panelH = displayH/4;
            int availableH = panelH - Layout::PADDING_LARGE - Layout::PADDING_MEDIUM - Layout::PADDING_MEDIUM;
            int litterGauge_h = availableH / 2;

            uint16_t gaugeColor = EPD_BLACK;
#if (EPD_SELECT == 1002)
            if (status.litter_level_percent > 90) gaugeColor = EPD_GREEN;
            else if (status.litter_level_percent > 80) gaugeColor = EPD_YELLOW;
            else gaugeColor = EPD_RED;
#endif
            litterGauge = new LinearGauge(_display, litterGauge_x , litterGauge_y, litterGauge_w, litterGauge_h, gaugeColor, EPD_WHITE);

            litterGauge->setRange(0, 100, "%");
            litterGauge->showLabel(true, "Litter: ");
            litterGauge->draw(status.litter_level_percent);
            
            // Status Box
            int16_t statusBox_x = litterGauge_x;
            int16_t statusBox_y = litterGauge_y + litterGauge_h + Layout::PADDING_MEDIUM;
            int16_t statusBox_w = litterGauge_w;
            int16_t statusBox_h = litterGauge_h;
            
            _display->drawRect(statusBox_x, statusBox_y, statusBox_w, statusBox_h, EPD_BLACK );
            
            String statusString;
            uint16_t boxColor = EPD_WHITE;
            uint16_t textColor = EPD_BLACK;

            if(status.is_drawer_full == true)
            {
                boxColor = (EPD_SELECT == 1002) ? EPD_RED : EPD_BLACK;
                textColor = EPD_WHITE;
                statusString = "Box FULL";
            }
            else if(status.litter_level_percent < 60)
            {
                boxColor = (EPD_SELECT == 1002) ? EPD_RED : EPD_BLACK;
                textColor = EPD_WHITE;
                statusString = "Litter LOW";
            }
            else
            {
                boxColor = (EPD_SELECT == 1002) ? EPD_GREEN : EPD_WHITE;
                textColor = (EPD_SELECT == 1002) ? EPD_WHITE : EPD_BLACK; // Logic preserved
                statusString = "Box OK";
            }
             
            if (boxColor != EPD_WHITE) {
                 _display->fillRect(statusBox_x+2, statusBox_y+2, statusBox_w-4, statusBox_h-4, boxColor);
            }

            _display->setFont(&FreeMonoBold9pt7b);
            _display->setTextSize(1);
            _display->setTextColor(textColor);
            
            int16_t tx, ty; uint16_t tw, th;
            _display->getTextBounds(statusString, 0, 0, &tx, &ty, &tw, &th);
            _display->setCursor(statusBox_x + (statusBox_w - tw)/2, statusBox_y + (statusBox_h - th)/2 + th/2 + 2); // Approximate vertical center
            _display->print(statusString);
        }
        else // whisker
        {
            LinearGauge *litterGauge;
            LinearGauge *wasteGauge;
            
            int gaugesX = displayW * 3 / 4 + 5;
            int offsetBase = displayH / 4 - 20 - 30; // Original logic
            int gaugeH = offsetBase / 2;
            int gaugeW = displayW / 4 - 20;

            int litterY = displayH * 3 / 4 + 20;
            int wasteY = displayH - 15 - gaugeH;

            uint16_t litterColor = EPD_BLACK;
            uint16_t wasteColor = EPD_BLACK;

#if (EPD_SELECT == 1002)
            if (status.litter_level_percent > 70.0) litterColor = EPD_GREEN;
            else if (status.litter_level_percent > 30.0) litterColor = EPD_YELLOW;
            else litterColor = EPD_RED;

            if (status.waste_level_percent < 30.0) wasteColor = EPD_GREEN;
            else if (status.waste_level_percent < 70.0) wasteColor = EPD_YELLOW;
            else wasteColor = EPD_RED;
#endif
            litterGauge = new LinearGauge(_display, gaugesX, litterY, gaugeW, gaugeH, litterColor, EPD_WHITE);
            wasteGauge = new LinearGauge(_display, gaugesX, wasteY, gaugeW, gaugeH, wasteColor, EPD_WHITE);

            litterGauge->setRange(0, 100, "%");
            litterGauge->showLabel(true, "Litter: ");
            litterGauge->draw(status.litter_level_percent);

            wasteGauge->setRange(0, 100, "%");
            wasteGauge->showLabel(true, "Waste: ");
            wasteGauge->draw(status.waste_level_percent);
        }
    }
}