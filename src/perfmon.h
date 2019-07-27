#ifndef perfmon_h_
#define perfmon_h_

#include <cstddef>

void PERF_log_pulse(double val);
size_t PERF_repr( char *out_buf, size_t n );

class PERF_prof_sect;

void PERF_prof_sect_init(PERF_prof_sect **ptr, const char *id);
void PERF_prof_sect_enter(PERF_prof_sect *ptr);
void PERF_prof_sect_exit(PERF_prof_sect *ptr);
void PERF_prof_reset( void );
size_t PERF_prof_repr_pulse( char *out_buf, size_t n );
size_t PERF_prof_repr_total( char *out_buf, size_t n );
size_t PERF_prof_repr_sect( char *out_buf, size_t n, const char *id );

#define PERF_PROF_ENTER( sect, sect_descr ) \
    static PERF_prof_sect * sect = NULL; \
    PERF_prof_sect_init( & sect, sect_descr ); \
    PERF_prof_sect_enter( sect )

#define PERF_PROF_EXIT( sect ) \
    PERF_prof_sect_exit( sect )

#endif // perfmon_h_