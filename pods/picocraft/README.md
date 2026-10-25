# PicoCraft 1.8

Piccolo gioco voxel "sandbox" per **ClockworkPi PicoCalc** con **Raspberry Pi Pico 2 W** (RP2350).
Mondo a blocchi generato proceduralmente, rendering 3D in tempo reale con raycasting voxel
su entrambi i core, costruzione e distruzione di blocchi, volo, acqua trasparente e salvataggi su SD.

**Novità della 1.8:** PicoCraft è diventato un pod di PicoPods. Display, tastiera, scheda SD, schermata di crash e build PC/browser vengono dalle librerie comuni (`../../lib`); in questa cartella restano il motore del gioco e il ciclo principale. La 1.7 non funzionava sul PicoCalc per tre problemi nei driver scritti a mano, che le librerie risolvono (vedi [Perché la 1.7 non funzionava](#perché-la-17-non-funzionava)). I salvataggi della 1.7 si caricano senza modifiche.

## Installazione

1. Copia `picocraft.uf2` sul Pico 2 W in modalità BOOTSEL, oppure mettilo nella cartella `pico2-apps/` della SD e avvialo da **UF2 Loader**.
2. La SD serve solo per i salvataggi: `/picocraft/world1.pcw` … `world3.pcw` (FAT32 o exFAT, ~256 KB ciascuno). Il gioco parte anche senza scheda.

## Comandi

| Tasto | Azione |
|---|---|
| W A S D | muoversi |
| Frecce | guardarsi intorno |
| Spazio | saltare / nuotare / salire in volo |
| C | scendere in volo |
| J, Del o Backspace | rompere il blocco mirato |
| K o Invio | piazzare il blocco selezionato |
| 1-9, Q / E | scegliere lo slot della barra |
| Tab | cambiare il tipo di blocco nello slot |
| F | volo on/off |
| F1 | schermata comandi |
| F3 | info debug (FPS, core, coordinate, seed) |
| Esc | menu (salva, carica, nuovo mondo, distanza visiva, volo) |

Nella schermata iniziale: Invio per giocare, N per un nuovo mondo, L per caricare lo slot 1.
I testi del gioco ora sono in inglese, come negli altri pod.

## Compilazione

PicoCraft è uno dei pod dell'albero PicoPods. Dalla cartella principale del repository:

```sh
cmake -B build -DPICO_SDK_PATH=/percorso/pico-sdk      # PicoCalc + Pico 2 W
cmake --build build -j --target picocraft
# -> build/pods/picocraft/picocraft.uf2
```

Serve un RP2350 (Pico 2 / Pico 2 W): il mondo occupa 256 KB di RAM e i due fotogrammi altri 100 KB, più di quanto abbia un Pico originale. Per il Pico 2 senza Wi-Fi: `-DPD_BOARD=pico2`.

Su PC (`cmake -B build-host -DPD_TERMINAL=host`) il gioco si apre in una finestra (con SDL2), oppure gira con tasti da script e salva le schermate:

```sh
PD_SDCARD=./sd PD_FRAMES=./frames PD_KEYS="wait:2500,shot,enter,wait:500,f3,wait:500,shot" ./build-host/pods/picocraft/picocraft
```

## Come funziona

- **Rendering:** raycasting DDA per pixel a 160×160, ingrandito 2× sul display 320×320. Nebbia, texture 16×16 procedurali, ombreggiatura per faccia, blocchi con trasparenza (foglie, vetro) e acqua semitrasparente. Il raycaster gira dalla RAM.
- **Due core:** le righe di ogni fotogramma vengono distribuite una alla volta da un contatore condiviso. Il core 1 fa solo rendering; il core 0, mentre il core 1 lavora, invia al display il fotogramma precedente e legge la tastiera (letture I2C lente, ~16 ms l'una), poi prende le righe rimaste. Nel browser c'è un solo thread e il core 0 fa tutto.
- **Display (`src/screen.cpp`):** il pannello accetta solo colore a 18 bit via SPI, quindi un fotogramma 2× sono 300 KB, che non entrano in RAM accanto al mondo. Il fotogramma parte a fasce di 8 righe (16 righe sul pannello, 15 KB): una fascia viene convertita mentre la precedente è in viaggio via DMA. SPI del display a 50 MHz (49 ms a fotogramma), come PodBoy.
- **Mondo:** 128×128×32 blocchi, toroidale (i bordi si richiudono), 4 bit per blocco. Terreno con rumore periodico, montagne innevate, spiagge, laghi, alberi, grotte e vene d'oro.
- **Clock:** 200 MHz (tensione regolata dalla libreria).
- **Memoria:** ~443 KB di RAM statica su 520 KB.

## Perché la 1.7 non funzionava

Il motore del gioco (mondo, raycaster, fisica, menu) era corretto: compilato su PC disegna il mondo come previsto. I problemi erano tutti nei driver hardware scritti a mano in `lcd.c` e `kbd.c`:

1. **Formato colore del display.** La 1.7 impostava il pannello a 16 bit per pixel (`COLMOD 0x55`) e inviava pixel RGB565. Il pannello del PicoCalc (ST7365P, classe ILI9488) via SPI accetta solo 18 bit (3 byte per pixel) e ignora in silenzio il comando a 16 bit: ogni fotogramma veniva letto come 3 byte per pixel, quindi immagine scomposta e colori sbagliati, e solo 2/3 dello schermo aggiornati. I rettangoli di prova colorati all'avvio servivano proprio a vederlo. Le librerie PicoPods usano 18 bit (`0x66`).
2. **Temporizzazione di CS/DC del display.** Il controller ha bisogno di qualche centinaio di ns tra un fronte di CS/DC e il successivo; con commutazioni GPIO consecutive a 200 MHz unisce le transazioni e legge male i byte di comando, quindi l'inizializzazione poteva fallire in parte. Inoltre la finestra (`0x2A`/`0x2B`) e i pixel (`0x2C`) erano in transazioni CS separate, mentre il pannello vuole tutto in una sola. Il driver `St7365p` delle librerie attende tra un fronte e l'altro e apre finestra e pixel nella stessa transazione.
3. **Tastiera.** Tra la scrittura del registro e la lettura della risposta la 1.7 aspettava 150 µs; il controller STM32 della tastiera ha bisogno di molto più tempo (il codice di riferimento ClockworkPi aspetta 16 ms), quindi le letture restituivano dati vecchi o non validi e i tasti non rispondevano o arrivavano sbagliati. Le librerie usano il ritardo di riferimento (`PD_PICOCALC_KBD_GAP_US`).

L'invio via DMA con interrupt della 1.7 (disattivato con `LCD_USE_DMA 0` dopo i problemi di "IRQ impazzito") è sostituito dall'invio a fasce descritto sopra, senza interrupt.

## Se qualcosa non va

| Sintomo | Cosa provare |
|---|---|
| Display con artefatti | Riporta `PD_PICOCALC_LCD_HZ` a `25000000` in `CMakeLists.txt` (più lento: 98 ms a fotogramma) |
| Colori negativi | `PD_PICOCALC_LCD_INVERT=0` in `CMakeLists.txt` |
| Instabilità o crash casuali | `SYS_CLOCK_KHZ 150000` in `src/config.h` (niente overclock) |
| Tastiera poco reattiva | `PD_PICOCALC_KBD_GAP_US=2000` in `CMakeLists.txt` |
| Gioco lento | Distanza visiva più bassa dal menu, oppure `RW`/`RH` più piccoli in `src/config.h` (es. 128×128: l'immagine resta centrata, con un bordo nero) |

In caso di crash compare una schermata rossa con il program counter. L'uscita seriale (UART0, 115200 baud) è attiva per il debug.

## Struttura

```
src/main.cpp      ciclo di gioco, fisica, HUD, menu, titolo, divisione del lavoro tra i core
src/render.cpp    raycaster voxel (in RAM)
src/world.cpp     generazione del mondo, picking dei blocchi
src/textures.cpp  texture procedurali
src/gfx.cpp       font 3x5 e primitive 2D sul fotogramma 160x160
src/screen.cpp    invio al display: ingrandimento 2x, 18 bit, a fasce via DMA
src/storage.cpp   salvataggi (pd_fs)
src/config.h      risoluzione, mondo, distanza visiva, clock
```

## Licenze

- Codice PicoCraft: tuo.
- FatFs (ChaN): licenza BSD-style, vedi `lib/services/fs/fatfs/LICENSE.txt`.
- Font Terminus (schermata iniziale, Dimitar Toshkov Zhekov): SIL Open Font License 1.1.

**Stato:** la 1.8 è stata compilata per Pico 2 W e provata su PC (build host: titolo, gioco, menu, salvataggio e caricamento); non è ancora stata provata sull'hardware.
