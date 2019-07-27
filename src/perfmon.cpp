#include "perfmon.h"

#include <map>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <ctime>
#include <cfloat>
#include <cstring>

#include <sys/time.h>

static double const INFINITY = DBL_MAX;

#define PULSE_PER_SECOND 10
#define SEC_PER_MIN 60
#define PULSE_PER_MIN (PULSE_PER_SECOND * SEC_PER_MIN)
#define MIN_PER_HOUR 60
#define PULSE_PER_HOUR (PULSE_PER_MIN * MIN_PER_HOUR)
#define HOUR_PER_DAY 24
#define PULSE_PER_DAY (PULSE_PER_HOUR * HOUR_PER_DAY)

#define USEC_PER_PULSE (1000000 / PULSE_PER_SECOND)


#define USEC_TOTAL( val ) ( ( (val)->tv_sec * 1000000) + (val)->tv_usec )


static struct 
{
    int const threshold;
    unsigned long count;
} threshold_info [] =
{
    /* Must be in ascending order */
    {10,    0},
    {30,    0},
    {50,    0},
    {70,    0},
    {90,    0},
    {100,   0},
    {250,   0},
    {500,   0},
    {1000,  0},
    {2500,  0}
};


static time_t init_time;

static double last_pulse;
static double max_pulse = 0;


class PerfIntvlData
{
public:
    explicit PerfIntvlData( size_t size, PerfIntvlData *next )
        : mSize( size )
        , mInd( 0 )
        , mCount( 0 )
        , mAvgs( size )
        , mMins( size )
        , mMaxes( size )
        , mpNextIntvl( next )
    {

    }

    void AddData(double avg, double min, double max);
    double GetAvgAvg() const;
    double GetMinMin() const;
    double GetMaxMax() const;

    size_t GetCount() const { return mCount; }

private:
    PerfIntvlData( const PerfIntvlData & ); // unimplemented
    PerfIntvlData & operator=( const PerfIntvlData & ); // unimplemented

    size_t mSize;
    size_t mInd;
    size_t mCount;

    std::vector<double> mAvgs;
    std::vector<double> mMins;
    std::vector<double> mMaxes;

    PerfIntvlData *mpNextIntvl;
};

static PerfIntvlData sHourData( HOUR_PER_DAY, NULL );
static PerfIntvlData sMinuteData( MIN_PER_HOUR, &sHourData );
static PerfIntvlData sSecData( SEC_PER_MIN, &sMinuteData );
static PerfIntvlData sPulseData( PULSE_PER_SECOND, &sSecData );


void 
PerfIntvlData::AddData(double avg, double min, double max)
{
    mAvgs[mInd] = avg;
    mMins[mInd] = min;
    mMaxes[mInd] = max;

    if ( mCount <= mInd )
    {
        mCount = mInd + 1;
    }

    if ( mInd < (mSize - 1) )
    {
        mInd += 1;
    }
    else
    {
        mInd = 0;

        if ( mpNextIntvl )
        {
            mpNextIntvl->AddData(
                this->GetAvgAvg(),
                this->GetMinMin(),
                this->GetMaxMax());
        }
    }
}

double
PerfIntvlData::GetAvgAvg() const
{
    double sum = 0;

    for ( size_t i = 0 ; i < mCount ; ++i)
    {
        sum += mAvgs[i];
    }

    return sum / mCount;
}

double
PerfIntvlData::GetMinMin() const
{
    double min = INFINITY;

    for ( size_t i = 0 ; i < mCount ; ++i )
    {
        if ( mMins[i] < min )
        {
            min = mMins[i];
        }
    }

    return min;
}

double
PerfIntvlData::GetMaxMax() const
{
    double max = 0;

    for ( size_t i = 0 ; i < mCount ; ++i )
    {
        if ( mMaxes[i] > max )
        {
            max = mMaxes[i];
        }
    }

    return max;   
}

static void check_init( void )
{
    static bool init_done = false;
    if (init_done)
        return;
    
    init_time = time(NULL);

    init_done = true;
}

