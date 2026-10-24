#pragma once
// Minimal FatFs surface so lib/picocalc_ui/src/ui.cpp (its file browser)
// compiles in the host simulator. Writer uses its own browser, so opening a
// folder here simply fails.
#include <cstdint>
typedef enum { FR_OK = 0, FR_NO_PATH = 5 } FRESULT;
typedef struct { int dummy; } DIR;
typedef struct { uint32_t fsize; uint8_t fattrib; char fname[256]; } FILINFO;
#define AM_HID 0x02
#define AM_SYS 0x04
#define AM_DIR 0x10
inline FRESULT f_opendir(DIR*, const char*) { return FR_NO_PATH; }
inline FRESULT f_readdir(DIR*, FILINFO* fi) { fi->fname[0] = 0; return FR_OK; }
inline FRESULT f_closedir(DIR*) { return FR_OK; }
