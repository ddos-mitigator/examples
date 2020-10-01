/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

LOCAL enum Result
respond(Context ctx, uint32_t rx, uint32_t tx, uint8_t flags) {
    struct TcpHeader* tcp = packet_transport_header(ctx);
    uint32_t seq = bswap32(tcp->th_seq);
    tcp->th_seq = tcp->th_ack;
    tcp->th_ack = bswap32(seq + rx);
    tcp->th_flags = flags;

    set_packet_length(ctx, tx);

    return RESULT_BACK;
}

LOCAL enum Result
process_syn(Context ctx) {
    set_packet_syncookie(ctx);
    return RESULT_BACK;
}

LOCAL uint32_t
write_http_302(uint8_t* data) {
    static const char response[] =
        "HTTP/1.0 302 Found\r\n"
        "Location: example.com/tutorial.html\r\n"
        "\r\n";

    uint32_t tx = sizeof(response) - 1;
    memcpy(data, response, tx);
    return tx;
}

LOCAL enum Result
process_ack(Context ctx) {
    uint16_t rx = 0;
    uint8_t* data = packet_transport_payload(ctx, &rx);

    if (rx == 0) {
        return RESULT_DROP;
    }

    if (syncookie_check(ctx, 0, 0)) {
        uint32_t tx = write_http_302(data);
        return respond(ctx, rx, tx, TCP_FLAG_ACK | TCP_FLAG_PUSH | TCP_FLAG_FIN);
    }

    return respond(ctx, rx, 0, TCP_FLAG_ACK);
}

LOCAL enum Result
process_fin(Context ctx) {
    uint16_t rx = 0;
    packet_transport_payload(ctx, &rx);
    return respond(ctx, rx, 0, TCP_FLAG_ACK);
}

LOCAL enum Result
process_tcp(Context ctx) {
    struct TcpHeader* tcp = packet_transport_header(ctx);

    switch (tcp->th_flags & (TCP_FLAG_SYN | TCP_FLAG_ACK | TCP_FLAG_FIN)) {
    case TCP_FLAG_SYN:
        return process_syn(ctx);
    case TCP_FLAG_ACK:
        return process_ack(ctx);
    case TCP_FLAG_FIN | TCP_FLAG_ACK:
        return process_fin(ctx);
    }

    return RESULT_DROP;
}

ENTRYPOINT enum Result
http(Context ctx) {
    if (packet_transport_proto(ctx) != IP_PROTO_TCP) {
        return RESULT_PASS;
    }

    return process_tcp(ctx);
}

PROGRAM_DISPLAY_ID("HTTP Redirect Example v0.1")