static void check_thresholds(double val)
{
    const unsigned int thresh_count = sizeof(threshold_info) / sizeof(threshold_info[0]);

    unsigned int i;

    for (i=0; i < thresh_count; ++i)
    {
        if (val > threshold_info[i].threshold)
        {
            ++(threshold_info[i].count);
        }
        else
        {
            break; //  Array should be in ascending order
        }
    }
}

void PERF_log_pulse(double val)
{
    check_init();

    last_pulse = val;
    if (val > max_pulse)
        max_pulse = val;
    
    check_thresholds(val);
    
    sPulseData.AddData( val, val, val );
}

size_t PERF_repr( char *out_buf, size_t n )
{
    if (!out_buf)
    {
        return 0;
    }
    if (n < 1)
    {
        out_buf[0] = '\0';
        return 0;
    }

    std::ostringstream os;

    const unsigned int thresh_count = sizeof(threshold_info) / sizeof(threshold_info[0]);
    unsigned int i;
    time_t total_secs = time(NULL) - init_time;
    double total_pulses = total_secs * PULSE_PER_SECOND;

    double pulse_min = sPulseData.GetMinMin();
    pulse_min = (pulse_min == INFINITY) ? 0 : pulse_min;

    double sec_min = sSecData.GetMinMin();
    sec_min = (sec_min == INFINITY) ? 0 : sec_min;

    double min_min = sMinuteData.GetMinMin();
    min_min = (min_min == INFINITY) ? 0 : min_min;

    double hour_min = sHourData.GetMinMin();
    hour_min = (hour_min == INFINITY) ? 0 : hour_min;

    os << std::fixed << std::setprecision(2)
       << "                     Avg         Min         Max\n\r"
       << std::setw(3) << 1 << " Pulse:   " 
       << std::setw(10) << last_pulse << "% " 
       << std::setw(10) << last_pulse << "% " 
       << std::setw(10) << last_pulse << "%\n\r"

       << std::setw(3) << sPulseData.GetCount() << " Pulses:  " 
       << std::setw(10) << sPulseData.GetAvgAvg() << "% "
       << std::setw(10) << pulse_min << "% "
       << std::setw(10) << sPulseData.GetMaxMax() << "%\n\r"

       << std::setw(3) << sSecData.GetCount() << " Seconds: " << std::setw(10)
       << std::setw(10) << sSecData.GetAvgAvg() << "% "
       << std::setw(10) << sec_min << "% "
       << std::setw(10) << sSecData.GetMaxMax() << "%\n\r"

       << std::setw(3) << sMinuteData.GetCount() << " Minutes: " << std::setw(10)
       << std::setw(10) << sMinuteData.GetAvgAvg() << "% "
       << std::setw(10) << min_min << "% "
       << std::setw(10) << sMinuteData.GetMaxMax() << "%\n\r"

       << std::setw(3) << sHourData.GetCount() << " Hours:   " << std::setw(10)
       << std::setw(10) << sHourData.GetAvgAvg() << "% "
       << std::setw(10) << hour_min << "% "
       << std::setw(10) << sHourData.GetMaxMax() << "%\n\r"

       << "\n\rMax pulse:      " << max_pulse << "\n\r\n\r";
  

    for (i=0; i < thresh_count; ++i)
    {
        os << "Over " << std::setw(5)  
           << threshold_info[i].threshold << "%:      "
           << (threshold_info[i].count / total_pulses) << "% "
           << "(" << threshold_info[i].count << ")\n\r";
    }

    

    std::string str = os.str();
    size_t copied = str.copy( out_buf, n - 1 );
    out_buf[copied] = '\0';

    return copied;
}


class PERF_prof_sect
{
public:
    explicit PERF_prof_sect(const char *id)
        : mId( id )
        , mLastEnterTime( )
        , mPulseTotal( )
        , mPulseMax( )
        , mTotal( )
        , mMax( )
        , mPulseEnterCount( 0 )
        , mPulseExitCount( 0 )
        , mTotalEnterCount( 0 )
    {
        timerclear(&mLastEnterTime);
        timerclear(&mPulseTotal);
        timerclear(&mPulseMax);
        timerclear(&mTotal);
        timerclear(&mMax);
    }

