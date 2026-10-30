#include "timing.h"
#include <stdio.h>
#include <string.h>
int timing_sample(Timing *t, uint64_t host, uint32_t mcu)
{
    int reset = t->started && ((uint32_t)(mcu-t->last_mcu) > INT32_MAX || host<t->last_host);
    if (reset) memset(t,0,sizeof(*t));
    if (!t->started) {
        t->started=1; t->first_host=t->last_host=host;
        t->first_mcu=t->last_mcu=mcu; t->min_interval=UINT64_MAX;
        return reset ? -1 : 0;
    }
    uint64_t delta=host-t->last_host;
    t->host_sum+=delta; t->intervals++;
    if (delta<t->min_interval) t->min_interval=delta;
    if (delta>t->max_interval) t->max_interval=delta;
    t->last_host=host; t->last_mcu=mcu;
    return 1;
}
void timing_print(const char *name, const Timing *t, uint32_t target_ms)
{
    if (!t->intervals) return;
    uint64_t elapsed=t->last_host-t->first_host;
    uint32_t mcu=t->last_mcu-t->first_mcu;
    double avg=(double)t->host_sum/t->intervals;
    printf("[TIMING %s] host_elapsed_ms=%llu mcu_elapsed_ms=%lu drift_ms=%lld interval_avg_ms=%.3f min_ms=%llu max_ms=%llu target_ms=%lu avg_error_ms=%.3f cumulative_error_ms=%lld intervals=%u window=%s\n",
        name,(unsigned long long)elapsed,(unsigned long)mcu,(long long)mcu-(long long)elapsed,
        avg,(unsigned long long)t->min_interval,(unsigned long long)t->max_interval,
        (unsigned long)target_ms,avg-target_ms,
        (long long)elapsed-(long long)t->intervals*target_ms,t->intervals,
        elapsed>=10000 ? "10s_observed" : "collecting");
}
