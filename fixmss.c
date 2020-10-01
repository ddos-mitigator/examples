/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

/* Комментарии к структуре файла см. в tcpopt.c. */

/* Значение, на которое необходимо заменять MSS.
 * Например, значение 800 = 0x320 -> 20 03.
 */
struct Params {
    uint16_t mss;
};

LOCAL void
fixup_mss(Context ctx, const struct Params* params, const uint8_t* data) {
    /* Перезаписываем MSS. */
    *(uint16_t*)data = params->mss;

    /* Сообщаем системе, что данные пакета изменились,
     * чтобы автоматически пересчитать контрольную сумму.
     */
    set_packet_mangled(ctx);
}

ENTRYPOINT enum Result
filter(Context ctx) {
    static const size_t MAX_OPTS = 6;

    const struct Params* params = parameters_get(ctx);
    const struct TcpHeader* tcp = packet_transport_header(ctx);
    const uint8_t* options = (const uint8_t*)tcp + sizeof(*tcp);
    size_t i, options_size, offset = 0;

    if (packet_transport_proto(ctx) != IP_PROTO_TCP) {
        return RESULT_PASS;
    }

    if (!(tcp->th_flags & TCP_FLAG_SYN)) {
        return RESULT_PASS;
    }

    options_size = (tcp->th_off * 4) - sizeof(*tcp);
    for (i = 0; i < MAX_OPTS; i++) {
        uint8_t type = options[offset + 0];
        uint8_t size = options[offset + 1];

        switch (type) {
        case TCP_OPT_NOP:
            offset++;
            continue;
        case TCP_OPT_EOL:
            return RESULT_PASS;
        case TCP_OPT_MAXSEG:
            fixup_mss(ctx, params, options + offset + 2);
            return RESULT_PASS;
        }

        offset += size;

        if (offset > options_size) {
            return RESULT_DROP;
        }
    }

    return RESULT_PASS;
}

PROGRAM_DISPLAY_ID("TCP MSS Fixup Example v0.1")
