#pragma once
#include <cstdint>
#include <chrono>
typedef uint64_t absolute_time_t;
inline absolute_time_t get_absolute_time(){ using namespace std::chrono; return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count(); }
inline uint32_t to_ms_since_boot(absolute_time_t t){ return uint32_t(t/1000); }
inline void sleep_ms(int){}
inline void sleep_us(int){}
inline void tight_loop_contents(){}
