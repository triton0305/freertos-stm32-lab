#ifndef LAB_TIMING_H
#define LAB_TIMING_H
#include <stdint.h>
typedef struct {
    uint64_t first_host, last_host, host_sum, min_interval, max_interval;
    uint32_t first_mcu, last_mcu;
    unsigned intervals;
    int started;
} Timing;
/* Return -1 on MCU reset/discontinuity, +1 on interval, 0 on first sample. */
int timing_sample(Timing *t, uint64_t host, uint32_t mcu);
void timing_print(const char *name, const Timing *t, uint32_t target_ms);
#endif
