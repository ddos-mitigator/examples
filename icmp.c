/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

ENTRYPOINT enum Result
pong(Context ctx) {
    struct IcmpHeader* icmp;
    uint16_t length;

    if (packet_transport_proto(ctx) != IP_PROTO_ICMP) {
        return RESULT_PASS;
    }

    icmp = packet_transport_header(ctx);
    if (icmp->icmp_type != ICMP_ECHO) {
        return RESULT_DROP;
    }

    icmp->icmp_type = ICMP_ECHO_REPLY;
    packet_transport_payload(ctx, &length);
    set_packet_length(ctx, length);
    return RESULT_BACK;
}

PROGRAM_DISPLAY_ID("ICMP Responder v0.5")
