#ifndef perfmon_h_
#define perfmon_h_

#include <cstddef>

namespace perfmon
{
    // Dependency. Must be defined in external code.
    extern const unsigned kPulsePerSecond;
}

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


class PerfProfScope
{
public:
    explicit PerfProfScope(PERF_prof_sect **ptr, const char *id)
        : ptr_(ptr)
    {
        PERF_prof_sect_init(ptr, id);
        PERF_prof_sect_enter(*ptr);
    }

    ~PerfProfScope()
    {
        PERF_prof_sect_exit(*ptr_);
    }

private:
    PERF_prof_sect **ptr_;
};

#define PERF_PROF_SCOPE( sect, sect_descr ) \
    static PERF_prof_sect * sect = NULL; \
    PerfProfScope sect ## scp(&sect, sect_descr);


#endif // perfmon_h_