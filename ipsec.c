/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

#define FLAG_INIT (1 << 0)
#define FLAG_AUTH (1 << 1)
#define FLAG_FULL (FLAG_INIT | FLAG_AUTH)

struct Settings {
    uint8_t init_required; /* IKE packet to port 500 required */
};

LOCAL enum Result
process_ike_port_500(Context ctx, TableKey key, TableValue flags) {
    if (!(flags & FLAG_INIT)) {
        table_put(ctx, key, FLAG_INIT);
    }
    return RESULT_PASS;
}

LOCAL enum Result
process_ike_port_4500(Context ctx, TableKey key, TableValue flags) {
    const struct Settings* settings = parameters_get(ctx);

    uint16_t length;
    const uint8_t* data = packet_transport_payload(ctx, &length);

    if (settings->init_required && !(flags & FLAG_INIT)) {
        /* packet to port 500 required first */
        return RESULT_DROP;
    }

    if (length < 22) {
        /* marker(4) + SPI(8) + SPI(8) + next(1) + version(1) */
        return RESULT_DROP;
    }

    if (data[0] || data[1] || data[2] || data[3]) {
        return RESULT_DROP; /* non-ESP marker */
    }

    if (data[21] != 0x20) {
        return RESULT_DROP; /* version 2.0 */
    }

    if (!(flags & FLAG_AUTH)) {
        table_put(ctx, key, flags | FLAG_AUTH);
    }
    return RESULT_PASS;
}

LOCAL enum Result
process_esp(Context ctx) {
    uint16_t length;
    const uint8_t* data = packet_transport_payload(ctx, &length);
    bool is_nat = (length == 1) && (data[0] == 0xFF);
    return (is_nat || (length >= 30)) ? RESULT_PASS : RESULT_DROP;
}

LOCAL enum Result
process_udp(Context ctx) {
    const struct Settings* settings = parameters_get(ctx);

    struct Flow flow;
    packet_flow(ctx, &flow);

    TableKey key = (TableKey)flow.src_ip.v4;
    uint16_t port = bswap16(flow.dst_port);

    struct TableRecord record;
    table_get(ctx, key, &record);
    TableValue flags = record.value;

    TableValue valid_flags = FLAG_AUTH;
    if (settings->init_required) {
        valid_flags |= FLAG_INIT;
    }

    if ((flags & valid_flags) == valid_flags) {
        return process_esp(ctx);
    }

    switch (port) {
    case 500:
        if (settings->init_required) {
            return process_ike_port_500(ctx, key, flags);
        }
        return RESULT_PASS;
    case 4500:
        return process_ike_port_4500(ctx, key, flags);
    default:
        return RESULT_DROP;
    }
}

ENTRYPOINT enum Result
ipsec(Context ctx) {
    if (packet_transport_proto(ctx) != IP_PROTO_UDP) {
        return RESULT_PASS;
    }
    return process_udp(ctx);
}

PROGRAM_DISPLAY_ID("IPsec (v4: NAT keepalive support) v0.1")
