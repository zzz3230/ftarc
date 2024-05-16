#ifndef FTARC_TIMINGS_H
#define FTARC_TIMINGS_H


#define EXTENDED_TIMING 1

#if EXTENDED_TIMING
extern double processor_time;
extern double filesystem_time;
#endif

#if EXTENDED_TIMING
extern double ext_timing_temp_time;
#define ANY_TIMING_BEGIN ext_timing_temp_time = clock();
#define PROCESSOR_TIMING_END processor_time += clock() - ext_timing_temp_time;
#define FILESYSTEM_TIMING_END filesystem_time += clock() - ext_timing_temp_time;
#else
#define ANY_TIMING_BEGIN
#define PROCESSOR_TIMING_END
#define FILESYSTEM_TIMING_END
#endif

#endif