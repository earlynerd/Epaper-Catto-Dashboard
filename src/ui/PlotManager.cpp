#include "ui/PlotManager.h"
#include "ui/DataProcessor.h"
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

    // 1. Process Data
    DashboardData data = DataProcessor::process(pets, allPetData, range, _petColors);
    
    // 2. Load Layout
    std::vector<WidgetConfig> layout = _dataManager->loadLayout();

    // 3. Render Widgets
    for (const auto& w : layout)
    {
        if (w.type == "ScatterPlot")
        {
            ScatterPlot plot(_display, w.x, w.y, w.w, w.h);
            char titleBuf[64];
            // Format title with date range if requested, otherwise just use config title
            if (w.title.indexOf("%s") >= 0) snprintf(titleBuf, sizeof(titleBuf), w.title.c_str(), range.name);
            else strncpy(titleBuf, w.title.c_str(), sizeof(titleBuf));
            
            plot.setLabels(titleBuf, "Date", "Weight"); // Axis labels could be in config too technically
            int xticks = (range.type == LAST_7_DAYS) ? 10 : 18;

            // Add all processed series to the plot
            for (const auto& series : data.series)
            {
                plot.addSeries(series.name.c_str(), series.scatterPoints, series.color, series.bgColor, xticks, 10);
            }
            plot.draw();
        }
        else if (w.type == "Histogram")
        {
            Histogram hist(_display, w.x, w.y, w.w, w.h);
            hist.setTitle(w.title.c_str());
            hist.setNormalization(true);
            
            // Apply bin count logic (could be in config)
            if (pets.size() == 1) hist.setBinCount(32);
            else hist.setBinCount(12);

            for (const auto& series : data.series)
            {
                if (w.dataSource == "interval")
                    hist.addSeries(series.name.c_str(), series.intervalValues, series.color, series.bgColor);
                else if (w.dataSource == "duration")
                    hist.addSeries(series.name.c_str(), series.durationValues, series.color, series.bgColor);
            }
            hist.plot();
        }
        else if (w.type == "LinearGauge")
        {
            float val = 0;
            String unit = "%";
            uint16_t color = EPD_BLACK;
            
            if (w.dataSource == "battery")
            {
                 int mv = analogReadMilliVolts(Config::Pins::BATTERY_ADC);
                 float v = (mv / 1000.0) * 2;
                 // Simple percentage calc
                 val = (v - 3.20) / (4.20 - 3.20) * 100.0;
                 if (val > 100) val = 100;
                 if (val < 0) val = 0;
                 
                 #if (EPD_SELECT == 1002)
                     if (val > 80) color = EPD_GREEN;
                     else if (val > 20) color = EPD_YELLOW;
                     else color = EPD_RED;
                 #endif
            }
            else if (w.dataSource == "litter")
            {
                val = status.litter_level_percent;
                 #if (EPD_SELECT == 1002)
                     if (val > 90) color = EPD_GREEN;
                     else if (val > 80) color = EPD_YELLOW;
                     else color = EPD_RED;
                 #endif
            }
            else if (w.dataSource == "waste")
            {
                val = status.waste_level_percent;
            }

            LinearGauge *gauge = nullptr;
            
            // Should properly deallocate if doing heavy creating? 
            // Currently PlotManager is destroyed/recreated regularly or not?
            // "Stack" allocation or smart pointers would be better but using new/delete logic or local obj logic.
            // Oh, we are in a loop in renderDashboard. "LinearGauge gauge" creates it on stack and destroys at loop end.
            // The original code used "LinearGauge gauge(...)". 
            // But we need polymorphism for BatteryGauge.
            
            if (w.dataSource == "battery")
            {
                 // Create BatteryGauge
                 BatteryGauge bg(_display, w.x, w.y, w.w, w.h, color, EPD_WHITE);
                 bg.setRange(w.min, w.max, unit);
                 bg.showLabel(true, w.title);
                 bg.draw(val);
            }
            else
            {
                 // Create Standard LinearGauge
                 LinearGauge lg(_display, w.x, w.y, w.w, w.h, color, EPD_WHITE);
                 lg.setRange(w.min, w.max, unit);
                 lg.showLabel(true, w.title);
                 lg.draw(val);
            }
        }
        else if (w.type == "TextLabel")
        {
             TextLabel label(_display, w.x, w.y, w.w, w.h, EPD_BLACK, EPD_WHITE);
             label.setFormat(w.title.length() > 0 ? w.title : "%m/%d %H:%M");
             
             if (w.dataSource == "datetime")
             {
                 time_t now;
                 time(&now);
                 label.draw(now);
             }
             
        }
        else if (w.type == "StatusBox")
        {
            StatusBox box(_display, w.x, w.y, w.w, w.h);
            box.draw(status);
        }
    }


    // --- Status Bar (Overlays) ---
    
    // Battery Icon Tip (Aesthetic overlay) - Can keep this or make it part of LinearGauge someday.
    // Converting to primitive drawing in layout would be overkill for 3 lines.
    // Keeping it hardcoded for now as it's just decoration for the specific battery gauge style.
    int battRight = Config::EPD_WIDTH_PX - 15;
    _display->drawLine(battRight - 1, 7, battRight - 1, 18, EPD_BLACK); 
    _display->drawLine(battRight, 7, battRight, 18, EPD_BLACK);
    _display->drawLine(battRight + 1, 7, battRight + 1, 18, EPD_BLACK);
    _display->drawLine(battRight, 9, battRight, 16, EPD_WHITE);
    _display->drawLine(battRight - 1, 9, battRight - 1, 16, EPD_WHITE);
    _display->drawLine(battRight - 2, 9, battRight - 2, 16, EPD_WHITE);
}