#include "zebra.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_table.h"
#include "bgpd/bgp_route.h"
#include "bgpd/bgp_attr.h"
#include "log.h"
#include "bgp_twamp_ipc.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

/* Global shared memory pointer */
static struct twamp_shm *shm = NULL;
static struct event *measurement_check_timer = NULL;
static uint32_t last_sequence = 0;

/* Forward declaration */
static void bgp_twamp_check_measurements(struct event *thread);
static int shm_fd = -1;

/* Initialize shared memory */
void bgp_twamp_init(struct bgp *bgp)
{
    fprintf(stderr, "*** bgp_twamp_init CALLED, enabled=%d\n", bgp->import_latency_cfg.enabled); fflush(stderr);
    pthread_mutexattr_t attr;
    
    if (!bgp->import_latency_cfg.enabled) {
        zlog_info("BGP TWAMP: Not enabled, skipping initialization");
        return;
    }
    
    /* Check if already initialized */
    if (shm != NULL) {
        zlog_info("BGP TWAMP: Already initialized, collecting next-hops");
        bgp_twamp_collect_nexthops(bgp);

	/* Start periodic measurement check timer */
	event_add_timer(bm->master, bgp_twamp_check_measurements, bgp, 5, &measurement_check_timer);
	fprintf(stderr, "*** TWAMP: Started measurement check timer\n"); fflush(stderr);
        return;
    }
    
    /* Create/open shared memory */
    shm_fd = shm_open(TWAMP_SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        zlog_err("BGP TWAMP: Failed to create shared memory: %s", 
                 strerror(errno));
        return;
    }
    
    /* Size it */
    if (ftruncate(shm_fd, sizeof(struct twamp_shm)) == -1) {
        zlog_err("BGP TWAMP: Failed to size shared memory: %s", 
                 strerror(errno));
        close(shm_fd);
        shm_fd = -1;
        return;
    }
    
    /* Map it */
    shm = mmap(NULL, sizeof(struct twamp_shm), 
               PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (shm == MAP_FAILED) {
        zlog_err("BGP TWAMP: Failed to map shared memory: %s", 
                 strerror(errno));
        close(shm_fd);
        shm_fd = -1;
        shm = NULL;
        return;
    }
    
    /* Initialize mutex for cross-process synchronization */
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    
    shm->nh_count = 0;
    shm->sequence = 0;
    shm->padding = 0;
    
    zlog_info("BGP TWAMP: Shared memory initialized at %s", TWAMP_SHM_NAME);
    
    /* Collect existing next-hops */
    bgp_twamp_collect_nexthops(bgp);

	/* Start periodic measurement check timer */
	event_add_timer(bm->master, bgp_twamp_check_measurements, bgp, 5, &measurement_check_timer);
	fprintf(stderr, "*** TWAMP: Started measurement check timer\n"); fflush(stderr);
}

/* Add next-hop to monitoring list */
void bgp_twamp_add_nexthop(struct in_addr *nh)
{
    int i;
    fprintf(stderr, "*** ADD_NEXTHOP: Adding peer, shm=%p\n", (void*)shm); fflush(stderr);
    
    if (!shm) {
        zlog_warn("BGP TWAMP: Shared memory not initialized");
        return;
    }
    
    pthread_mutex_lock(&shm->lock);
    
    /* Check if already exists */
    for (i = 0; i < (int)shm->nh_count; i++) {
        if (shm->nexthops[i].addr.s_addr == nh->s_addr) {
            shm->nexthops[i].active = 1;
            pthread_mutex_unlock(&shm->lock);
            return;
        }
    }
    
    /* Add new entry */
    if (shm->nh_count < MAX_NEXTHOPS) {
        shm->nexthops[shm->nh_count].addr.s_addr = nh->s_addr;
        shm->nexthops[shm->nh_count].active = 1;
        shm->nexthops[shm->nh_count].measured = 0;
        shm->nexthops[shm->nh_count].padding[0] = 0;
        shm->nexthops[shm->nh_count].padding[1] = 0;
        shm->nexthops[shm->nh_count].latency_ms = UINT32_MAX;  /* Max = not measured */
        shm->nexthops[shm->nh_count].last_updated = 0;
        shm->nh_count++;
        shm->sequence++;  /* Signal change to TWAMP */
        
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, nh, buf, sizeof(buf));
        zlog_info("BGP TWAMP: Added next-hop %s for monitoring", buf);
    } else {
        zlog_warn("BGP TWAMP: Max next-hops (%d) reached, cannot add more", 
                  MAX_NEXTHOPS);
    }
    
    pthread_mutex_unlock(&shm->lock);
}

/* Remove next-hop from monitoring */
void bgp_twamp_remove_nexthop(struct in_addr *nh)
{
    int i;
    
    if (!shm)
        return;
    
    pthread_mutex_lock(&shm->lock);
    
    for (i = 0; i < (int)shm->nh_count; i++) {
        if (shm->nexthops[i].addr.s_addr == nh->s_addr) {
            shm->nexthops[i].active = 0;  /* Mark inactive */
            shm->sequence++;
            
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, nh, buf, sizeof(buf));
            zlog_info("BGP TWAMP: Removed next-hop %s from monitoring", buf);
            break;
        }
    }
    
    pthread_mutex_unlock(&shm->lock);
}


