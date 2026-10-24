#pragma once
#include <cstdint>
#include <cstddef>
#define GPIO_OUT 1
#define GPIO_FUNC_SPI 1
void gpio_put(int pin, int v);
inline void gpio_init(int){}
inline void gpio_set_dir(int,int){}
inline void gpio_set_function(int,int){}
inline void gpio_set_input_hysteresis_enabled(int,bool){}
inline void busy_wait_at_least_cycles(uint32_t){}
