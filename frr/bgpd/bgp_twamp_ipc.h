
#ifndef _BGP_TWAMP_IPC_H
#define _BGP_TWAMP_IPC_H


#include <stdint.h>
#include <time.h>
#include <pthread.h>


#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include <netinet/in.h>
#include <arpa/inet.h>


#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TWAMP_SHM_NAME "/bgp_twamp_shm"
#define MAX_NEXTHOPS 1024


struct twamp_nexthop {
    struct in_addr addr;    
    uint32_t latency_ms;     
    uint8_t active;          
    uint8_t measured;        
    uint8_t padding[2];    
    time_t last_updated;     
};


struct twamp_shm {
    pthread_mutex_t lock;           
    uint32_t nh_count;                
    uint32_t sequence;                 
    uint32_t padding;                
    struct twamp_nexthop nexthops[MAX_NEXTHOPS];
};


struct bgp;


void bgp_twamp_init(struct bgp *bgp);
void bgp_twamp_add_nexthop(struct in_addr *nh);
void bgp_twamp_remove_nexthop(struct in_addr *nh);
uint32_t bgp_twamp_get_latency(struct in_addr *nh);
void bgp_twamp_cleanup(void);
void bgp_twamp_collect_nexthops(struct bgp *bgp);

#ifdef __cplusplus
}
#endif

#endif 