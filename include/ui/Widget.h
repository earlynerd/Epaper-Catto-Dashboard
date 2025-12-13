#ifndef WIDGET_H
#define WIDGET_H

#include <Arduino.h>
#include <Adafruit_GFX.h>

// Base class for all widgets
class Widget {
public:
    Widget(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg);
    virtual void draw(float value) = 0; // Pure virtual

protected:
    Adafruit_GFX* _gfx;
    int16_t _x, _y, _w, _h;
    uint16_t _cFg, _cBg;
    String _units;
};

// ---------------------------------------------------------
// Linear Gauge: A horizontal progress bar with a frame
// ---------------------------------------------------------
class LinearGauge : public Widget {
public:
    LinearGauge(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg);
    
    void setRange(float minVal, float maxVal, String units);
    void showLabel(bool show, String label);
    void draw(float value) override;

private:
    float _min, _max;
    bool _showLabel;
    String _label;
};

// ---------------------------------------------------------
// Ring Gauge: A circular donut chart
// ---------------------------------------------------------
class RingGauge : public Widget {
public:
    RingGauge(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t radius, int16_t thickness, uint16_t colorFg, uint16_t colorBg);
    
    void setRange(float minVal, float maxVal, String units);
    void setAngleRange(int16_t startAngle, int16_t endAngle); 
    void draw(float value) override;

private:
    int16_t _radius, _thickness;
    float _min, _max;
    int16_t _startAngle, _endAngle;

    // Helper to draw filled arcs using triangles
    void fillArc(int16_t x, int16_t y, int16_t start_angle, int16_t end_angle, int16_t r_outer, int16_t r_inner, uint16_t color);
};

// ---------------------------------------------------------
// Sparkline: A historical line graph (Fixed buffer size)
// ---------------------------------------------------------
#define SPARKLINE_BUFFER_SIZE 20

class Sparkline : public Widget {
public:
    Sparkline(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg);
    
    void setRange(float minVal, float maxVal);
    void push(float value); // Add new data point
    void draw(float currentVal) override; // currentVal argument is ignored here, uses history

private:
    float _data[SPARKLINE_BUFFER_SIZE];
    uint8_t _head;
    float _min, _max;
    bool _autoScale;

};

// ---------------------------------------------------------
// Battery Gauge: LinearGauge with a generic "+" tip
// ---------------------------------------------------------
class BatteryGauge : public LinearGauge {
public:
    BatteryGauge(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg);
    void draw(float value) override;
};

#endif