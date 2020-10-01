/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

ENTRYPOINT enum Result
entrypoint(Context ctx) {
    const uint8_t* params = parameters_get(ctx);

    uint8_t l4_proto = packet_transport_proto(ctx);

    struct Flow flow;
    packet_flow(ctx, &flow);

    /* Для любых пакетов UDP: */
    if (l4_proto == IP_PROTO_UDP) {
        /* Заносим в таблицу BPF: ключ --- IP отправителя, значение --- 3. */
        uint64_t value = 3;
        table_ex_put(
            ctx, &flow.src_ip, &flow.src_ip + 1, &value, &value + 1);

        /* Пишем первые 8 байтов параметров в качестве payload. */
        uint16_t length;
        uint8_t* payload = packet_transport_payload(ctx, &length);
        for (int i = 0; i < 8; i++) {
            payload[i] = params[i];
        }

        /* Отправляем пакет назад. */
        return RESULT_BACK;
    }

    /* Если IP отправителя нет в таблице BPF, сбрасываем пакет. */
    uint64_t value = 0;
    struct TableExResult result = table_ex_get(
            ctx, &flow.src_ip, &flow.src_ip + 1, &value, &value + 1);
    if (!result.found) {
        return RESULT_DROP;
    }

    /* Если по ключу = IP отправителя в таблице BPF записано значение 1,
     * сбрасываем пакет.
     */
    if (value == 1) {
        return RESULT_DROP;
    }

    /* Проспускаем любые пакеты, IP отправителя которых есть в таблице BPF. */
    return RESULT_PASS;
}

PROGRAM_DISPLAY_ID("IPv6 Example v0.1")
