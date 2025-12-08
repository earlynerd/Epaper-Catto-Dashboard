#ifndef STATUS_BOX_H
#define STATUS_BOX_H

#include "ui/Widget.h"
#include "core/SharedTypes.h"
#include "core/Config.h" // For EPD_SELECT

class StatusBox : public Widget {
public:
    StatusBox(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h);
    void draw(const SL_Status& status);
    void draw(float value) override; // unused

private:
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
};

#endif
