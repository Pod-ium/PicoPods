# PicoBoy

Emulatore Game Boy (DMG) in C++ per **ClockworkPi PicoCalc** con **Raspberry Pi Pico 2 W**.

- Scelta della ROM da un file manager sulla scheda SD
- Menu di pausa (Esc): riprendi, reset, **carica un'altra ROM senza riavviare**, schema dei comandi, palette, volume
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

Serve il Pico SDK 2.1.x e il toolchain `arm-none-eabi-gcc`.

```sh
export PICO_SDK_PATH=/percorso/pico-sdk
mkdir build && cd build
cmake ..
make -j
```

L'output è `build/picoboy.uf2`. Non serve un linker script personalizzato: UF2 Loader esegue gli UF2 standard.

## Come funziona

- **Interfaccia e display** vengono dalla libreria condivisa `lib/picocalc_ui` (driver LCD, font, menu, file manager), estratta da PicoCalc Writer e riutilizzabile in altri progetti PicoCalc. Vedi `lib/picocalc_ui/README.md`.
- **Core 0** esegue l'emulazione. Lo schermo 160×144 viene scalato 2× (320×288) e inviato al display via DMA, una riga alla volta da interrupt, così l'emulazione non si ferma durante il trasferimento.
- **Core 1** legge la tastiera. Il controller STM32 della tastiera richiede letture I2C lente e bloccanti.
- **La ROM viene copiata in flash** e letta direttamente da lì, perché le ROM fino a 1–2 MB non entrano nei 520 KB di RAM. La copia avviene solo quando cambi gioco; riaprire lo stesso file è istantaneo. Con UF2 Loader il firmware rileva la partizione dell'app (rimappata dal loader) e resta al suo interno.
- **La velocità è sincronizzata sull'audio**: il PWM gira a 32768 Hz, la frequenza nativa del Game Boy, e l'emulatore si regola in base a quanti campioni sono in coda.

## Limiti noti

- **Frame rate del display circa 10–12 fps**: il pannello accetta solo colore a 18 bit via SPI, a 25 MHz. Il gioco gira comunque a velocità piena, a 60 Hz, e salta solo alcuni fotogrammi a schermo.
- **Solo Game Boy originale**. I giochi "solo Game Boy Color" vengono rifiutati con un messaggio; quelli compatibili con entrambi girano in modalità DMG.
- L'orologio MBC3 (RTC) avanza solo mentre il gioco è in esecuzione e non viene salvato nel `.sav`.
- Nessun boot ROM Nintendo: l'emulatore parte direttamente dallo stato post-boot.

## Se qualcosa non va

| Sintomo | Cosa provare in `src/config.h` |
|---|---|
| Nessun audio | `AUDIO_PIN_L 26` (le schede più vecchie usano GP26 invece di GP28) |
| Display con artefatti | Abbassa `hz` o aumenta `kEdgeCycles` in `lib/picocalc_ui` |
| Instabilità o crash casuali | `SYS_CLOCK_KHZ 150000` (niente overclock) |
| Tastiera poco reattiva | Abbassa `KBD_READ_GAP_US` (per esempio `2000`) |

L'uscita seriale (UART0, 115200 baud) è attiva per il debug.

## Test del core su PC

Il core dell'emulatore (`src/gb/`) non dipende dal Pico e si compila anche su PC:

```sh
g++ -O2 -o host_test tools/host_test/main.cpp src/gb/*.cpp
./host_test rom.gb [frame] [schermata.pgm]
```

Il core è stato verificato così:

- supera tutti i test `cpu_instrs` di Blargg (gli 11 singoli e la ROM combinata MBC1) e `instr_timing`;
- riproduce **dmg-acid2** identico pixel per pixel all'immagine di riferimento.

## Licenze

- Codice PicoBoy: tuo.
- FatFs (ChaN): licenza BSD-style, vedi `lib/fatfs/LICENSE.txt`.
- Font Terminus (Dimitar Toshkov Zhekov): SIL Open Font License 1.1.
- Driver del display basato su PicoCalc Writer e sul codice di riferimento ClockworkPi.

Usa ROM ricavate dalle tue cartucce, oppure giochi homebrew.
