#ifndef LAYOUT_TYPES_H
#define LAYOUT_TYPES_H

#include <Arduino.h>

struct WidgetConfig {
    String type;        // "ScatterPlot", "Histogram", "LinearGauge"
    int x, y, w, h, p1, p2;
    String title;       // Optional title
    String dataSource;  // "scatter", "interval", "duration", "battery", "litter", "waste"
    // Optional extras
    int min = 0;
    int max = 100;
    String unit;
    uint16_t color;    
    WidgetConfig() : x(0), y(0), w(0), h(0), p1(0), p2(0), min(0), max(100) {}
    WidgetConfig(String t, int _x, int _y, int _w, int _h, int _p1, int _p2, String _title, String _ds, int _min, int _max, String _unit, uint16_t _color)
        : type(t), x(_x), y(_y), w(_w), h(_h), p1(_p1), p2(_p2), title(_title), dataSource(_ds), min(_min), max(_max), unit(_unit), color(_color) {}
};

#endif // LAYOUT_TYPES_H
