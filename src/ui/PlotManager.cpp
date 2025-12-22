#include "ui/PlotManager.h"
#include "ui/DataProcessor.h"
#include "Fonts/FreeMono9pt7b.h"
#include "Fonts/FreeMonoBold9pt7b.h"

// Layout Constants
namespace Layout
{
    constexpr int HEADER_HEIGHT = 20; // Estimated from context

    // Histogram Layout
    constexpr int HIST_Y_OFFSET_DENOM = 4;           // 1/4 of height from bottom
    constexpr int HIST_HEIGHT_DENOM = 4;             // 1/4 of height
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

PlotManager::PlotManager(GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> *disp, DataManager *datamanager)
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
 * @param vbat the measured battery voltage for display
 */
void PlotManager::renderDashboard(const std::vector<SL_Pet> &pets, PetDataMap &allPetData, const DateRangeInfo &range, const SL_Status &status, float vbat)
{
    _display->fillScreen(GxEPD_WHITE);

    // 1. Process Data
    DashboardData data = DataProcessor::process(pets, allPetData, range, _petColors);

    // 1b. Process Env Data
    std::vector<env_data> envRecords = _dataManager->getEnvData();
    DashboardData envPlotData = DataProcessor::processEnvData(envRecords, range, _petColors);

    // 2. Load Layout
    std::vector<WidgetConfig> layout = _dataManager->loadLayout();

    // 3. Render Widgets
    for (const auto &w : layout)
    {
        if (w.type == "ScatterPlot")
        {
            ScatterPlot plot(_display, w.x, w.y, w.w, w.h);
            char titleBuf[64];
            // Format title with date range if requested, otherwise just use config title
            if (w.title.indexOf("%s") >= 0)
                snprintf(titleBuf, sizeof(titleBuf), w.title.c_str(), range.name);
            else
                strncpy(titleBuf, w.title.c_str(), sizeof(titleBuf));

            plot.setLabels(titleBuf, "Date", "Value"); // Generic Y label
            int xticks=8, yticks=8;
            if (w.p1 > 0 && w.p2 > 0)
            {
                xticks = w.p1;
                yticks = w.p2;
            }
            else
            {
                xticks = (range.type == LAST_7_DAYS) ? 10 : 18;
                if (w.w <= 400)
                    xticks = xticks / 2;
                yticks = w.h / PIXELS_PER_TICK;
            }
            if (w.dataSource == "scatter" || w.dataSource == "")
            {
                for (const auto &series : data.series)
                {
                    plot.addSeries(series.name.c_str(), series.scatterPoints, series.color, series.bgColor, xticks, yticks);
                }
            }
            else if (w.dataSource == "temperature_history")
            {
                // Add Temp from processed env data. Series 0 is Temp.
                if (envPlotData.series.size() > 0)
                {
                    const auto &s = envPlotData.series[0];
                    plot.addSeries(s.name.c_str(), s.scatterPoints, s.color, s.bgColor, xticks, 10);
                }
            }
            else if (w.dataSource == "humidity_history")
            {
                // Add Humidity from processed env data. Series 1 is Humidity.
                if (envPlotData.series.size() > 1)
                {
                    const auto &s = envPlotData.series[1];
                    plot.addSeries(s.name.c_str(), s.scatterPoints, s.color, s.bgColor, xticks, 10);
                }
            }

            plot.draw();
        }
        else if (w.type == "Histogram")
        {
            Histogram hist(_display, w.x, w.y, w.w, w.h);
            hist.setTitle(w.title.c_str());
            hist.setNormalization(true);

            if (w.p1 != 0)
                hist.setBinCount(w.p1);
            else
            {
                if ((pets.size() == 1) && (w.w >= 400))
                    hist.setBinCount(32);
                else
                    hist.setBinCount(14);
            }
            for (const auto &series : data.series)
            {
                if (w.dataSource == "interval")
                    hist.addSeries(series.name.c_str(), series.intervalValues, series.color, series.bgColor);
                else if (w.dataSource == "duration")
                    hist.addSeries(series.name.c_str(), series.durationValues, series.color, series.bgColor);
                else if (w.dataSource == "weight")
                    hist.addSeries(series.name.c_str(), series.weightValues, series.color, series.bgColor);
                else if (w.dataSource == "weight_change")
                    hist.addSeries(series.name.c_str(), series.deltaWeightValues, series.color, series.bgColor);
            }
            hist.plot();
        }
        else if (w.type == "LinearGauge")
        {
            float val = 0;

            uint16_t color = EPD_BLACK;

            if (w.dataSource == "battery")
            {
                
                // Simple percentage calc
                val = (vbat - 3.20) / (4.10 - 3.20) * 100.0;
                if (val > 100)
                    val = 100;
                if (val < 0)
                    val = 0;

#if (EPD_SELECT == 1002)
                if (val > 80)
                    color = EPD_GREEN;
                else if (val > 20)
                    color = EPD_YELLOW;
                else
                    color = EPD_RED;
#endif
            }
            else if (w.dataSource == "litter")
            {
                val = status.litter_level_percent;
#if (EPD_SELECT == 1002)
                if (val > 80)
                    color = EPD_GREEN;
                else if (val > 60)
                    color = EPD_YELLOW;
                else
                    color = EPD_RED;
#endif
            }
            else if (w.dataSource == "waste")
            {
                val = status.waste_level_percent;
#if (EPD_SELECT == 1002)
                if (val < 30)
                    color = EPD_GREEN;
                else if (val > 80)
                    color = EPD_RED;
                else
                    color = EPD_YELLOW;
#endif
            }

            LinearGauge *gauge = nullptr;

            if (w.dataSource == "battery")
            {
                // Create BatteryGauge
                BatteryGauge bg(_display, w.x, w.y, w.w, w.h, color, EPD_WHITE);
                bg.setRange(w.min, w.max, w.unit);
                bg.showLabel(true, w.title);
                bg.draw(val);
                int battRight = w.x + w.w;
                const int16_t buttonTop = w.y + w.h / 3 - 1;
                const int16_t buttonBottom = w.y + w.h - w.h / 3 + 1;
                _display->drawLine(battRight, buttonTop, battRight, buttonBottom, EPD_BLACK); // three black lines to make the button top of battery
                _display->drawLine(battRight + 1, buttonTop, battRight + 1, buttonBottom, EPD_BLACK);
                _display->drawLine(battRight + 2, buttonTop, battRight + 2, buttonBottom, EPD_BLACK);

                _display->drawLine(battRight - 1, buttonTop, battRight - 1, buttonBottom - 1, EPD_WHITE); // three white ones to erase a bit in the center
                _display->drawLine(battRight, buttonTop, battRight, buttonBottom - 1, EPD_WHITE);
                _display->drawLine(battRight + 1, buttonTop, battRight + 1, buttonBottom - 1, EPD_WHITE);
            }
            else
            {
                // Create Standard LinearGauge
                LinearGauge lg(_display, w.x, w.y, w.w, w.h, color, EPD_WHITE);
                lg.setRange(w.min, w.max, w.unit);
                lg.showLabel(true, w.title);
                lg.draw(val);
            }
        }
        else if (w.type == "RingGauge")
        {
            float val = 0;

            uint16_t color = EPD_BLACK;

            if (w.dataSource == "battery")
            {
                int mv = analogReadMilliVolts(Config::Pins::BATTERY_ADC);
                float v = (mv / 1000.0) * 2;
                // Simple percentage calc
                val = (v - 3.20) / (4.20 - 3.20) * 100.0;
                if (val > 100)
                    val = 100;
                if (val < 0)
                    val = 0;
            }
            else if (w.dataSource == "litter")
            {
                val = status.litter_level_percent;
            }
            else if (w.dataSource == "waste")
            {
                val = status.waste_level_percent;
            }

            RingGauge rg(_display, w.x, w.y, w.w, w.h, color, EPD_WHITE);
            rg.setRange(w.min, w.max, w.unit);
            rg.setAngleRange(w.p1, w.p2);
            rg.showLabel(true, w.title);
            rg.draw(val);
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
            else if (w.dataSource == "temperature" && !envRecords.empty())
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.1f C", envRecords.back().temperature);
                label.setFormat(buf);
                label.draw(envRecords.back().temperature);
            }
            else if (w.dataSource == "humidity" && !envRecords.empty())
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "%.0f%%", envRecords.back().humidity);
                label.setFormat(buf);
                label.draw(envRecords.back().humidity);
            }
        }
        else if (w.type == "StatusBox")
        {
            StatusBox box(_display, w.x, w.y, w.w, w.h);
            box.draw(status);
        }
    }
}