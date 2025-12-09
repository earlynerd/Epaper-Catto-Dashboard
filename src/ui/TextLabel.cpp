#include "ui/TextLabel.h"
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

TextLabel::TextLabel(Adafruit_GFX* gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t colorFg, uint16_t colorBg)
    : Widget(gfx, x, y, w, h, colorFg, colorBg)
{
}

void TextLabel::setFormat(String fmt)
{
    _format = fmt;
}

void TextLabel::draw(float value)
{
    // Default draw, just prints format string if no time provided or not used
    _gfx->setFont(&FreeMono9pt7b);
    _gfx->setTextSize(0);
    _gfx->setTextColor(_cFg);
    _gfx->setCursor(_x, _y + 12); // Baseline adjustment
    _gfx->print(_format);
}

void TextLabel::draw(time_t now)
{
    struct tm timeinfo;
    char strftime_buf[64];
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), _format.c_str(), &timeinfo);

    _gfx->setFont(&FreeMono9pt7b);
    _gfx->setTextSize(0);
    _gfx->setTextColor(_cFg);

    // If w > 0, we can align. If not, just draw at X.
    if (_w > 0)
    {
        int16_t x1, y1; uint16_t tw, th;
        _gfx->getTextBounds(strftime_buf, 0, 0, &x1, &y1, &tw, &th);
        _gfx->setCursor(_x, _y + 12); 
    }
    else
    {
        _gfx->setCursor(_x, _y + 12);
    }
    _gfx->print(strftime_buf);
}