/* Get latency for a next-hop */
uint32_t bgp_twamp_get_latency(struct in_addr *nh)
{
    int i;
    uint32_t latency;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, nh, buf, sizeof(buf));
    
    if (!shm) {
        fprintf(stderr, "*** GET_LATENCY: shm is NULL for %s\n", buf); fflush(stderr);
        return UINT32_MAX;
    }
    
    pthread_mutex_lock(&shm->lock);
    
    fprintf(stderr, "*** GET_LATENCY: shm=%p, shm->nh_count=%d\n", (void*)shm, shm ? shm->nh_count : -1); fflush(stderr);
    fprintf(stderr, "*** GET_LATENCY: Looking for %s, nh_count=%d\n", buf, shm->nh_count); fflush(stderr);
    
    for (i = 0; i < (int)shm->nh_count; i++) {
        char stored_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &shm->nexthops[i].addr, stored_ip, sizeof(stored_ip));
        fprintf(stderr, "*** RAW BYTES: addr=%08x lat=%u act=%u meas=%u\n", 
                ntohl(shm->nexthops[i].addr.s_addr), 
                shm->nexthops[i].latency_ms, 
                shm->nexthops[i].active, 
                shm->nexthops[i].measured); fflush(stderr);
        fprintf(stderr, "*** GET_LATENCY:   [%d] %s: latency=%u active=%d measured=%d\n", 
                i, stored_ip, shm->nexthops[i].latency_ms, 
                shm->nexthops[i].active, shm->nexthops[i].measured); 
        fflush(stderr);
        
        if (shm->nexthops[i].addr.s_addr == nh->s_addr && 
            shm->nexthops[i].measured &&
            shm->nexthops[i].active) {
            latency = shm->nexthops[i].latency_ms;
            fprintf(stderr, "*** GET_LATENCY: FOUND! Returning %u ms\n", latency); fflush(stderr);
            pthread_mutex_unlock(&shm->lock);
            return latency;
        }
    }
    
    fprintf(stderr, "*** GET_LATENCY: NOT FOUND, returning UINT32_MAX\n"); fflush(stderr);
    pthread_mutex_unlock(&shm->lock);
    return UINT32_MAX;
}
void bgp_twamp_collect_nexthops(struct bgp *bgp)
{
    struct peer *peer;
    struct listnode *node, *nnode;
    struct in_addr peer_ip;
    int count = 0;
    
    if (!bgp || !bgp->import_latency_cfg.enabled) {
        zlog_info("BGP TWAMP: Feature not enabled or BGP instance invalid");
        return;
    }
    
    if (!shm) {
        zlog_warn("BGP TWAMP: Shared memory not initialized");
        return;
    }
    
    zlog_info("BGP TWAMP: Starting peer IP collection from peer list");
    
    /* Iterate through all BGP peers */
    for (ALL_LIST_ELEMENTS(bgp->peer, node, nnode, peer)) {
        
        /* Skip if not IBGP */
        if (peer->sort != BGP_PEER_IBGP)
            continue;
            
        /* Skip if not established */
        if (peer->connection->status != Established)
            continue;
        
        /* Skip if no connection */
        if (!peer->connection)
            continue;
            
        /* Get peer IPv4 address */
        if (peer->connection->su.sa.sa_family == AF_INET) {
            peer_ip.s_addr = peer->connection->su.sin.sin_addr.s_addr;
            
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer_ip, buf, sizeof(buf));
            zlog_info("BGP TWAMP: Found IBGP peer %s", buf);
            
            /* Add to shared memory */
            bgp_twamp_add_nexthop(&peer_ip);
            count++;
        }
    }
    
    fprintf(stderr, "*** COLLECTED %d peers!\n", count); fflush(stderr);
    fprintf(stderr, "*** COLLECTED %d peers!\n", count); fflush(stderr);
    zlog_info("BGP TWAMP: Collected %d IBGP peer IPs", count);
}
    

/* Cleanup shared memory */
/* Check if measurements have been updated */
static void bgp_twamp_check_measurements(struct event *thread)
{
	struct bgp *bgp = EVENT_ARG(thread);
	
	if (!shm || !bgp->import_latency_cfg.enabled) {
		measurement_check_timer = NULL;
		return;
	}
	
	pthread_mutex_lock(&shm->lock);
	uint32_t current_seq = shm->sequence;
	pthread_mutex_unlock(&shm->lock);
	
	/* If sequence changed, measurements were updated */
	if (current_seq != last_sequence) {
		fprintf(stderr, "*** TWAMP: Measurements updated (seq %u -> %u), triggering BGP refresh\n", 
		        last_sequence, current_seq); fflush(stderr);
		last_sequence = current_seq;

		/* Trigger VPN route re-import to update weights */
		vpn_leak_postchange_all();
	}
	
	/* Re-schedule timer for next check (every 5 seconds) */
	event_add_timer(bm->master, bgp_twamp_check_measurements, bgp, 5, &measurement_check_timer);
}


void bgp_twamp_cleanup(void)
{
    /* Check if not initialized */
    if (!shm && shm_fd < 0) {
        zlog_info("BGP TWAMP: Nothing to cleanup");
        return;
    }
    
    if (shm) {
        pthread_mutex_destroy(&shm->lock);
        munmap(shm, sizeof(struct twamp_shm));
        shm = NULL;
    }
    
    if (shm_fd >= 0) {
        close(shm_fd);
        shm_unlink(TWAMP_SHM_NAME);
        shm_fd = -1;
    }
    
    zlog_info("BGP TWAMP: Cleaned up shared memory");
}