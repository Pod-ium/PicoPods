// Writer's LCD API on top of the shared picocalc_ui driver (lib/picocalc_ui).
// The panel driver itself (init sequence, 18-bit colour, CS/DC timing) lives
// in the library; this file only maps Writer's names and RGB565 colours.
#include "lcd.h"

#include "board.h"
#include "pcui/lcd.h"

namespace lcd {

void init() {
    pcui::lcd::Config cfg;
    cfg.spi = LCD_SPI;
    cfg.sck = LCD_SCK_PIN;
    cfg.mosi = LCD_MOSI_PIN;
    cfg.miso = LCD_MISO_PIN;
    cfg.cs = LCD_CS_PIN;
    cfg.dc = LCD_DC_PIN;
    cfg.rst = LCD_RST_PIN;
    cfg.hz = LCD_SPI_HZ;
    cfg.madctl = LCD_MADCTL;
    cfg.invert = LCD_INVERT != 0;
    pcui::lcd::init(cfg);
}

void setWindow(int x0, int y0, int x1, int y1) { pcui::lcd::set_window(x0, y0, x1, y1); }

void writePixels(const uint8_t* data, size_t bytes) { pcui::lcd::write(data, bytes); }

void endPixels() { pcui::lcd::end(); }

void fillRect(int x, int y, int w, int h, uint16_t color) { pcui::lcd::fill(x, y, w, h, to888(color)); }

}  // namespace lcd