    void PulseReset();
    inline void Enter();
    inline void Exit();

    std::string const & GetId() { return mId; }
    
    struct timeval const & GetPulseTotal() { return mPulseTotal; }
    struct timeval const & GetPulseMax() { return mPulseMax; }
    struct timeval const & GetTotal() { return mTotal; }
    struct timeval const & GetMax() { return mMax; }

    unsigned long int GetPulseEnterCount() { return mPulseEnterCount; }
    unsigned long int GetPulseExitCount() { return mPulseExitCount; }
    unsigned long int GetTotalEnterCount() { return mTotalEnterCount; }

private:
    PERF_prof_sect( const PERF_prof_sect & ); // unimplemented
    PERF_prof_sect & operator=( const PERF_prof_sect & ); // unimplemented

    std::string mId;
    struct timeval mLastEnterTime;
    struct timeval mPulseTotal;
    struct timeval mPulseMax;
    struct timeval mTotal;
    struct timeval mMax;
    unsigned long int mPulseEnterCount;
    unsigned long int mPulseExitCount;
    unsigned long int mTotalEnterCount;
};

class PerfProfMgr
{
public:
    PerfProfMgr()
        : mSections( )
    {

    };

    PERF_prof_sect *NewSection(const char *id);
    void ResetAll();
    size_t ReprPulse( char *out_buf, size_t n ) const { return ReprBase( out_buf, n, false, nullptr ); }
    size_t ReprTotal( char *out_buf, size_t n ) const { return ReprBase( out_buf, n, true,  nullptr ); }
    size_t ReprSect( char *out_buf, size_t n, const char *id ) const;

private:
    size_t ReprBase( char *out_buf, size_t n, bool isTotal, PERF_prof_sect *sect ) const;
    void SectOutput( std::ostream &os, PERF_prof_sect *sect, bool isTotal) const;

    std::map<std::string, PERF_prof_sect *> mSections;
};

size_t
PerfProfMgr::ReprSect(char *out_buf, size_t n, const char *id) const
{
    if (!out_buf)
    {
        return 0;
    }
    if (n < 1)
    {
        out_buf[0] = '\0';
        return 0;
    }

    auto &&it = mSections.find(id);
    if (it == mSections.end())
    {
        std::ostringstream os;
        os << "No such section '" << id << "'\n\r";
        std::string str = os.str();
        size_t copied = str.copy( out_buf, n - 1 );
        out_buf[copied] = '\0';

        return copied;
    }

    size_t copied = ReprBase(out_buf, n, false, it->second);
    copied += ReprBase(out_buf + copied, n - copied, true, it->second);
    return copied;
}

void
PerfProfMgr::SectOutput(std::ostream &os, PERF_prof_sect *sect, bool isTotal) const
{
    unsigned long int enterCount;
    long int usecTotal;
    long int usecMax;

    if (isTotal)
    {
        enterCount = sect->GetTotalEnterCount();
        usecTotal = USEC_TOTAL( &(sect->GetTotal()) );
        usecMax = USEC_TOTAL( &(sect->GetMax()) );
    }
    else
    {
        enterCount = sect->GetPulseEnterCount();
        usecTotal = USEC_TOTAL( &(sect->GetPulseTotal()) );
        usecMax = USEC_TOTAL( &(sect->GetPulseMax()) );
    }

    /* Only bother to print sections that were active this pulse */
    if ( enterCount < 1 )
    {
        return;
    }

    os << std::left 
       << std::setw(20) << sect->GetId() << "|" 
       << std::right 
       << std::setw(12) << enterCount << "|";
    if (!isTotal)
    {
       os << std::setw(12) << sect->GetPulseExitCount() << "|";
    }
    os << std::setw(12) << usecTotal << "|";

    if (isTotal)
    {
        time_t total_secs = time(NULL) - init_time;
        double sect_secs = static_cast<double>(usecTotal) / 1000000;

        os << std::setw(12-1) << ( 100 * sect_secs / total_secs) << "%|";
    }
    else
    {
        os << std::setw(12-1) << ( 100 * static_cast<double>(usecTotal) / USEC_PER_PULSE ) << "%|";
    }
    os << std::setw(20-1) << ( 100 * static_cast<double>(usecMax) / USEC_PER_PULSE ) << "%\n\r";
}

