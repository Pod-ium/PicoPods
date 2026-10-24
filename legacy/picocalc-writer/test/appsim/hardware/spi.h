#pragma once
#include <cstdint>
#include <cstddef>
typedef unsigned int uint;
typedef int spi_inst_t;
#define spi1 ((spi_inst_t*)1)
#define SPI_CPOL_0 0
#define SPI_CPHA_0 0
#define SPI_MSB_FIRST 0
#define SPI_SSPICR_RORIC_BITS 1
struct spi_hw_t { uint32_t dr, icr; };
inline spi_hw_t* spi_get_hw(spi_inst_t*) { static spi_hw_t hw; return &hw; }
inline unsigned spi_init(spi_inst_t*,unsigned b){return b;}
inline void spi_set_format(spi_inst_t*,int,int,int,int){}
inline bool spi_is_busy(spi_inst_t*) { return false; }
inline bool spi_is_readable(spi_inst_t*) { return false; }
inline unsigned spi_get_dreq(spi_inst_t*, bool) { return 0; }
int spi_write_blocking(spi_inst_t*, const uint8_t*, size_t);
