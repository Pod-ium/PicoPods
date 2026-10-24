# PicoBoy 1.2

Emulatore Game Boy (DMG) in C++ per **ClockworkPi PicoCalc** con **Raspberry Pi Pico 2 W**.

**Correzione nella 1.2:** dopo l'overclock a 200 MHz l'SDK spostava il clock delle periferiche sui 48 MHz dell'USB, limitando ogni SPI a 24 MHz qualunque velocità si scegliesse. Ora il clock delle periferiche segue quello di sistema, e la barra del titolo mostra la velocità SPI reale (es. `60/15 fps 33MHz`).

**Novità della 1.2:** opzione **Screen speed** nel menu di pausa (Normal 25 MHz, Fast 33 MHz, Turbo 50 MHz) che accelera solo l'invio dei fotogrammi di gioco, mentre menu e testi restano alla velocità sicura; aggiornamenti dello schermo a ritmo regolare durante gli scorrimenti; palette, volume e velocità vengono ricordati in `/.picoboy/settings.txt`.

**Novità della 1.1:** PicoBoy è ora costruito su picodev. Display, tastiera (letta sul core 1), audio PWM, scheda SD, cache delle ROM in flash, menu e file manager vengono dalle librerie condivise (`../../lib`); in questa cartella restano solo il core dell'emulatore (`src/gb/`), `src/main.cpp` e `src/config.h`. Funziona anche su PC (build host di picodev). Serve un RP2350 (Pico 2 / Pico 2 W): il buffer video da 276 KB non entra nella RAM di un Pico originale.

- Scelta della ROM da un file manager sulla scheda SD
- Menu di pausa (Esc): riprendi, reset, **carica un'altra ROM senza riavviare**, schema dei comandi, palette, volume, velocità dello schermo
- Salvataggi `.sav` automatici per le cartucce con batteria
- Audio stereo PWM, 4 canali
- Cartucce supportate: ROM only, MBC1, MBC2, MBC3 (con RTC), MBC5

## Installazione rapida

1. Copia `picoboy.uf2` sul Pico 2 W in modalità BOOTSEL, oppure mettilo nella cartella `pico2-apps/` della SD e avvialo da **UF2 Loader**.
2. Crea una cartella `/roms` sulla SD e copiaci i file `.gb` (sottocartelle ammesse). Se `/roms` non esiste, il browser parte dalla radice.
3. La SD deve essere FAT32 o exFAT.

## Comandi

| Game Boy | PicoCalc |
|---|---|
| D-pad | Frecce |
| A | `X` oppure `K` |
| B | `Z` oppure `J` |
| Start | Invio |
| Select | Spazio oppure Backspace |
| Pausa / menu | Esc oppure F1 |

Nel browser: frecce su/giù per scorrere, Invio/destra per aprire, sinistra/Backspace per risalire di cartella.
I tasti si cambiano in `src/config.h`; la voce **Controls** del menu di pausa mostra sempre la mappatura attuale.

## Compilazione

PicoBoy è una delle app dell'albero picodev. Dalla cartella principale di picodev:

```sh
cmake -B build -DPICO_SDK_PATH=/percorso/pico-sdk      # PicoCalc + Pico 2 W
cmake --build build -j --target picoboy
# -> build/apps/picoboy/picoboy.uf2
```

Serve il Pico SDK 2.1.x e il toolchain `arm-none-eabi-gcc`. Non serve un linker script personalizzato: UF2 Loader esegue gli UF2 standard. Per il Pico 2 senza Wi-Fi: `-DPD_BOARD=pico2`.

Su PC (`cmake -B build-host -DPD_TERMINAL=host`) l'emulatore gira con tasti da script e salva le schermate:

```sh
PD_SDCARD=./sd PD_FRAMES=./frames PD_KEYS="down,enter,wait:3000,shot" ./build-host/apps/picoboy/picoboy
```

## Come funziona

