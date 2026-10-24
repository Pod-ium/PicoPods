// PicoBoy - PWM audio via DMA
#include "audio.h"
#include "../config.h"
#include "../gb/gb.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

static constexpr int RING_BITS = 11;                 // 2048 samples per channel
static constexpr int RING = 1 << RING_BITS;
static constexpr uint32_t RING_MASK = RING - 1;
static constexpr uint32_t DMA_COUNT = 0x0FFFFFFF;  // re-armed long before it runs out

static uint32_t ring_l[RING] __attribute__((aligned(RING * 4)));
static uint32_t ring_r[RING] __attribute__((aligned(RING * 4)));

static int ch_l = -1, ch_r = -1;
static uint slice_l, slice_r, chan_l, chan_r;
static bool same_slice;
static uint32_t wrap_top, mid;
static uint32_t wpos;
static int volume = DEFAULT_VOLUME;
static int32_t gain;   // Q15 multiplier from int16 sample to PWM level offset

static void recompute_gain() { gain = (int32_t)((int64_t)mid * volume / 100); }

static inline uint32_t level_word(uint32_t lvl, uint chan) { return chan == PWM_CHAN_B ? (lvl << 16) : lvl; }

static void start_dma(int ch, uint32_t* ring, uint slice) {
    dma_channel_config c = dma_channel_get_default_config(ch);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, RING_BITS + 2);   // wrap the read address every RING words
    channel_config_set_dreq(&c, pwm_get_dreq(slice));
    dma_channel_configure(ch, &c, &pwm_hw->slice[slice].cc, ring, DMA_COUNT, false);
}

void audio_init() {
    slice_l = pwm_gpio_to_slice_num(AUDIO_PIN_L); chan_l = pwm_gpio_to_channel(AUDIO_PIN_L);
    slice_r = pwm_gpio_to_slice_num(AUDIO_PIN_R); chan_r = pwm_gpio_to_channel(AUDIO_PIN_R);
    same_slice = slice_l == slice_r;

    uint32_t sys = clock_get_hz(clk_sys);
    wrap_top = sys / gb::AUDIO_RATE - 1;     // e.g. 200 MHz -> 6103 (~12.5 bit resolution)
    mid = wrap_top / 2;
    recompute_gain();

    gpio_set_function(AUDIO_PIN_L, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_PIN_R, GPIO_FUNC_PWM);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&cfg, 1);
    pwm_config_set_wrap(&cfg, wrap_top);
    pwm_init(slice_l, &cfg, false);
    if (!same_slice) pwm_init(slice_r, &cfg, false);

    wpos = 0;
    audio_silence();

    ch_l = dma_claim_unused_channel(true);
    start_dma(ch_l, ring_l, slice_l);
    uint32_t mask = 1u << ch_l;
    if (!same_slice) {
        ch_r = dma_claim_unused_channel(true);
        start_dma(ch_r, ring_r, slice_r);
        mask |= 1u << ch_r;
    }
    dma_start_channel_mask(mask);
    pwm_set_mask_enabled((1u << slice_l) | (1u << slice_r));
}

static uint32_t read_pos() {
    uintptr_t ra = dma_hw->ch[ch_l].read_addr;
    return ((ra - (uintptr_t)ring_l) >> 2) & RING_MASK;
}

int audio_buffered() { return (int)((wpos - read_pos()) & RING_MASK); }

void audio_silence() {
    for (int i = 0; i < RING; i++) {
        if (same_slice) ring_l[i] = level_word(mid, chan_l) | level_word(mid, chan_r);
        else { ring_l[i] = level_word(mid, chan_l); ring_r[i] = level_word(mid, chan_r); }
    }
    if (ch_l >= 0) wpos = (read_pos() + 256) & RING_MASK;   // restart with a small lead
}

static inline uint32_t to_level(int32_t s) {
    int32_t v = (int32_t)mid + ((s * gain) >> 15);
    if (v < 0) v = 0;
    if (v > (int32_t)wrap_top) v = wrap_top;
    return (uint32_t)v;
}

void audio_push(const int16_t* st, int n) {
    // Re-arm the DMA if the (very long) transfer count ever ran out
    if (!dma_channel_is_busy(ch_l)) {
        dma_channel_set_trans_count(ch_l, DMA_COUNT, false);
        uint32_t mask = 1u << ch_l;
        if (!same_slice) { dma_channel_set_trans_count(ch_r, DMA_COUNT, false); mask |= 1u << ch_r; }
        dma_start_channel_mask(mask);
    }
    // If the reader overtook us (underrun), jump ahead of it
    if (audio_buffered() > RING - 700) wpos = (read_pos() + 256) & RING_MASK;

    for (int i = 0; i < n; i++) {
        uint32_t l = to_level(st[i * 2]), r = to_level(st[i * 2 + 1]);
        if (same_slice) ring_l[wpos] = level_word(l, chan_l) | level_word(r, chan_r);
        else { ring_l[wpos] = level_word(l, chan_l); ring_r[wpos] = level_word(r, chan_r); }
        wpos = (wpos + 1) & RING_MASK;
    }
}

void audio_set_volume(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    volume = percent;
    recompute_gain();
}

int audio_get_volume() { return volume; }
