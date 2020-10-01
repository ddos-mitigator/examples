/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

#define ROUTE_MAX 8

struct Route {
    uint32_t network;           /* big-endian */
    uint32_t mask;              /* big-endian */
    struct EtherAddr next_hop;  /* MAC address   */
};

/* Helper script assumes 2-byte padding in the end. */
STATIC_ASSERT(sizeof(struct Route) == 16);

struct Params {
    /* Action when no route is found, default 0 = RESULT_PASS. */
    uint32_t def_result;
    /* Default 0 means "apply def_result". */
    uint32_t route_count;
    /* Sorted most-specific first (i.e. default route is last if present). */
    struct Route routes[0];
};

ENTRYPOINT enum Result
entrypoint(Context ctx) {
    const struct Params* params;
    const struct IpHeader* ip;
    uint32_t i;

    if (packet_network_proto(ctx) != ETHER_TYPE_IP) {
        return RESULT_PASS;
    }

    params = parameters_get(ctx);
    ip = packet_network_header(ctx);

    UNROLL /* Required to pass verification, works for ROUTE_MAX <= 8. */
    for (i = 0; (i < params->route_count) && (i < ROUTE_MAX); i++) {
        const struct Route* route = &params->routes[i];
        if ((ip->ip_dst & route->mask) == route->network) {
            struct EtherHeader* ether = packet_ether_header(ctx);
            ether->ether_dhost = route->next_hop;
            return RESULT_PASS;
        }
    }

    return params->def_result;
}

PROGRAM_DISPLAY_ID("L2 Router Example v0.1")
