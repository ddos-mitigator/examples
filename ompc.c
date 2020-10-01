/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>
 */
#include <mitigator_bpf.h>

/* Размер маски и паттерна. */
typedef uint32_t Value;

/* Например, нужно воспроизвести (ip[30] == 0x41) and (ip[32] == 0x42).
 * Смещение 30 (hex 1E), маска FF 00 FF 00 (hex), паттерн 41 00 42 00 (hex).
 * Параметры:
 *
 * 1e 00 00 00
 * FF 00 FF 00
 * 41 00 42 00
 */
struct Params {
    uint32_t offset;
    Value mask;
    Value pattern;
};

ENTRYPOINT enum Result
filter(Context ctx) {
    const struct Params* params;
    const uint8_t* ip;
    const uint8_t* payload;
    uint16_t payload_length;
    uint16_t total_length;
    Value value;

    params = parameters_get(ctx);

    /* У нас нет функции, чтобы получить указатель на данные от начала IP
     * и их длину, поэтому вычисляем длину через указатель на IP, указатель
     * на L4 payload и длину L4 payload.
     */
    ip = packet_network_header(ctx);
    payload = packet_transport_payload(ctx, &payload_length);
    total_length = (payload - ip) + payload_length;

    /* Доказываем верификатору, что смещение не может быть за пределами
     * самого большого возможного пакета с учетом размера значения.
     */
    if (params->offset >= MAX_PAYLOAD_LENGTH - sizeof(value)) {
        return RESULT_PASS;
    }

    /* Проверяем, что смещение не за пределами конкретного пакета. */
    if (params->offset >= total_length) {
        return RESULT_PASS;
    }

    /* Целевая проверка. */
    value = *(const Value*)(ip + params->offset);
    if ((value & params->mask) == params->pattern) {
        return RESULT_DROP;
    }

    return RESULT_PASS;
}

PROGRAM_DISPLAY_ID("Offset-Mask-Pattern Criteria (32 bit) v0.1")
