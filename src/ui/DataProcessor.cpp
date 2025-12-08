#include "ui/DataProcessor.h"
#include <time.h>

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
                continue;

            // 1. Scatter Plot Data (Weight vs Time)
            float weight_lbs = (float)record.weight_lbs;
            struct tm *thistimestamp = localtime(&record.timestamp);
            time_t ts = mktime(thistimestamp);
            series.scatterPoints.push_back({(float)ts, weight_lbs});

            // 2. Duration Histogram Data (Duration in Minutes)
            if (record.duration_seconds > 0.0)
                series.durationValues.push_back((float)record.duration_seconds / 60.0);

            // 3. Interval Histogram Data (Hours since last visit)
            if (lastTimestamp > 0)
                series.intervalValues.push_back(((float)(record.timestamp - lastTimestamp)) / 3600.0);

            lastTimestamp = record.timestamp;
        }

        data.series.push_back(series);
        idx++;
    }

    return data;
}
