#ifndef TEXT_LABEL_H
#define TEXT_LABEL_H

#include "ui/Widget.h"
#include <time.h>

class TextLabel : public Widget {
public:
    TextLabel(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg);
    
    // Set format string (e.g., "%m/%d %H:%M") or static text
    void setFormat(String fmt);
    
    // Draw current time (if dataSource="datetime") or static text
    void draw(float value) override; // value ignored
    void draw(time_t now);

private:
    String _format;
};

#endif
