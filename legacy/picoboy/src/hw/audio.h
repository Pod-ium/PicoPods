// PicoBoy - PWM audio output fed by DMA from a ring buffer.
// The PWM carrier runs at the Game Boy's native 32768 Hz sample rate, and its wrap
// event paces the DMA, so the emulator can be synchronised to the audio clock.
#pragma once
#include <stdint.h>

void audio_init();
void audio_push(const int16_t* stereo, int count);   // interleaved L,R pairs
int  audio_buffered();                                // samples queued but not yet played
void audio_silence();                                 // fill the ring with silence
void audio_set_volume(int percent);
int  audio_get_volume();
