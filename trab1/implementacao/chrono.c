#define _POSIX_C_SOURCE 199309L

#include "chrono.h"
#include <stdio.h>
#include <time.h>

void chrono_reset(chronometer_t *chrono) {
    chrono->xtotal_ns = 0;
    chrono->xn_events = 0;
}

void chrono_start(chronometer_t *chrono) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &(chrono->xadd_time1));
}

void chrono_stop(chronometer_t *chrono) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &(chrono->xadd_time2));

    long long ns1 = (long long)chrono->xadd_time1.tv_sec * 1000000000LL +
                    chrono->xadd_time1.tv_nsec;
    long long ns2 = (long long)chrono->xadd_time2.tv_sec * 1000000000LL +
                    chrono->xadd_time2.tv_nsec;
    long long deltat_ns = ns2 - ns1;

    chrono->xtotal_ns += deltat_ns;
    chrono->xn_events++;
}

long long chrono_gettotal(chronometer_t *chrono) {
    return chrono->xtotal_ns;
}

long long chrono_getcount(chronometer_t *chrono) {
    return chrono->xn_events;
}

void chrono_reportTime(chronometer_t *chrono, const char *s) {
    if (chrono->xn_events == 0) return;
    printf("\n%s deltaT(ns): %lld ns for %ld ops \n"
           "        ==> each op takes %lld ns\n",
           s, chrono->xtotal_ns, chrono->xn_events,
           chrono->xtotal_ns / chrono->xn_events);
}

void chrono_report_TimeInLoop(chronometer_t *chrono, const char *s, int loop_count) {
    if (chrono->xn_events == 0 || loop_count == 0) return;
    long total_ops = chrono->xn_events * loop_count;
    printf("\n%s deltaT(ns): %lld ns for %ld ops \n"
           "        ==> each op takes %lld ns\n",
           s, chrono->xtotal_ns, total_ops,
           chrono->xtotal_ns / total_ops);
}
