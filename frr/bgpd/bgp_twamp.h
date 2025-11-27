#ifndef _BGP_TWAMP_H
#define _BGP_TWAMP_H

#include "bgp_twamp_ipc.h"


struct bgp;
struct in_addr;


extern void bgp_twamp_init(struct bgp *bgp);


extern void bgp_twamp_add_nexthop(struct in_addr *nh);


extern void bgp_twamp_remove_nexthop(struct in_addr *nh);


extern uint32_t bgp_twamp_get_latency(struct in_addr *nh);


extern void bgp_twamp_collect_nexthops(struct bgp *bgp);


extern void bgp_twamp_cleanup(void);

#endif 