#include <Arduino.h>
#include "Dashboard.h"
#include <math.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "config.h"

// --- Base Widget ---
Widget::Widget(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg)
    : _gfx(gfx), _x(x), _y(y), _w(w), _h(h), _cFg(colorFg), _cBg(colorBg) {}

// --- Linear Gauge ---
LinearGauge::LinearGauge(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg)
    : Widget(gfx, x, y, w, h, colorFg, colorBg), _showLabel(false)
{
    _min = 0;
    _max = 100;
}

void LinearGauge::setRange(float minVal, float maxVal, String units)
{
    _min = minVal;
    _max = maxVal;
    _units = units;
}

void LinearGauge::showLabel(bool show, String label)
{
    _showLabel = show;
    _label = label;
}

void LinearGauge::draw(float value) {
    // Clamp value
    if (value < _min) value = _min;
    if (value > _max) value = _max;

    // 1. Clear background
    _gfx->fillRect(_x, _y, _w, _h, _cBg);

    // 2. Draw Outline
    _gfx->drawRect(_x, _y, _w, _h, EPD_BLACK);

    bool isVertical = _h > _w;
    // 3. Calculate Fill Width
    // We leave a 2px padding inside the border
    int16_t activeW = _w - 4;
    int16_t activeH = _h - 4;

    int16_t barW = (int16_t)((value - _min) * activeW / (_max - _min));
    int16_t barH = (int16_t)((value - _min) * activeH / (_max - _min));
    // 4. Draw the Bar
    if ((barW > 0) && (!isVertical)) {
        _gfx->fillRect(_x + 2, _y + 2, barW, _h - 4, _cFg);
    }
    if((barH > 0) && (isVertical)) {
        _gfx->fillRect(_x + 2, _y + 2 + activeH - barH, _w - 4, barH, _cFg);
    }

    // 5. Inverted Text Label (Pixel-by-Pixel Analysis)
    if (_showLabel) {
        String valStr = String(value, 1);
        valStr = _label + valStr + _units;
        int16_t x1, y1;
        uint16_t w, h;
        _gfx->setFont(&FreeSansBold9pt7b);
        _gfx->setTextSize(1);
        _gfx->getTextBounds(valStr, 0, 0, &x1, &y1, &w, &h);
        
        // Calculate where the text *should* go on the screen (centered)
        int16_t textScreenX = _x + (_w - w) / 2;
        int16_t textScreenY = _y + (_h - h) / 2;

        // Create a temporary 1-bit canvas just for the text
        // This acts as a bitmap mask
        GFXcanvas1 textCanvas(w, h);
        textCanvas.setFont(&FreeSansBold9pt7b);
        textCanvas.setTextSize(1);
        textCanvas.setTextColor(1); // "On" pixels
        // Adjust cursor so the text fills the canvas tightly (negating the bounds offset)
        textCanvas.setCursor(-x1, -y1); 
        textCanvas.print(valStr);

        // Define the X-limit of the filled bar
        int16_t barLimitX = _x + 2 + barW;

        // Iterate through the text canvas pixels
        for (int16_t j = 0; j < h; j++) {
            for (int16_t i = 0; i < w; i++) {
                // If this pixel is part of the text
                if (textCanvas.getPixel(i, j)) {
                    int16_t absX = textScreenX + i;
                    int16_t absY = textScreenY + j;

                    // INVERSION LOGIC:
                    // Check if this specific pixel lies on top of the filled bar.
                    // If yes -> Draw in Background Color (White)
                    // If no  -> Draw in Foreground Color (Black)
                    
                    bool overBar = (absX >= _x + 2) && (absX < barLimitX);
                    
                    uint16_t finalColor = overBar ? _cBg : _cFg;
                    _gfx->drawPixel(absX, absY, finalColor);
                }
            }
        }
    }
}

// --- Ring Gauge ---
RingGauge::RingGauge(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t radius, int16_t thickness, uint16_t colorFg, uint16_t colorBg)
    : Widget(gfx, x, y, radius * 2, radius * 2, colorFg, colorBg),
      _radius(radius), _thickness(thickness),
      _startAngle(135), _endAngle(405) // Standard "Speedometer" layout (270 degrees total)
{
    _min = 0;
    _max = 100;
}

void RingGauge::setRange(float minVal, float maxVal)
{
    _min = minVal;
    _max = maxVal;
}

void RingGauge::setAngleRange(int16_t startAngle, int16_t endAngle)
{
    _startAngle = startAngle;
    _endAngle = endAngle;
}

