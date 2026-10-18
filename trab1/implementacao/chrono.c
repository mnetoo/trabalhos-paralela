// chrono.h
//
// A small library to measure time in programs
//
// by W.Zola (2017)

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if !defined(_WIN32) && !defined(__MINGW32__)
#include <sys/mman.h>
#endif

#include <sys/time.h>     /* struct timeval definition           */
#include <unistd.h>       /* declaration of gettimeofday()       */

#include <time.h>

#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
#endif

typedef struct {
    struct timespec xadd_time1, xadd_time2;
    long long xtotal_ns;
    long xn_events;
} chronometer_t;

static inline void chrono_reset( chronometer_t *chrono )
{
    chrono->xtotal_ns = 0;
    chrono->xn_events = 0;
}

static inline void chrono_start( chronometer_t *chrono ) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &(chrono->xadd_time1) );
}

static inline long long chrono_gettotal( chronometer_t *chrono ) {
    return chrono->xtotal_ns;
}

static inline long long chrono_getcount( chronometer_t *chrono ) {
    return chrono->xn_events;
}

static inline void chrono_stop( chronometer_t *chrono ) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &(chrono->xadd_time2) );

    long long ns1 = (long long)chrono->xadd_time1.tv_sec*1000*1000*1000 + 
                    chrono->xadd_time1.tv_nsec;
    long long ns2 = (long long)chrono->xadd_time2.tv_sec*1000*1000*1000 + 
                    chrono->xadd_time2.tv_nsec;
    long long deltat_ns = ns2 - ns1;
    
    chrono->xtotal_ns += deltat_ns;
    chrono->xn_events++;
}

static inline void chrono_decrease_time( chronometer_t *chrono, chronometer_t *sub_chrono ) {
    chrono->xtotal_ns -= sub_chrono->xtotal_ns;
}

static inline void chrono_decreaseTime( chronometer_t *chrono, chronometer_t *sub_chrono ) {
    chrono->xtotal_ns -= sub_chrono->xtotal_ns;
}

static inline void chrono_reportTime( chronometer_t *chrono, const char *s ) {
    if (chrono->xn_events == 0) return;
    printf("\n%s deltaT(ns): %lld ns for %ld ops \n"
           "        ==> each op takes %lld ns\n",
           s, chrono->xtotal_ns, chrono->xn_events, 
           chrono->xtotal_ns/chrono->xn_events );
}

static inline void chrono_report_TimeInLoop( chronometer_t *chrono, const char *s, int loop_count ) {
    if (chrono->xn_events == 0 || loop_count == 0) return;
    printf("\n%s deltaT(ns): %lld ns for %ld ops \n"
           "        ==> each op takes %lld ns\n",
           s, chrono->xtotal_ns, chrono->xn_events*loop_count, 
           chrono->xtotal_ns/(chrono->xn_events*loop_count) );
}
