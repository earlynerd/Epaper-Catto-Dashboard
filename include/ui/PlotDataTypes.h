#ifndef PLOT_DATA_TYPES_H
#define PLOT_DATA_TYPES_H

#include <Arduino.h>
#include <vector>
#include "ui/ScatterPlot.h" // For DataPoint

struct ProcessedSeries {
    String name;
    uint16_t color;
    uint16_t bgColor;
    std::vector<DataPoint> scatterPoints; // For ScatterPlot
    std::vector<float> intervalValues;    // For Interval Histogram
    std::vector<float> durationValues;    // For Duration Histogram
};

struct DashboardData {
    std::vector<ProcessedSeries> series;
    // Add other aggregate stats here if needed later
};

struct ColorPair {
    uint16_t color;
    uint16_t background;
};

#endif // PLOT_DATA_TYPES_H
