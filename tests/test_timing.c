#define _POSIX_C_SOURCE 200809L
#include "../tools/timing.h"
#include "../App/lab_time.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
static uint64_t now_ms(void) { LARGE_INTEGER n,f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f); return (uint64_t)((long double)n.QuadPart*1000/f.QuadPart); }
static void sleep_ms(unsigned n) { Sleep(n); }
#else
#include <time.h>
static uint64_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000; }
static void sleep_ms(unsigned n) { struct timespec t={n/1000,(long)(n%1000)*1000000}; nanosleep(&t,NULL); }
#endif
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void)
{
    uint32_t deadline=UINT32_MAX-99;
    CHECK(!lab_due(UINT32_MAX-100,deadline));
    CHECK(lab_due(400,deadline));
    CHECK(lab_advance(400,&deadline,100)==5 && deadline==500);
    LabDebounce b={0};
    CHECK(!lab_debounce(&b,10,1)); CHECK(!lab_debounce(&b,30,0));
    CHECK(!lab_debounce(&b,40,1)); CHECK(!lab_debounce(&b,89,1));
    CHECK(lab_debounce(&b,90,1)); CHECK(!lab_debounce(&b,2000,1));
    CHECK(!lab_debounce(&b,2010,0)); CHECK(!lab_debounce(&b,2050,1));
    CHECK(!lab_debounce(&b,2100,0)); CHECK(!lab_debounce(&b,2150,0));
    CHECK(!lab_debounce(&b,2200,1)); CHECK(lab_debounce(&b,2250,1));
    Timing wrap={0}; timing_sample(&wrap,0,UINT32_MAX-499);
    CHECK(timing_sample(&wrap,1000,500)==1);
    CHECK(timing_sample(&wrap,2000,0)==-1);
    Timing t={0}; uint64_t start=now_ms(); timing_sample(&t,start,0);
    for(unsigned i=1;i<=10;i++) {
        uint64_t goal=start+i*1000;
        while(now_ms()<goal) sleep_ms(1);
        /* Synthetic MCU data: tests host clock and arithmetic, NOT hardware. */
        timing_sample(&t,now_ms(),i*1000);
    }
    timing_print("HOST_SYNTHETIC_MCU",&t,1000);
    CHECK(t.last_host-t.first_host>=10000);
    CHECK(t.last_host-t.first_host<=11000);
    puts("HOST VERIFIED: monotonic 10-second test, deadline wrap, debounce, reset PASS");
    return 0;
}
