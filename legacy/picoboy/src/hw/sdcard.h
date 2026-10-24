// PicoBoy - SD card (SPI). FatFs calls the disk_* functions in sdcard.cpp.
#pragma once
void sd_hw_init();
bool sd_card_present();
