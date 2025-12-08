#include "ui/StatusBox.h"
#include <Fonts/FreeMonoBold9pt7b.h>

StatusBox::StatusBox(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h)
    : Widget(gfx, x, y, w, h, EPD_BLACK, EPD_WHITE)
{
}

void StatusBox::draw(float value) {}

void StatusBox::draw(const SL_Status& status)
{
    // Frame
    _gfx->drawRect(_x, _y, _w, _h, EPD_BLACK);

    String statusString;
    uint16_t boxColor = EPD_WHITE;
    uint16_t textColor = EPD_BLACK;

    if(status.is_drawer_full == true)
    {
        boxColor = (EPD_SELECT == 1002) ? EPD_RED : EPD_BLACK;
        textColor = EPD_WHITE;
        statusString = "Box FULL";
    }
    else if(status.litter_level_percent < 60)
    {
        boxColor = (EPD_SELECT == 1002) ? EPD_RED : EPD_BLACK;
        textColor = EPD_WHITE;
        statusString = "Litter LOW";
    }
    else
    {
        boxColor = (EPD_SELECT == 1002) ? EPD_GREEN : EPD_WHITE;
        textColor = (EPD_SELECT == 1002) ? EPD_WHITE : EPD_BLACK;
        statusString = "Box OK";
    }
     
    if (boxColor != EPD_WHITE) {
         _gfx->fillRect(_x+2, _y+2, _w-4, _h-4, boxColor);
    }

    _gfx->setFont(&FreeMonoBold9pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(textColor);
    
    int16_t tx, ty; uint16_t tw, th;
    _gfx->getTextBounds(statusString, 0, 0, &tx, &ty, &tw, &th);
    
    // Center text
    _gfx->setCursor(_x + (_w - tw)/2, _y + (_h - th)/2 + th/2 + 2); 
    _gfx->print(statusString);
}
