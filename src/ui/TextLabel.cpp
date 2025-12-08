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
    
    // Simple centering or left align? Original code was specific. 
    // Let's right align within the width if it looks like a time string?
    // Actually, Layout engine gives x,y. Let's start text at x.
    // The original code calculated X based on text width from right edge.
    // Here we trust X from config.
    
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
        // Align Right logic if X is near edge? Or just trust user layout.
        // Let's implement Right Alignment if X is effectively 0 (meaning user wants us to calculate it?)
        // No, user provided X, Y, W. Let's Right Align inside W if W is spacious?
        // Or simplified: Left align at X.
        
        // Wait, original logic: int timeX = Config::EPD_WIDTH_PX - 15 - w;
        // User probably put X=... in config.
        // Let's just draw at X.
        _gfx->setCursor(_x, _y + 12); 
    }
    else
    {
        _gfx->setCursor(_x, _y + 12);
    }
    _gfx->print(strftime_buf);
}
