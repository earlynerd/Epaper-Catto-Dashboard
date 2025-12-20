#include "ui/DataProcessor.h"
#include <time.h>
#include <vector>
#include <algorithm>
#include <cmath>

DashboardData DataProcessor::process(const std::vector<SL_Pet> &pets, 
                                     const PetDataMap &allPetData, 
                                     const DateRangeInfo &range,
                                     const std::vector<ColorPair>& colors)
{
    DashboardData data;
    
    time_t now = time(NULL);
    time_t timeStart = now - range.seconds;

    int idx = 0;
    for (const auto &pet : pets)
    {
        // Guard: check if we even have data for this pet
        auto it = allPetData.find(pet.id.toInt());
        if (it == allPetData.end())
        {
            idx++;
            continue;
        }

        ProcessedSeries series;
        series.name = pet.name;
        series.color = colors[idx % colors.size()].color;
        series.bgColor = colors[idx % colors.size()].background;

        time_t lastTimestamp = -1;
        const auto& petRecords = it->second;

        // Iterate through records for this pet
        for (auto const &recordPair : petRecords)
        {
            const SL_Record &record = recordPair.second;
            
            // Filter by date range
            if (record.timestamp < timeStart)
            {
                continue;
            }

            // 1. Scatter Plot Data (Weight vs Time)
            float weight_lbs = (float)record.weight_lbs;
            struct tm *thistimestamp = localtime(&record.timestamp);
            time_t ts = mktime(thistimestamp);
            series.scatterPoints.push_back({(float)ts, weight_lbs});
            //if(series.weightValues.size() > 0) series.deltaWeightValues.push_back(weight_lbs - series.weightValues.back());
            series.weightValues.push_back(weight_lbs);
            // 2. Duration Histogram Data (Duration in Minutes)
            if (record.duration_seconds > 0.0)
                series.durationValues.push_back((float)record.duration_seconds / 60.0);

            // 3. Interval Histogram Data (Hours since last visit)
            if (lastTimestamp > 0)
                series.intervalValues.push_back(((float)(record.timestamp - lastTimestamp)) / 3600.0);

            lastTimestamp = record.timestamp;
        }
        series.deltaWeightValues = getWeightChangeRates(series.scatterPoints, 30, 5);
        data.series.push_back(series);
        idx++;
    }

    return data;
}

DashboardData DataProcessor::processEnvData(const std::vector<env_data>& envData,
                                            const DateRangeInfo& range,
                                            const std::vector<ColorPair>& colors)
{
    DashboardData data;
    
    time_t now = time(NULL);
    time_t timeStart = now - range.seconds;

    // Series 1: Temperature
    ProcessedSeries tempSeries;
    tempSeries.name = "Temperature";
    // Choose a color (e.g. Red for Temp)
    tempSeries.color = colors.size() > 0 ? colors[0].color : 0; 
    tempSeries.bgColor = colors.size() > 0 ? colors[0].background : 0;

    // Series 2: Humidity
    ProcessedSeries humidSeries;
    humidSeries.name = "Humidity";
    // Choose a color (e.g. Blue for Humidity)
    humidSeries.color = colors.size() > 1 ? colors[1].color : 0;
    humidSeries.bgColor = colors.size() > 1 ? colors[1].background : 0;

    for (const auto& rec : envData)
    {
        if (rec.timestamp < timeStart) continue;

        struct tm *thistimestamp = localtime(&rec.timestamp);
        time_t ts = mktime(thistimestamp);

        tempSeries.scatterPoints.push_back({(float)ts, rec.temperature});
        humidSeries.scatterPoints.push_back({(float)ts, rec.humidity});
    }

    data.series.push_back(tempSeries);
    data.series.push_back(humidSeries);

    return data;
}

std::vector<float> DataProcessor::getWeightChangeRates(std::vector<DataPoint> scatterPoints, int intervalDays, int smoothingWindow) {
    if (scatterPoints.size() < 2) return {};

    // 1. Ensure chronological order
    std::sort(scatterPoints.begin(), scatterPoints.end(), [](const DataPoint& a, const DataPoint& b) {
        return a.x < b.x;
    });

    // 2. Simple Centered Smoothing
    // Even with cleaned data, various sources of noise cause "jitter". Smoothing 3-5 points 
    // helps find the "true" weight 
    std::vector<float> smoothedWeights;
    int n = scatterPoints.size();
    int radius = smoothingWindow / 2;

    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int j = i - radius; j <= i + radius; ++j) {
            if (j >= 0 && j < n) {
                sum += scatterPoints[j].y;
                count++;
            }
        }
        smoothedWeights.push_back(sum / count);
    }

    // 3. Calculate Rate of Change Normalized to intervalDays
    std::vector<float> dailyRates;
    const double SECONDS_IN_DAY = 86400.0;

    for (size_t i = 1; i < smoothedWeights.size(); ++i) {
        float weightDiff = smoothedWeights[i] - smoothedWeights[i - 1];
        double timeDiff = scatterPoints[i].x - scatterPoints[i - 1].x;

        // If the box records multiple times in one hour, the "rate" can be 
        // extremely volatile. We look for meaningful time gaps (e.g., > 1 hour).
        if (timeDiff > 3600.0) { 
            // Result is: (Change in Weight / Seconds) * Seconds in a Day
            float ratePerDay = (float)((weightDiff / timeDiff) * SECONDS_IN_DAY);
            float ratePerInterval = intervalDays * ratePerDay;
            dailyRates.push_back(ratePerInterval);
        }
    }

    return dailyRates;
}