// picocalc_ui - LCD driver. Timing notes come from PicoCalc Writer:
// the controller needs a minimum CS-high time between transactions and setup
// time after CS/DC changes. Back-to-back GPIO toggles are too fast for it; it
// then merges transactions and misreads command bytes. Every edge is padded.
#include "pcui/lcd.h"

#include <initializer_list>
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

namespace pcui {
namespace lcd {
namespace {

Config g;
bool g_in_data = false;
int g_dma = -1;

constexpr uint32_t kEdgeCycles = 80;   // ~400 ns at 200 MHz, ~530 ns at 150 MHz

inline void settle() { busy_wait_at_least_cycles(kEdgeCycles); }
inline void cs_low() { gpio_put(g.cs, 0); settle(); }
inline void cs_high() { settle(); gpio_put(g.cs, 1); settle(); }
inline void dc_cmd() { gpio_put(g.dc, 0); settle(); }
inline void dc_data() { gpio_put(g.dc, 1); settle(); }
inline void send(const uint8_t* d, size_t n) { spi_write_blocking(g.spi, d, n); }

void cmd(uint8_t c, const uint8_t* data = nullptr, size_t n = 0) {
    if (g_in_data) end();
    cs_low();
    dc_cmd();
    send(&c, 1);
    if (n) {
        dc_data();
        send(data, n);
    }
    cs_high();
}

void cmdl(uint8_t c, std::initializer_list<uint8_t> d) { cmd(c, d.begin(), d.size()); }

}  // namespace

void init(const Config& cfg) {
    g = cfg;
    spi_init(g.spi, g.hz);
    spi_set_format(g.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(g.sck, GPIO_FUNC_SPI);
    gpio_set_function(g.mosi, GPIO_FUNC_SPI);
    gpio_set_function(g.miso, GPIO_FUNC_SPI);
    gpio_set_input_hysteresis_enabled(g.miso, true);
    for (uint pin : {g.cs, g.dc, g.rst}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
    }
    sleep_ms(10);
    gpio_put(g.rst, 0);
    sleep_ms(10);
    gpio_put(g.rst, 1);
    sleep_ms(200);

    cmdl(0xE0, {0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F});
    cmdl(0xE1, {0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F});
    cmdl(0xC0, {0x17, 0x15});
    cmdl(0xC1, {0x41});
    cmdl(0xC5, {0x00, 0x12, 0x80});
    cmdl(0x36, {g.madctl});
    cmdl(0x3A, {0x66});        // 18-bit colour; 16-bit (0x55) is silently ignored
    cmdl(0xB0, {0x00});
    cmdl(0xB1, {0xA0});
    cmd(g.invert ? 0x21 : 0x20);
    cmdl(0xB4, {0x02});
    cmdl(0xB6, {0x02, 0x02, 0x3B});
    cmdl(0xB7, {0xC6});
    cmdl(0xE9, {0x00});
    cmdl(0xF7, {0xA9, 0x51, 0x2C, 0x82});
    cmd(0x11);
    sleep_ms(120);
    fill(0, 0, SCREEN_W, SCREEN_H, 0x000000);
    cmd(0x29);
    sleep_ms(120);
    cmdl(0x36, {g.madctl});   // the reference driver re-applies this too

    g_dma = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(g_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(g.spi, true));
    dma_channel_configure(g_dma, &c, &spi_get_hw(g.spi)->dr, nullptr, 0, false);
}

void set_window(int x0, int y0, int x1, int y1) {
    if (g_in_data) end();
    const uint8_t ca[] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
    const uint8_t ra[] = {uint8_t(y0 >> 8), uint8_t(y0), uint8_t(y1 >> 8), uint8_t(y1)};
    static const uint8_t kCaset = 0x2A, kRaset = 0x2B, kRamwr = 0x2C;
    cs_low();
    dc_cmd(); send(&kCaset, 1);
    dc_data(); send(ca, 4);
    dc_cmd(); send(&kRaset, 1);
    dc_data(); send(ra, 4);
    dc_cmd(); send(&kRamwr, 1);
    dc_data();
    g_in_data = true;
}

void write(const uint8_t* data, size_t bytes) { send(data, bytes); }

void write_dma(const uint8_t* data, size_t bytes) {
    dma_channel_set_read_addr(g_dma, data, false);
    dma_channel_set_trans_count(g_dma, bytes, true);
}

bool dma_busy() { return dma_channel_is_busy(g_dma); }

void wait_dma() { dma_channel_wait_for_finish_blocking(g_dma); }

void end() {
    if (!g_in_data) return;
    g_in_data = false;
    // After DMA the SPI may still be shifting and the RX FIFO has overflowed
    while (spi_is_busy(g.spi)) tight_loop_contents();
    while (spi_is_readable(g.spi)) (void)spi_get_hw(g.spi)->dr;
    spi_get_hw(g.spi)->icr = SPI_SSPICR_RORIC_BITS;
    cs_high();
}

int dma_channel() { return g_dma; }

void fill(int x, int y, int w, int h, Color color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    static uint8_t row[SCREEN_W * BPP];
    uint8_t px[3];
    encode(color, px);
    for (int i = 0; i < w; ++i) {
        row[3 * i] = px[0];
        row[3 * i + 1] = px[1];
        row[3 * i + 2] = px[2];
    }
    set_window(x, y, x + w - 1, y + h - 1);
    for (int r = 0; r < h; ++r) write(row, size_t(w) * BPP);
    end();
}

}  // namespace lcd
}  // namespace pcui
