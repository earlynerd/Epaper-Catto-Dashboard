#ifndef LAYOUT_TYPES_H
#define LAYOUT_TYPES_H

#include <Arduino.h>

struct WidgetConfig {
    String type;        // "ScatterPlot", "Histogram", "LinearGauge"
    int x, y, w, h;
    String title;       // Optional title
    String dataSource;  // "scatter", "interval", "duration", "battery", "litter", "waste"
    // Optional extras
    int min = 0;
    int max = 100;

    WidgetConfig() : x(0), y(0), w(0), h(0), min(0), max(100) {}
    WidgetConfig(String t, int _x, int _y, int _w, int _h, String _title, String _ds, int _min, int _max)
        : type(t), x(_x), y(_y), w(_w), h(_h), title(_title), dataSource(_ds), min(_min), max(_max) {}
};

#endif // LAYOUT_TYPES_H
