/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

/* Параметры программы, в данном случае, минимальный MSS.
 * Например, значение 800 = 0x320 -> 20 03.
 */
struct Params {
    uint16_t mss_min;
};

/* Проверка, что MSS не менее заданного.
 * Возвращает true, если принято решение по пакету, иначе false.
 * Решение записывается в выходной параметр result.
 */
LOCAL Bool
check_option(const struct Params* params, uint8_t type, const uint8_t* data,
        enum Result *result) {
    if (type != TCP_OPT_MAXSEG) {
        return false;
    }

    uint16_t mss = bswap16(*(uint16_t*)data);
    *result = (mss < params->mss_min) ? RESULT_DROP : RESULT_PASS;
    return true;
}

ENTRYPOINT enum Result
filter(Context ctx) {
    static const size_t MAX_OPTS = 6;

    const struct Params* params = parameters_get(ctx);
    const struct TcpHeader* tcp = packet_transport_header(ctx);
    const uint8_t* options = (const uint8_t*)tcp + sizeof(*tcp);
    size_t i, options_size, offset = 0;
    enum Result result;

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
            /* Пустая опция для выравнивания. */
            offset++;
            continue;
        case TCP_OPT_EOL:
            /* Нормальный конец списка опций. */
            return RESULT_PASS;
        default:
            /* Специфичная для фильтра обработка значимых опций. */
            if (check_option(params, type, options + offset + 2, &result)) {
                return result;
            }
        }

        offset += size;

        /* Некорректная длина опции, сбрасываем такой пакет. */
        if (offset > options_size) {
            return RESULT_DROP;
        }
    }

    return RESULT_PASS;
}

PROGRAM_DISPLAY_ID("TCP Option Filter Template v0.2")
