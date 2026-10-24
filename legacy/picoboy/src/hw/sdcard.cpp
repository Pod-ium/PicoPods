// PicoBoy - SD card over SPI, exposed to FatFs through the diskio interface.
#include "../config.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "sdcard.h"
extern "C" {
#include "ff.h"
#include "diskio.h"
}

static bool block_addr = true;   // SDHC/SDXC use block addressing
static DSTATUS status = STA_NOINIT;

static inline void cs(bool v) { gpio_put(SD_PIN_CS, v); }
static inline uint8_t xchg(uint8_t v) { uint8_t r; spi_write_read_blocking(SD_SPI, &v, &r, 1); return r; }

static bool wait_ready(uint32_t ms) {
    absolute_time_t until = make_timeout_time_ms(ms);
    do { if (xchg(0xFF) == 0xFF) return true; } while (!time_reached(until));
    return false;
}

static void deselect() { cs(1); xchg(0xFF); }
static bool select() { cs(0); xchg(0xFF); if (wait_ready(500)) return true; deselect(); return false; }

static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
    if (cmd & 0x80) {   // ACMD: prefix with CMD55
        cmd &= 0x7F;
        uint8_t r = send_cmd(55, 0);
        if (r > 1) return r;
    }
    if (cmd != 12) { deselect(); if (!select()) return 0xFF; }
    uint8_t pkt[6] = {(uint8_t)(0x40 | cmd), (uint8_t)(arg >> 24), (uint8_t)(arg >> 16),
                      (uint8_t)(arg >> 8), (uint8_t)arg, 0x01};
    if (cmd == 0) pkt[5] = 0x95;
    if (cmd == 8) pkt[5] = 0x87;
    spi_write_blocking(SD_SPI, pkt, 6);
    if (cmd == 12) xchg(0xFF);   // skip stuff byte
    uint8_t r;
    int n = 10;
    do { r = xchg(0xFF); } while ((r & 0x80) && --n);
    return r;
}

static bool rcvr_block(uint8_t* buf, uint32_t len) {
    absolute_time_t until = make_timeout_time_ms(200);
    uint8_t token;
    do { token = xchg(0xFF); } while (token == 0xFF && !time_reached(until));
    if (token != 0xFE) return false;
    spi_read_blocking(SD_SPI, 0xFF, buf, len);
    xchg(0xFF); xchg(0xFF);   // CRC
    return true;
}

static bool xmit_block(const uint8_t* buf, uint8_t token) {
    if (!wait_ready(500)) return false;
    xchg(token);
    if (token == 0xFD) return true;
    spi_write_blocking(SD_SPI, buf, 512);
    xchg(0xFF); xchg(0xFF);   // dummy CRC
    return (xchg(0xFF) & 0x1F) == 0x05;
}

void sd_hw_init() {
    static bool pins = false;
    if (!pins) {
        spi_init(SD_SPI, 400000);
        spi_set_format(SD_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(SD_PIN_SCK, GPIO_FUNC_SPI);
        gpio_set_function(SD_PIN_MOSI, GPIO_FUNC_SPI);
        gpio_set_function(SD_PIN_MISO, GPIO_FUNC_SPI);
        gpio_pull_up(SD_PIN_MISO);
        gpio_init(SD_PIN_CS); gpio_set_dir(SD_PIN_CS, GPIO_OUT); gpio_put(SD_PIN_CS, 1);
        gpio_init(SD_PIN_DETECT); gpio_set_dir(SD_PIN_DETECT, GPIO_IN); gpio_pull_up(SD_PIN_DETECT);
        pins = true;
    }
}

bool sd_card_present() {
    sd_hw_init();
    return !gpio_get(SD_PIN_DETECT);   // detect switch pulls low when a card is inserted
}

extern "C" DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv) return STA_NOINIT;
    sd_hw_init();
    spi_set_baudrate(SD_SPI, 400000);
    cs(1);
    for (int i = 0; i < 10; i++) xchg(0xFF);   // >= 74 clocks with CS high

    status = STA_NOINIT;
    uint8_t type = 0;
    if (send_cmd(0, 0) == 1) {
        absolute_time_t until = make_timeout_time_ms(1000);
        if (send_cmd(8, 0x1AA) == 1) {                          // SD v2
            uint8_t ocr[4];
            for (auto& o : ocr) o = xchg(0xFF);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
                while (!time_reached(until) && send_cmd(0x80 | 41, 1u << 30)) {}
                if (!time_reached(until) && send_cmd(58, 0) == 0) {
                    for (auto& o : ocr) o = xchg(0xFF);
                    block_addr = ocr[0] & 0x40;
                    type = 2;
                }
            }
        } else {                                                 // SD v1 or MMC
            uint8_t acmd = (send_cmd(0x80 | 41, 0) <= 1) ? (0x80 | 41) : 1;
            while (!time_reached(until) && send_cmd(acmd, 0)) {}
            if (!time_reached(until) && send_cmd(16, 512) == 0) type = 1;
            block_addr = false;
        }
    }
    deselect();
    if (type) {
        spi_set_baudrate(SD_SPI, SD_SPI_HZ);
        status = 0;
    }
    return status;
}

extern "C" DSTATUS disk_status(BYTE pdrv) { return pdrv ? STA_NOINIT : status; }

extern "C" DRESULT disk_read(BYTE pdrv, BYTE* buf, LBA_t sector, UINT count) {
    if (pdrv || !count) return RES_PARERR;
    if (status & STA_NOINIT) return RES_NOTRDY;
    uint32_t addr = block_addr ? (uint32_t)sector : (uint32_t)sector * 512;
    if (count == 1) {
        if (send_cmd(17, addr) == 0 && rcvr_block(buf, 512)) count = 0;
    } else if (send_cmd(18, addr) == 0) {
        do { if (!rcvr_block(buf, 512)) break; buf += 512; } while (--count);
        send_cmd(12, 0);
    }
    deselect();
    return count ? RES_ERROR : RES_OK;
}

extern "C" DRESULT disk_write(BYTE pdrv, const BYTE* buf, LBA_t sector, UINT count) {
    if (pdrv || !count) return RES_PARERR;
    if (status & STA_NOINIT) return RES_NOTRDY;
    uint32_t addr = block_addr ? (uint32_t)sector : (uint32_t)sector * 512;
    while (count) {
        if (send_cmd(24, addr) != 0 || !xmit_block(buf, 0xFE)) break;
        buf += 512;
        addr += block_addr ? 1 : 512;
        count--;
    }
    deselect();
    return count ? RES_ERROR : RES_OK;
}

extern "C" DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv) return RES_PARERR;
    if (status & STA_NOINIT) return RES_NOTRDY;
    switch (cmd) {
        case CTRL_SYNC: {
            bool ok = select();
            deselect();
            return ok ? RES_OK : RES_ERROR;
        }
        case GET_BLOCK_SIZE: *(DWORD*)buff = 1; return RES_OK;
        default: return RES_PARERR;
    }
}
