#ifndef LAB_TIME_H
#define LAB_TIME_H
#include <stdint.h>
#include "app_config.h"
/* All deadlines are less than INT32_MAX ticks ahead; tick rate is 1000 Hz. */
static inline int lab_due(uint32_t now, uint32_t deadline)
{ return (int32_t)(now - deadline) >= 0; }
static inline uint32_t lab_advance(uint32_t now, uint32_t *deadline, uint32_t period)
{
    uint32_t skipped = (now - *deadline) / period;
    *deadline += (skipped + 1u) * period;
    return skipped;
}
typedef struct { uint32_t since; int candidate, stable; } LabDebounce;
/* Call on edge notification and periodically. A held press never repeats.
 * Rearm only after a stable 50 ms release. */
static inline int lab_debounce(LabDebounce *d, uint32_t now, int pressed)
{
    if (pressed != d->candidate) { d->candidate = pressed; d->since = now; }
    if (d->candidate != d->stable && (uint32_t)(now - d->since) >= APP_BUTTON_DEBOUNCE_MS) {
        d->stable = d->candidate;
        return d->stable;
    }
    return 0;
}
#endif
