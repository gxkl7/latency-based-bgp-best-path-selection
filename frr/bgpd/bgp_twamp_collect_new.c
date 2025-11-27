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
        if (peer->status != Established)
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
    
    zlog_info("BGP TWAMP: Collected %d IBGP peer IPs", count);
}
