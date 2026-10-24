// SD card over SPI (SPI0 on the PicoCalc) + FatFs disk I/O glue.
#include <cstring>

#include "board.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

extern "C" {
#include "diskio.h"
#include "ff.h"
}

namespace {

bool g_ready = false;
bool g_highCap = false;  // SDHC/SDXC: block addressing
uint32_t g_sectors = 0;

inline uint8_t xfer(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(SD_SPI, &b, &r, 1);
    return r;
}
inline void select() { gpio_put(SD_CS_PIN, 0); xfer(0xFF); }
inline void deselect() { gpio_put(SD_CS_PIN, 1); xfer(0xFF); }

// Cards may stay busy for a long time after writes (FAT updates, erasing
// clusters when a big file is deleted or truncated). 500 ms was too tight.
constexpr uint32_t kBusyMs = 2000;
constexpr uint32_t kReadTokenMs = 500;
constexpr int kRetries = 3;

bool waitReady(uint32_t timeoutMs) {
    const absolute_time_t until = make_timeout_time_ms(timeoutMs);
    do {
        if (xfer(0xFF) == 0xFF) return true;
    } while (absolute_time_diff_us(get_absolute_time(), until) > 0);
    return false;
}

// Sends a command; leaves the card selected. Returns R1.
uint8_t command(uint8_t cmd, uint32_t arg) {
    if (cmd & 0x80) {  // ACMD: prefix with CMD55
        cmd &= 0x7F;
        const uint8_t r = command(55, 0);
        if (r > 1) return r;
    }
    if (cmd != 12) {
        deselect();
        select();
        if (cmd != 0 && !waitReady(kBusyMs)) return 0xFF;
    }
    uint8_t pkt[6] = {uint8_t(0x40 | cmd), uint8_t(arg >> 24), uint8_t(arg >> 16),
                      uint8_t(arg >> 8), uint8_t(arg), 0x01};
    if (cmd == 0) pkt[5] = 0x95;
    if (cmd == 8) pkt[5] = 0x87;
    spi_write_blocking(SD_SPI, pkt, 6);
    if (cmd == 12) xfer(0xFF);  // skip stuff byte
    uint8_t r = 0xFF;
    for (int i = 0; i < 10; ++i) {
        r = xfer(0xFF);
        if (!(r & 0x80)) break;
    }
    return r;
}

bool readData(uint8_t* buf, uint32_t n) {
    const absolute_time_t until = make_timeout_time_ms(kReadTokenMs);
    uint8_t token;
    do {
        token = xfer(0xFF);
    } while (token == 0xFF && absolute_time_diff_us(get_absolute_time(), until) > 0);
    if (token != 0xFE) return false;
    spi_read_blocking(SD_SPI, 0xFF, buf, n);
    xfer(0xFF);  // CRC
    xfer(0xFF);
    return true;
}

bool writeData(const uint8_t* buf, uint8_t token) {
    if (!waitReady(kBusyMs)) return false;
    xfer(token);
    if (token == 0xFD) return true;  // stop-tran token has no payload
    spi_write_blocking(SD_SPI, buf, 512);
    xfer(0xFF);  // dummy CRC
    xfer(0xFF);
    return (xfer(0xFF) & 0x1F) == 0x05;
}

bool cardInit() {
    g_ready = false;
    spi_init(SD_SPI, 400 * 1000);
    spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(SD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SD_MISO_PIN, GPIO_FUNC_SPI);
    gpio_pull_up(SD_MISO_PIN);
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_put(SD_CS_PIN, 1);
    sleep_ms(2);
    for (int i = 0; i < 10; ++i) xfer(0xFF);  // >= 74 clocks with CS high

    uint8_t r = 0xFF;
    for (int i = 0; i < 10 && r != 1; ++i) r = command(0, 0);
    if (r != 1) { deselect(); return false; }

    bool v2 = false;
    if (command(8, 0x1AA) == 1) {
        uint8_t ocr[4];
        spi_read_blocking(SD_SPI, 0xFF, ocr, 4);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) { deselect(); return false; }
        v2 = true;
    }
    const absolute_time_t until = make_timeout_time_ms(1500);
    do {
        r = command(0x80 | 41, v2 ? 0x40000000 : 0);
    } while (r == 1 && absolute_time_diff_us(get_absolute_time(), until) > 0);
    if (r != 0) { deselect(); return false; }

    g_highCap = false;
    if (v2 && command(58, 0) == 0) {
        uint8_t ocr[4];
        spi_read_blocking(SD_SPI, 0xFF, ocr, 4);
        g_highCap = (ocr[0] & 0x40) != 0;
    }
    if (!g_highCap) command(16, 512);

    uint8_t csd[16];
    g_sectors = 0;
    if (command(9, 0) == 0 && readData(csd, 16)) {
        if ((csd[0] >> 6) == 1) {
            const uint32_t cs = (uint32_t(csd[7] & 0x3F) << 16) | (uint32_t(csd[8]) << 8) | csd[9];
            g_sectors = (cs + 1) * 1024;
        } else {
            const uint32_t rbl = csd[5] & 15;
            const uint32_t cs = (uint32_t(csd[6] & 3) << 10) | (uint32_t(csd[7]) << 2) | (csd[8] >> 6);
            const uint32_t mult = ((csd[9] & 3) << 1) | (csd[10] >> 7);
            g_sectors = (cs + 1) << (mult + 2 + rbl - 9);
        }
    }
    deselect();
    spi_set_baudrate(SD_SPI, SD_SPI_HZ);
    g_ready = true;
    return true;
}

}  // namespace

extern "C" {

DSTATUS disk_status(BYTE pdrv) { return (pdrv == 0 && g_ready) ? 0 : STA_NOINIT; }

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return cardInit() ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !g_ready) return RES_NOTRDY;
    for (int attempt = 0; attempt < kRetries; ++attempt) {
        const uint32_t addr = g_highCap ? sector : sector * 512;
        bool ok;
        if (count == 1) {
            ok = command(17, addr) == 0 && readData(buff, 512);
        } else {
            ok = command(18, addr) == 0;
            for (UINT i = 0; ok && i < count; ++i) ok = readData(buff + i * 512, 512);
            command(12, 0);
            waitReady(kBusyMs);  // CMD12 has a busy (R1b) response
        }
        deselect();
        if (ok) return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !g_ready) return RES_NOTRDY;
    for (UINT i = 0; i < count; ++i) {
        const uint32_t s = sector + i;
        bool ok = false;
        for (int attempt = 0; !ok && attempt < kRetries; ++attempt) {
            ok = command(24, g_highCap ? s : s * 512) == 0 && writeData(buff + i * 512, 0xFE) &&
                 waitReady(kBusyMs);
            deselect();
        }
        if (!ok) return RES_ERROR;
    }
    return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0 || !g_ready) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC: {
            select();
            const bool ok = waitReady(kBusyMs);
            deselect();
            return ok ? RES_OK : RES_ERROR;
        }
        case GET_SECTOR_COUNT: *static_cast<LBA_t*>(buff) = g_sectors; return RES_OK;
        case GET_SECTOR_SIZE: *static_cast<WORD*>(buff) = 512; return RES_OK;
        case GET_BLOCK_SIZE: *static_cast<DWORD*>(buff) = 1; return RES_OK;
        default: return RES_PARERR;
    }
}

}  // extern "C"