// Custom Helper to draw thick arc without native GFX support
void RingGauge::fillArc(int16_t cx, int16_t cy, int16_t start_angle, int16_t end_angle, int16_t r_outer, int16_t r_inner, uint16_t color)
{
    float angle_step = 6.0; // Resolution in degrees (higher = smoother but slower)

    // Convert 0-360 standard to GFX standard (0 is Right, 90 is Bottom in standard math,
    // but usually 0 is Top in UI thinking. Let's stick to standard Trig: 0=Right, -90=Top)
    // To make it intuitive: Input 0 = Top, 90 = Right.
    // Math adjustment: theta = (angle - 90)

    for (float i = start_angle; i < end_angle; i += angle_step)
    {
        float a1 = (i - 90) * DEG_TO_RAD;
        float a2 = ((i + angle_step) - 90) * DEG_TO_RAD;

        // Ensure we don't over-shoot the end angle
        if (i + angle_step > end_angle)
        {
            a2 = (end_angle - 90) * DEG_TO_RAD;
        }

        // Outer points
        int16_t x1_out = cx + cos(a1) * r_outer;
        int16_t y1_out = cy + sin(a1) * r_outer;
        int16_t x2_out = cx + cos(a2) * r_outer;
        int16_t y2_out = cy + sin(a2) * r_outer;

        // Inner points
        int16_t x1_in = cx + cos(a1) * r_inner;
        int16_t y1_in = cy + sin(a1) * r_inner;
        int16_t x2_in = cx + cos(a2) * r_inner;
        int16_t y2_in = cy + sin(a2) * r_inner;

        // Draw two triangles to form the quad segment
        _gfx->fillTriangle(x1_out, y1_out, x1_in, y1_in, x2_out, y2_out, color);
        _gfx->fillTriangle(x2_out, y2_out, x1_in, y1_in, x2_in, y2_in, color);
    }
}

void RingGauge::draw(float value)
{
    if (value < _min)
        value = _min;
    if (value > _max)
        value = _max;

    int16_t totalAngle = _endAngle - _startAngle;
    float ratio = (value - _min) / (_max - _min);
    int16_t activeEndAngle = _startAngle + (int16_t)(totalAngle * ratio);

    // 1. Draw Empty Background Ring (Grey/Dotted or just Outline)
    // For e-paper, we usually just clear the area or draw a thin guide
    // _gfx->drawCircle(_x + _radius, _y + _radius, _radius, _cFg); // Outer border
    // _gfx->drawCircle(_x + _radius, _y + _radius, _radius - _thickness, _cFg); // Inner border

    // 2. Draw Active Arc
    fillArc(_x, _y, _startAngle, activeEndAngle, _radius, _radius - _thickness, _cFg);

    // 3. Draw Value in Center
    _gfx->setFont(&FreeSansBold9pt7b);
    _gfx->setTextSize(2);
    _gfx->setTextColor(_cFg);

    // Simple centering logic
    int16_t cx = _x;
    int16_t cy = _y;

    String valStr = String((int)value);
    int16_t txtW = valStr.length() * 12; // approx width for size 2
    int16_t txtH = 14;

    _gfx->setCursor(cx - (txtW / 2), cy - (txtH / 2));
    _gfx->print(valStr);
}

// --- Sparkline ---
Sparkline::Sparkline(Adafruit_GFX *gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg)
    : Widget(gfx, x, y, w, h, colorFg, colorBg), _head(0), _autoScale(true)
{
    // Initialize buffer
    for (int i = 0; i < SPARKLINE_BUFFER_SIZE; i++)
        _data[i] = 0;
    _min = 0;
    _max = 100;
}

void Sparkline::setRange(float minVal, float maxVal)
{
    _min = minVal;
    _max = maxVal;
    _autoScale = false;
}

void Sparkline::push(float value)
{
    _data[_head] = value;
    _head = (_head + 1) % SPARKLINE_BUFFER_SIZE;
}

void Sparkline::draw(float currentVal)
{
    // Clear area
    _gfx->fillRect(_x, _y, _w, _h, _cBg);
    _gfx->drawRect(_x, _y, _w, _h, _cFg);

    // Determine scale
    float lMin = _min;
    float lMax = _max;

    if (_autoScale)
    {
        lMin = 99999;
        lMax = -99999;
        for (int i = 0; i < SPARKLINE_BUFFER_SIZE; i++)
        {
            if (_data[i] < lMin)
                lMin = _data[i];
            if (_data[i] > lMax)
                lMax = _data[i];
        }
        // Add a little breathing room
        float range = lMax - lMin;
        if (range == 0)
            range = 1;
        lMax += range * 0.1;
        lMin -= range * 0.1;
    }

    float stepX = (float)(_w - 4) / (SPARKLINE_BUFFER_SIZE - 1);
    float range = lMax - lMin;
    if (range == 0)
        range = 1;

    // Draw lines
    int16_t prevX = -1;
    int16_t prevY = -1;

    // We start from the oldest data point (tail) to newest (head)
    for (int i = 0; i < SPARKLINE_BUFFER_SIZE; i++)
    {
        int index = (_head + i) % SPARKLINE_BUFFER_SIZE;
        float val = _data[index];

        // Normalize value to height (flipping Y because 0 is top)
        int16_t pointY = _y + _h - 2 - (int16_t)((val - lMin) * (_h - 4) / range);
        int16_t pointX = _x + 2 + (int16_t)(i * stepX);

        if (i > 0)
        {
            _gfx->drawLine(prevX, prevY, pointX, pointY, _cFg);
        }
        prevX = pointX;
        prevY = pointY;
    }
}