size_t
PerfProfMgr::ReprBase(char *out_buf, size_t n, bool isTotal, PERF_prof_sect *sect) const
{
    if (!out_buf)
    {
        return 0;
    }
    if (n < 1)
    {
        out_buf[0] = '\0';
        return 0;
    }

    std::ostringstream os;

    if (isTotal)
    {
        os << "Cumulative profiling info\n\r";
    }
    else
    {
        os << "Pulse profiling info\n\r";
    }

    os << std::setprecision(2) << std::fixed
       << "\n\r"
       << std::left 
       << std::setw(20) << "Section name" << "|"
       << std::right 
       << std::setw(12) << "Enter Count" << "|";
    if (!isTotal)
    {
        os << std::setw(12) << "Exit Count" << "|";
    }
    os << std::setw(12) << "usec total" << "|";

    if (isTotal)
    {
        os << std::setw(12) << "total %" << "|";
    }
    else
    {
        os << std::setw(12) << "pulse %" << "|";
    }
    os << std::setw(20) << "max pulse % (1 entry)" << "\n\r";
       
    os << std::setfill('-') << std::setw(80) << " " << std::setfill(' ') << "\n\r" ;

    if (sect)
    {
        SectOutput(os, sect, isTotal);
    }
    else
    {
        for ( auto &&entry : this->mSections )
        {
            SectOutput(os, entry.second, isTotal);
        }
    }

    std::string str = os.str();
    size_t copied = str.copy( out_buf, n - 1 );
    out_buf[copied] = '\0';

    return copied;
}

PERF_prof_sect * 
PerfProfMgr::NewSection(const char *id)
{
    PERF_prof_sect *ptr = new PERF_prof_sect( id );
    mSections[id] = ptr;
    // mSections.push_back( ptr );
    return ptr;
}

void
PerfProfMgr::ResetAll()
{
    for ( auto &&entry : this->mSections )
    {
        entry.second->PulseReset();
    }
}

void
PERF_prof_sect::PulseReset()
{
    mPulseEnterCount = 0;
    mPulseExitCount = 0;
    timerclear( &mPulseTotal );
    timerclear( &mPulseMax );
}

void
PERF_prof_sect::Enter()
{
    ++mPulseEnterCount;
    ++mTotalEnterCount;
    gettimeofday(&mLastEnterTime, NULL);
}

void
PERF_prof_sect::Exit()
{
    ++mPulseExitCount;
    struct timeval now;
    struct timeval diff;
    gettimeofday(&now, NULL);
 
    timersub(&now, &mLastEnterTime, &diff);   
    timeradd(&mPulseTotal, &diff, &mPulseTotal);
    timeradd(&mTotal, &diff, &mTotal);

    if ( timercmp(&diff, &mPulseMax, > ) )
    {
        mPulseMax = diff;
    }
    if ( timercmp(&diff, &mMax, > ) )
    {
        mMax = diff;
    }
}

static PerfProfMgr sProfMgr;

void PERF_prof_sect_init(PERF_prof_sect **ptr, const char *id)
{
    /* If already inited then do nothing */
    if (*ptr)
    {
        return;
    }

    *ptr = sProfMgr.NewSection(id);
}

void PERF_prof_sect_enter(PERF_prof_sect *ptr)
{
    ptr->Enter();
}

void PERF_prof_sect_exit(PERF_prof_sect *ptr)
{
    ptr->Exit();
}

void PERF_prof_reset( void )
{
    sProfMgr.ResetAll();
}

size_t PERF_prof_repr_pulse( char *out_buf, size_t n )
{
    return sProfMgr.ReprPulse(out_buf, n);
}

size_t PERF_prof_repr_total( char *out_buf, size_t n )
{
    return sProfMgr.ReprTotal(out_buf, n);
}

size_t PERF_prof_repr_sect( char *out_buf, size_t n, const char *id)
{
    return sProfMgr.ReprSect(out_buf, n, id);
}