- **picodev** fornisce display, font, menu e file manager (`pd_ui`, `pd_gfx`), tastiera (`pd_input`, lettura sul core 1), audio (`PwmAudio`), SD e FatFs (`pd_fs`), cache in flash (`pd_flash_cache`) e l'invio dei fotogrammi via DMA (`IndexedBlitter`). Vedi `docs/MIGRATION.md` di picodev.
- **Core 0** esegue l'emulazione. Lo schermo 160×144 viene scalato 2× (320×288) e inviato al display via DMA, una riga alla volta da interrupt, così l'emulazione non si ferma durante il trasferimento.
- **Core 1** legge la tastiera. Il controller STM32 della tastiera richiede letture I2C lente e bloccanti.
- **La ROM viene copiata in flash** e letta direttamente da lì, perché le ROM fino a 1–2 MB non entrano nei 520 KB di RAM. La copia avviene solo quando cambi gioco; riaprire lo stesso file è istantaneo. Con UF2 Loader il firmware rileva la partizione dell'app (rimappata dal loader) e resta al suo interno.
- **La velocità è sincronizzata sull'audio**: il PWM gira a 32768 Hz, la frequenza nativa del Game Boy, e l'emulatore si regola in base a quanti campioni sono in coda.

## Limiti noti

- **Negli scorrimenti a schermo intero lo schermo resta indietro rispetto al gioco.** Un fotogramma completo sono 276 KB (18 bit per pixel, l'unico formato accettato dal pannello): circa 88 ms a 25 MHz, 66 ms a 33 MHz, 44 ms a 50 MHz, cioè circa 11, 15 o 22 aggiornamenti al secondo. Il gioco gira comunque a 60 fps. Il pannello mostra l'immagine mentre viene scritta e la sua linea di sincronizzazione (TE) non è collegata al Pico, quindi durante gli scorrimenti si vede un po' di tearing; più alta è la velocità, meno si nota.
- **Screen speed oltre le specifiche.** Il datasheet dell'ST7365P indica 15 MHz circa (ciclo di scrittura di 66 ns); già i 25 MHz di ClockworkPi sono un overclock. Se con Fast o Turbo la grafica del gioco appare corrotta, scegli una velocità più bassa: menu e testi restano sempre leggibili perché usano la velocità normale.
- **Solo Game Boy originale**. I giochi "solo Game Boy Color" vengono rifiutati con un messaggio; quelli compatibili con entrambi girano in modalità DMG.
- L'orologio MBC3 (RTC) avanza solo mentre il gioco è in esecuzione e non viene salvato nel `.sav`.
- Nessun boot ROM Nintendo: l'emulatore parte direttamente dallo stato post-boot.

## Se qualcosa non va

| Sintomo | Cosa provare |
|---|---|
| Nessun audio | `PD_PICOCALC_AUDIO_L=26` in `CMakeLists.txt` (le schede più vecchie usano GP26 invece di GP28) |
| Display con artefatti | Abbassa `PD_PICOCALC_LCD_HZ` in `CMakeLists.txt` |
| Instabilità o crash casuali | `SYS_CLOCK_KHZ 150000` in `src/config.h` (niente overclock) |
| Tastiera poco reattiva | `PD_PICOCALC_KBD_GAP_US=2000` in `CMakeLists.txt` |

L'uscita seriale (UART0, 115200 baud) è attiva per il debug.

## Test del core su PC

Il core dell'emulatore (`src/gb/`) non dipende dal Pico e si compila anche su PC:

```sh
g++ -O2 -o host_test tools/host_test/main.cpp src/gb/*.cpp   # da apps/picoboy
./host_test rom.gb [frame] [schermata.pgm]
```

Il core è stato verificato così:

- supera tutti i test `cpu_instrs` di Blargg (gli 11 singoli e la ROM combinata MBC1) e `instr_timing`;
- riproduce **dmg-acid2** identico pixel per pixel all'immagine di riferimento.

## Licenze

- Codice PicoBoy: tuo.
- FatFs (ChaN): licenza BSD-style, vedi `lib/fs/fatfs/LICENSE.txt` di picodev.
- Font Terminus (Dimitar Toshkov Zhekov): SIL Open Font License 1.1.
- Driver del display (picodev) basato su PicoCalc Writer e sul codice di riferimento ClockworkPi.

Usa ROM ricavate dalle tue cartucce, oppure giochi homebrew.

**Stato:** la 1.1 è stata compilata e provata su PC (supera `cpu_instrs` e riproduce dmg-acid2 nella build host); non è ancora stata provata sull'hardware.
