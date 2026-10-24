#pragma once
// DMA stubs for the host simulator: picocalc_ui claims a channel at init, but
// Writer only uses blocking writes, so nothing here ever runs a transfer.
#include <cstdint>
enum dma_channel_transfer_size { DMA_SIZE_8 = 0 };
struct dma_channel_config { int dummy; };
inline int dma_claim_unused_channel(bool) { return 0; }
inline dma_channel_config dma_channel_get_default_config(int) { return {}; }
inline void channel_config_set_transfer_data_size(dma_channel_config*, int) {}
inline void channel_config_set_read_increment(dma_channel_config*, bool) {}
inline void channel_config_set_write_increment(dma_channel_config*, bool) {}
inline void channel_config_set_dreq(dma_channel_config*, unsigned) {}
inline void dma_channel_configure(int, const dma_channel_config*, volatile void*, const void*, unsigned, bool) {}
inline void dma_channel_set_read_addr(int, const void*, bool) {}
inline void dma_channel_set_trans_count(int, unsigned, bool) {}
inline bool dma_channel_is_busy(int) { return false; }
inline void dma_channel_wait_for_finish_blocking(int) {}
