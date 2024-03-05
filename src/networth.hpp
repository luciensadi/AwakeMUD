#ifndef __networth_hpp__
#define __networth_hpp__

long get_net_worth(struct char_data *ch, int mode);

#define NETWORTH_MODE_ALL        (1 << 0)
#define NETWORTH_MODE_LIQUID     (1 << 1)
#define NETWORTH_MODE_VEHICLES   (1 << 2)
#define NETWORTH_MODE_LEASES     (1 << 3)
#define NETWORTH_MODE_APARTMENTS (1 << 4)
#define NETWORTH_MODE_ON_PC      (1 << 5)
#define NETWORTH_MODE_SKILLS_ETC (1 << 6)
#define NUM_NETWORTH_BITS 7

#endif // __networth_hpp__