#include "PlotManager.h"
#include "Fonts/FreeMono9pt7b.h"
PlotManager::PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp, DataManager* datamanager)
    : _display(disp), _dataManager(datamanager) {}

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

    // --- Draw Histograms ---
    if (status.api_type == PETKIT)
    {
        Histogram histInterval(_display, 0, _display->height() * 3 / 4, _display->width() / 2, _display->height() / 4);
        histInterval.setTitle("Interval (Hours)");
        histInterval.setBinCount(18);
        histInterval.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histInterval.addSeries(pets[i].name.c_str(), interval_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
            // histDuration.addSeries(pets[i].name.c_str(), duration_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histInterval.plot();

        Histogram histDuration(_display, _display->width() / 2, _display->height() * 3 / 4, _display->width() / 2, _display->height() / 4);
        histDuration.setTitle("Duration (Minutes)");
        histDuration.setBinCount(18);
        histDuration.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histDuration.addSeries(pets[i].name.c_str(), duration_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histDuration.plot();
    }
    else
    {
        Histogram histInterval(_display, 0, _display->height() * 3 / 4, _display->width() *2 /3, _display->height() / 4);
        histInterval.setTitle("Interval (Hours)");
        if (pets.size() == 1)
            histInterval.setBinCount(32);
        else
            histInterval.setBinCount(24);
        histInterval.setNormalization(true);
        for (int i = 0; i < numPets; ++i)
        {
            histInterval.addSeries(pets[i].name.c_str(), interval_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
            // histDuration.addSeries(pets[i].name.c_str(), duration_hist[i], _petColors[i % 4].color, _petColors[i % 4].background);
        }
        histInterval.plot();
    }

    // --- Draw ScatterPlot ---
    ScatterPlot plot(_display, 0, 0, EPD_WIDTH, EPD_HEIGHT * 3 / 4);
    char title[64];
    sprintf(title, "Weight (lb) - %s", range.name);
    plot.setLabels(title, "Date", "Weight(lb)");

    int xticks = (range.type == LAST_7_DAYS) ? 10 : 18; // Simplified logic

    for (int i = 0; i < numPets; ++i)
    {
        plot.addSeries(pets[i].name.c_str(), pet_scatterplot[i], _petColors[i % 4].color, _petColors[i % 4].background, xticks, 10);
    }
    plot.draw();

    // --- Status Bar ---
    // Draw Battery
    int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
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
    char buffer[32];
    int battery_percent = (int)(100.0 * (battery_voltage - empty_voltage) / (full_voltage - empty_voltage));

    LinearGauge *batteryGauge;
#if (EPD_SELECT == 1001)
    batteryGauge = new LinearGauge(_display, _display->width() - 15 - 60, 2, 59, 22, EPD_BLACK, EPD_WHITE);
#elif (EPD_SELECT == 1002)
    if (battery_percent > 80)
    {
        batteryGauge = new LinearGauge(_display, _display->width() - 15 - 60, 2, 59, 22, EPD_GREEN, EPD_WHITE);
    }
    else if ((battery_percent <= 60) && (battery_percent > 20))
    {
        batteryGauge = new LinearGauge(_display, _display->width() - 15 - 60, 2, 59, 22, EPD_YELLOW, EPD_WHITE);
    }
    else
        batteryGauge = new LinearGauge(_display, _display->width() - 15 - 60, 2, 59, 22, EPD_RED, EPD_WHITE);
#endif
    batteryGauge->setRange(0, 100, "% ");
    batteryGauge->showLabel(true, " ");
    batteryGauge->draw(battery_percent);
    _display->drawLine(EPD_WIDTH - 16, 7, EPD_WIDTH - 16, 18, EPD_BLACK); // make it look like a battery
    _display->drawLine(EPD_WIDTH - 15, 7, EPD_WIDTH - 15, 18, EPD_BLACK);
    _display->drawLine(EPD_WIDTH - 14, 7, EPD_WIDTH - 14, 18, EPD_BLACK);
    _display->drawLine(EPD_WIDTH - 15, 9, EPD_WIDTH - 15, 16, EPD_WHITE);
    _display->drawLine(EPD_WIDTH - 16, 9, EPD_WIDTH - 16, 16, EPD_WHITE);
    _display->drawLine(EPD_WIDTH - 17, 9, EPD_WIDTH - 17, 16, EPD_WHITE);

    // Draw Update Time
    struct tm timeinfo;
    char strftime_buf[64]; // Buffer to hold the formatted string

    x = 0; y = 0;
    x1 = 0; y1 = 0;
    w = 0; h = 0;

    time(&now);                   // Get current epoch time
    localtime_r(&now, &timeinfo); // Convert to struct tm
    strftime(strftime_buf, sizeof(strftime_buf), "%m/%d/%y %H:%M", &timeinfo);
    _display->setFont(&FreeMono9pt7b);
    _display->setTextSize(0);
    _display->getTextBounds(strftime_buf, x, y, &x1, &y1, &w, &h);
    x = EPD_WIDTH - 15 - w;
    _display->setFont(&FreeMono9pt7b); // Use the provided font
    _display->setTextSize(0);
    _display->setTextColor(EPD_BLACK); // Use the provided color
    _display->setCursor(29, h / 2 + 12);
    _display->print(strftime_buf);

    if (status.litter_level_percent > 0)
    {

        if (status.api_type == PETKIT)
        {
            LinearGauge *litterGauge;
#if (EPD_SELECT == 1001)
            litterGauge = new LinearGauge(_display, _display->width() - 15 - 40 - 160, 2, 120, 22, EPD_BLACK, EPD_WHITE);
#else
            if (status.litter_level_percent > 90)
                litterGauge = new LinearGauge(_display, _display->width() - 15 - 40 - 160, 2, 120, 22, EPD_GREEN, EPD_WHITE);
            else if (status.litter_level_percent > 80)
                litterGauge = new LinearGauge(_display, _display->width() - 15 - 40 - 160, 2, 120, 22, EPD_YELLOW, EPD_WHITE);
            else
                litterGauge = new LinearGauge(_display, _display->width() - 15 - 40 - 160, 2, 120, 22, EPD_RED, EPD_WHITE);
#endif

            litterGauge->setRange(0, 100, "%");
            litterGauge->showLabel(true, "Litter: ");
            litterGauge->draw(status.litter_level_percent);
       
        }
        else // whisker
        {
            
            
            
            LinearGauge *litterGauge;
            LinearGauge *wasteGauge;
#if (EPD_SELECT == 1002)
            if (status.litter_level_percent > 70.0)
                litterGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() * 3 / 4 + 20, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_GREEN, EPD_WHITE);
            else if (status.litter_level_percent > 30.0)
                litterGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() * 3 / 4 + 20, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_YELLOW, EPD_WHITE);
            else
                litterGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() * 3 / 4 + 20, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_RED, EPD_WHITE);

            if (status.waste_level_percent < 30.0)
                wasteGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() - 15 - (_display->height() / 4 - 20 - 30) / 2, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_GREEN, EPD_WHITE);
            else if (status.waste_level_percent < 70.0)
                wasteGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() - 15 - (_display->height() / 4 - 20 - 30) / 2, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_YELLOW, EPD_WHITE);
            else
                wasteGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() - 15 - (_display->height() / 4 - 20 - 30) / 2, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_RED, EPD_WHITE);
#else
            litterGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() * 3 / 4 + 20, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_BLACK, EPD_WHITE);
            wasteGauge = new LinearGauge(_display, _display->width() * 3 / 4 + 5, _display->height() - 15 - (_display->height() / 4 - 20 - 30) / 2, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_BLACK, EPD_WHITE);
#endif
            // LinearGauge litterGauge(_display, _display->width() * 3 / 4 + 5, _display->height() * 3 / 4 + 20, _display->width() / 4 - 20, (_display->height() / 4 - 20 - 30) / 2, EPD_BLACK, EPD_WHITE);
            litterGauge->setRange(0, 100, "%");
            litterGauge->showLabel(true, "Litter: ");
            litterGauge->draw(status.litter_level_percent);

            wasteGauge->setRange(0, 100, "%");
            wasteGauge->showLabel(true, "Waste: ");
            wasteGauge->draw(status.waste_level_percent);
            
        }
    }
}